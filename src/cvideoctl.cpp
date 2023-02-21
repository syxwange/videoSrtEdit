#include "cvideoctl.h"
#include <QMutex>


extern QMutex g_show_rect_mutex;

static int framedrop = -1;
static int infinite_buffer = -1;
static int64_t audio_callback_time;

#define FF_QUIT_EVENT    (SDL_USEREVENT + 2)


/* prepare a new audio buffer */
void sdlAudioCallback(void* opaque, Uint8* stream, int len)
{
    VideoState* is = (VideoState*)opaque;
    int audio_size, len1;
    const auto & pVideoCtl = CVideoCtl::getInstance();
    audio_callback_time = av_gettime_relative();

    while (len > 0) {
        if (is->audio_buf_index >= is->audio_buf_size) {
            audio_size = pVideoCtl.audioDecodeFrame(is);
            if (audio_size < 0) {
                /* if error, just output silence */
                is->audio_buf = NULL;
                is->audio_buf_size = SDL_AUDIO_MIN_BUFFER_SIZE / is->audio_tgt.frame_size * is->audio_tgt.frame_size;
            }
            else {
                is->audio_buf_size = audio_size;
            }
            is->audio_buf_index = 0;
        }
        len1 = is->audio_buf_size - is->audio_buf_index;
        if (len1 > len)
            len1 = len;
        if (is->audio_buf && is->audio_volume == SDL_MIX_MAXVOLUME)
            memcpy(stream, (uint8_t*)is->audio_buf + is->audio_buf_index, len1);
        else {
            memset(stream, 0, len1);
            if (is->audio_buf)
                SDL_MixAudio(stream, (uint8_t*)is->audio_buf + is->audio_buf_index, len1, is->audio_volume);
        }
        len -= len1;
        stream += len1;
        is->audio_buf_index += len1;
    }
    is->audio_write_buf_size = is->audio_buf_size - is->audio_buf_index;
    /* Let's assume the audio driver that is used by SDL has two periods. */
    if (!std::isnan(is->audio_clock)) {
        pVideoCtl.setClockAt(&is->audclk, is->audio_clock - (double)(2 * is->audio_hw_buf_size + is->audio_write_buf_size) / is->audio_tgt.bytes_per_sec, is->audio_clock_serial, audio_callback_time / 1000000.0);
        pVideoCtl.syncClockToSlave(&is->extclk, &is->audclk);
    }
}

int decodeInterruptCb(void* ctx)
{
    VideoState* is = (VideoState*)ctx;
    return is->abort_request;
}


bool CVideoCtl::init()
{
    if (bInited_ == true)
    {
        return true;
    }

    connect(this, &CVideoCtl::SigStop, &CVideoCtl::onStop);
  
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
    {
        av_log(NULL, AV_LOG_FATAL, "Could not initialize SDL - %s\n", SDL_GetError());
        av_log(NULL, AV_LOG_FATAL, "(Did you set the DISPLAY variable?)\n");
        return false;
    }
    SDL_EventState(SDL_SYSWMEVENT, SDL_IGNORE);
    SDL_EventState(SDL_USEREVENT, SDL_IGNORE);
    bInited_ = true;
    return true;
}

VideoState* CVideoCtl::streamOpen(const char* filename)
{
    VideoState *is;
    //构造视频状态类
    is = (VideoState *)av_mallocz(sizeof(VideoState));
    if (!is)
        return nullptr;
    //视频文件名
    is->last_video_stream = is->video_stream = -1;
    is->last_audio_stream = is->audio_stream = -1;
    is->last_subtitle_stream = is->subtitle_stream = -1;
    is->filename = av_strdup(filename);
    if (!is->filename)
        goto fail;
    //指定输入格式
    is->ytop = 0;
    is->xleft = 0;

    /* start video display */
    //初始化视频帧队列
    if (frame_queue_init(&is->pictq, &is->videoq, VIDEO_PICTURE_QUEUE_SIZE, 1) < 0)
        goto fail;
    //初始化字幕帧队列
    if (frame_queue_init(&is->subpq, &is->subtitleq, SUBPICTURE_QUEUE_SIZE, 0) < 0)
        goto fail;
    //初始化音频帧队列
    if (frame_queue_init(&is->sampq, &is->audioq, SAMPLE_QUEUE_SIZE, 1) < 0)
        goto fail;
    //初始化队列中的数据包
    if (packet_queue_init(&is->videoq) < 0 ||
        packet_queue_init(&is->audioq) < 0 ||
        packet_queue_init(&is->subtitleq) < 0)
        goto fail;
    //构建 继续读取线程 信号量
    if (!(is->continue_read_thread = SDL_CreateCond())) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateCond(): %s\n", SDL_GetError());
        goto fail;
    }
    //视频、音频 时钟
    initClock(&is->vidclk, &is->videoq.serial);
    initClock(&is->audclk, &is->audioq.serial);
    initClock(&is->extclk, &is->extclk.serial);
    is->audio_clock_serial = -1;
    //音量
    if (nStartupVolume_ < 0)
        av_log(NULL, AV_LOG_WARNING, "-volume=%d < 0, setting to 0\n", nStartupVolume_);
    if (nStartupVolume_ > 100)
        av_log(NULL, AV_LOG_WARNING, "-volume=%d > 100, setting to 100\n", nStartupVolume_);
    nStartupVolume_ = av_clip(nStartupVolume_, 0, 100);
    nStartupVolume_ = av_clip(SDL_MIX_MAXVOLUME * nStartupVolume_ / 100, 0, SDL_MIX_MAXVOLUME);
    is->audio_volume = nStartupVolume_;
    
    emit SigVideoVolume(nStartupVolume_ * 1.0 / SDL_MIX_MAXVOLUME);
    emit SigPauseStat(is->paused);

    is->av_sync_type = AV_SYNC_AUDIO_MASTER;
    //构建读取线程
    is->read_tid = std::thread(&CVideoCtl::readThread, this, is);

    return is;
fail:
    streamClose(is);
    return NULL;
}

void CVideoCtl::streamClose(VideoState* is)
{
    /* XXX: use a special url_shutdown call to abort parse cleanly */
    is->abort_request = 1;
    is->read_tid.join();

    /* close each stream */
    if (is->audio_stream >= 0)
        streamComponentClose(is, is->audio_stream);
    if (is->video_stream >= 0)
        streamComponentClose(is, is->video_stream);
    if (is->subtitle_stream >= 0)
        streamComponentClose(is, is->subtitle_stream);

    avformat_close_input(&is->ic);

    packet_queue_destroy(&is->videoq);
    packet_queue_destroy(&is->audioq);
    packet_queue_destroy(&is->subtitleq);

    /* free all pictures */
    frame_queue_destory(&is->pictq);
    frame_queue_destory(&is->sampq);
    frame_queue_destory(&is->subpq);
    SDL_DestroyCond(is->continue_read_thread);
    sws_freeContext(is->img_convert_ctx);
    sws_freeContext(is->sub_convert_ctx);
    av_free(is->filename);

    if (is->vid_texture)
        SDL_DestroyTexture(is->vid_texture);
    if (is->sub_texture)
        SDL_DestroyTexture(is->sub_texture);
    av_free(is);
}

void CVideoCtl::streamComponentClose(VideoState* is, int stream_index)
{
    AVFormatContext* ic = is->ic;
    AVCodecParameters* codecpar;

    if (stream_index < 0 || stream_index >= ic->nb_streams)
        return;
    codecpar = ic->streams[stream_index]->codecpar;

    switch (codecpar->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        decoder_abort(&is->auddec, &is->sampq);
        SDL_CloseAudio();
        decoder_destroy(&is->auddec);
        swr_free(&is->swr_ctx);
        av_freep(&is->audio_buf1);
        is->audio_buf1_size = 0;
        is->audio_buf = NULL;

        if (is->rdft) {
            av_rdft_end(is->rdft);
            av_freep(&is->rdft_data);
            is->rdft = NULL;
            is->rdft_bits = 0;
        }
        break;
    case AVMEDIA_TYPE_VIDEO:
        decoder_abort(&is->viddec, &is->pictq);
        decoder_destroy(&is->viddec);
        break;
    case AVMEDIA_TYPE_SUBTITLE:
        decoder_abort(&is->subdec, &is->subpq);
        decoder_destroy(&is->subdec);
        break;
    default:
        break;
    }

    ic->streams[stream_index]->discard = AVDISCARD_ALL;
    switch (codecpar->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        is->audio_st = NULL;
        is->audio_stream = -1;
        break;
    case AVMEDIA_TYPE_VIDEO:
        is->video_st = NULL;
        is->video_stream = -1;
        break;
    case AVMEDIA_TYPE_SUBTITLE:
        is->subtitle_st = NULL;
        is->subtitle_stream = -1;
        break;
    default:
        break;
    }
}

void CVideoCtl::streamTogglePause(VideoState* is) const
{
    if (is->paused) {
        is->frame_timer += av_gettime_relative() / 1000000.0 - is->vidclk.last_updated;
        if (is->read_pause_return != AVERROR(ENOSYS)) {
            is->vidclk.paused = 0;
        }
        setClock(&is->vidclk, getClock(&is->vidclk), is->vidclk.serial);
    }
    setClock(&is->extclk, getClock(&is->extclk), is->extclk.serial);
    is->paused = is->audclk.paused = is->vidclk.paused = is->extclk.paused = !is->paused;
}

void CVideoCtl::streamCycleChannel(VideoState* is, int codec_type)
{
    AVFormatContext* ic = is->ic;
    int start_index, stream_index;
    int old_index;
    AVStream* st;
    AVProgram* p = NULL;
    int nb_streams = is->ic->nb_streams;

    if (codec_type == AVMEDIA_TYPE_VIDEO) {
        start_index = is->last_video_stream;
        old_index = is->video_stream;
    }
    else if (codec_type == AVMEDIA_TYPE_AUDIO) {
        start_index = is->last_audio_stream;
        old_index = is->audio_stream;
    }
    else {
        start_index = is->last_subtitle_stream;
        old_index = is->subtitle_stream;
    }
    stream_index = start_index;

    if (codec_type != AVMEDIA_TYPE_VIDEO && is->video_stream != -1) {
        p = av_find_program_from_stream(ic, NULL, is->video_stream);
        if (p) {
            nb_streams = p->nb_stream_indexes;
            for (start_index = 0; start_index < nb_streams; start_index++)
                if (p->stream_index[start_index] == stream_index)
                    break;
            if (start_index == nb_streams)
                start_index = -1;
            stream_index = start_index;
        }
    }

    for (;;) {
        if (++stream_index >= nb_streams)
        {
            if (codec_type == AVMEDIA_TYPE_SUBTITLE)
            {
                stream_index = -1;
                is->last_subtitle_stream = -1;
                goto the_end;
            }
            if (start_index == -1)
                return;
            stream_index = 0;
        }
        if (stream_index == start_index)
            return;
        st = is->ic->streams[p ? p->stream_index[stream_index] : stream_index];
        if (st->codecpar->codec_type == codec_type) {
            /* check that parameters are OK */
            switch (codec_type) {
            case AVMEDIA_TYPE_AUDIO:
                if (st->codecpar->sample_rate != 0 &&
                    st->codecpar->channels != 0)
                    goto the_end;
                break;
            case AVMEDIA_TYPE_VIDEO:
            case AVMEDIA_TYPE_SUBTITLE:
                goto the_end;
            default:
                break;
            }
        }
    }
the_end:
    if (p && stream_index != -1)
        stream_index = p->stream_index[stream_index];
    av_log(NULL, AV_LOG_INFO, "Switch %s stream from #%d to #%d\n",
        av_get_media_type_string((AVMediaType)codec_type),
        old_index,
        stream_index);

    streamComponentClose(is, old_index);
    streamComponentOpen(is, stream_index);
}

int CVideoCtl::streamHasEnoughPackets(AVStream* st, int stream_id, PacketQueue* queue) const
{
    return stream_id < 0 ||queue->abort_request ||(st->disposition & AV_DISPOSITION_ATTACHED_PIC) ||
        queue->nb_packets > MIN_FRAMES && (!queue->duration || av_q2d(st->time_base) * queue->duration > 1.0);

}

void CVideoCtl::streamSeek(VideoState* is, int64_t pos, int64_t rel) const
{
    if (!is->seek_req) {
        is->seek_pos = pos;
        is->seek_rel = rel;
        is->seek_flags &= ~AVSEEK_FLAG_BYTE;
        is->seek_req = 1;
        SDL_CondSignal(is->continue_read_thread);
    }
}

void CVideoCtl::videoImageDisplay(VideoState* is)
{
    Frame* vp;
    Frame* sp = NULL;
    SDL_Rect rect;

    vp = frame_queue_peek_last(&is->pictq);
    if (is->subtitle_st) {
        if (frame_queue_nb_remaining(&is->subpq) > 0) {
            sp = frame_queue_peek(&is->subpq);

            if (vp->pts >= sp->pts + ((float)sp->sub.start_display_time / 1000)) {
                if (!sp->uploaded) {
                    uint8_t* pixels[4];
                    int pitch[4];
                    int i;
                    if (!sp->width || !sp->height) {
                        sp->width = vp->width;
                        sp->height = vp->height;
                    }
                    if (reallocTexture(&is->sub_texture, SDL_PIXELFORMAT_ARGB8888, sp->width, sp->height, SDL_BLENDMODE_BLEND, 1) < 0)
                        return;

                    for (i = 0; i < sp->sub.num_rects; i++) {
                        AVSubtitleRect* sub_rect = sp->sub.rects[i];

                        sub_rect->x = av_clip(sub_rect->x, 0, sp->width);
                        sub_rect->y = av_clip(sub_rect->y, 0, sp->height);
                        sub_rect->w = av_clip(sub_rect->w, 0, sp->width - sub_rect->x);
                        sub_rect->h = av_clip(sub_rect->h, 0, sp->height - sub_rect->y);

                        is->sub_convert_ctx = sws_getCachedContext(is->sub_convert_ctx,
                            sub_rect->w, sub_rect->h, AV_PIX_FMT_PAL8,
                            sub_rect->w, sub_rect->h, AV_PIX_FMT_BGRA,
                            0, NULL, NULL, NULL);
                        if (!is->sub_convert_ctx) {
                            av_log(NULL, AV_LOG_FATAL, "Cannot initialize the conversion context\n");
                            return;
                        }
                        if (!SDL_LockTexture(is->sub_texture, (SDL_Rect*)sub_rect, (void**)pixels, pitch)) {
                            sws_scale(is->sub_convert_ctx, (const uint8_t* const*)sub_rect->data, sub_rect->linesize,
                                0, sub_rect->h, pixels, pitch);
                            SDL_UnlockTexture(is->sub_texture);
                        }
                    }
                    sp->uploaded = 1;
                }
            }
            else
                sp = NULL;
        }
    }

    calculateDisplayRect(&rect, is->xleft, is->ytop, is->width, is->height, vp->width, vp->height, vp->sar);

    if (!vp->uploaded) {
        int sdl_pix_fmt = vp->frame->format == AV_PIX_FMT_YUV420P ? SDL_PIXELFORMAT_YV12 : SDL_PIXELFORMAT_ARGB8888;
        if (reallocTexture(&is->vid_texture, sdl_pix_fmt, vp->frame->width, vp->frame->height, SDL_BLENDMODE_NONE, 0) < 0)
            return;
        if (uploadTexture(is->vid_texture, vp->frame, &is->img_convert_ctx) < 0)
            return;
        vp->uploaded = 1;
        vp->flip_v = vp->frame->linesize[0] < 0;

        //通知宽高变化
        if (nFrameW_ != vp->frame->width || nFrameH_ != vp->frame->height)
        {
            nFrameW_ = vp->frame->width;
            nFrameH_ = vp->frame->height;
            emit SigFrameDimensionsChanged(nFrameW_, nFrameH_);
        }
    }

    SDL_RenderCopyEx(renderer_, is->vid_texture, NULL, &rect, 0, NULL, (SDL_RendererFlip)(vp->flip_v ? SDL_FLIP_VERTICAL : 0));
    if (sp) {
        SDL_RenderCopy(renderer_, is->sub_texture, NULL, &rect);
    }
}

int CVideoCtl::videoOpen(VideoState* is)
{
    int w, h;

    w = nScreenWidth_;
    h = nScreenHeight_;

    if (!window_) {
        int flags = SDL_WINDOW_SHOWN;
        flags |= SDL_WINDOW_RESIZABLE;

        window_ = SDL_CreateWindowFrom((void*)playWid_);
        SDL_GetWindowSize(window_, &w, &h);//初始宽高设置为显示控件宽高
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
        if (window_) {
            SDL_RendererInfo info;
            if (!renderer_)
                renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
            if (!renderer_) {
                av_log(NULL, AV_LOG_WARNING, "Failed to initialize a hardware accelerated renderer: %s\n", SDL_GetError());
                renderer_ = SDL_CreateRenderer(window_, -1, 0);
            }
            if (renderer_) {
                if (!SDL_GetRendererInfo(renderer_, &info))
                    av_log(NULL, AV_LOG_VERBOSE, "Initialized %s renderer.\n", info.name);
            }
        }
    }
    else {
        SDL_SetWindowSize(window_, w, h);
    }

    if (!window_ || !renderer_) {
        av_log(NULL, AV_LOG_FATAL, "SDL: could not set video mode - exiting\n");
        doExit(is);
    }

    is->width = w;
    is->height = h;

    return 0;
}

void CVideoCtl::videoDisplay(VideoState* is) 
{
    if (!window_)
        videoOpen(is);
    if (renderer_)
    {
        //恰好显示控件大小在变化，则不刷新显示
        if (g_show_rect_mutex.tryLock())
        {
            SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
            SDL_RenderClear(renderer_);
            videoImageDisplay(is);
            SDL_RenderPresent(renderer_);

            g_show_rect_mutex.unlock();
        }
    }
}

int CVideoCtl::subtitleThread(void* arg) const
{
    VideoState* is = (VideoState*)arg;
    Frame* sp;
    int got_subtitle;
    double pts;

    for (;;) {
        if (!(sp = frame_queue_peek_writable(&is->subpq)))
            return 0;

        if ((got_subtitle = decoder_decode_frame(&is->subdec, NULL, &sp->sub)) < 0)
            break;

        pts = 0;

        if (got_subtitle && sp->sub.format == 0) {
            if (sp->sub.pts != AV_NOPTS_VALUE)
                pts = sp->sub.pts / (double)AV_TIME_BASE;
            sp->pts = pts;
            sp->serial = is->subdec.pkt_serial;
            sp->width = is->subdec.avctx->width;
            sp->height = is->subdec.avctx->height;
            sp->uploaded = 0;

            /* now we can update the picture count */
            frame_queue_push(&is->subpq);
        }
        else if (got_subtitle) {
            avsubtitle_free(&sp->sub);
        }
    }
    return 0;
}

int CVideoCtl::audioThread(void* arg) const
{
    VideoState* is = (VideoState*)arg;
    AVFrame* frame = av_frame_alloc();
    Frame* af;

    int got_frame = 0;
    AVRational tb;
    int ret = 0;

    if (!frame)
        return AVERROR(ENOMEM);

    do {
        if ((got_frame = decoder_decode_frame(&is->auddec, frame, NULL)) < 0)
            goto the_end;

        if (got_frame) {
            tb = { 1, frame->sample_rate };

            if (!(af = frame_queue_peek_writable(&is->sampq)))
                goto the_end;

            af->pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);
            af->pos = frame->pkt_pos;
            af->serial = is->auddec.pkt_serial;
            af->duration = av_q2d({ frame->nb_samples, frame->sample_rate });

            av_frame_move_ref(af->frame, frame);
            frame_queue_push(&is->sampq);

        }
    } while (ret >= 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF);
the_end:

    av_frame_free(&frame);
    return ret;
}

int CVideoCtl::videoThread(void* arg) const
{
    VideoState* is = (VideoState*)arg;
    AVFrame* frame = av_frame_alloc();
    double pts;
    double duration;
    int ret;
    AVRational tb = is->video_st->time_base;
    AVRational frame_rate = av_guess_frame_rate(is->ic, is->video_st, NULL);

    if (!frame)
    {
        return AVERROR(ENOMEM);
    }

    //循环从队列中获取视频帧
    for (;;) {
        ret = getVideoFrame(is, frame);
        if (ret < 0)
            goto the_end;
        if (!ret)
            continue;

        duration = (frame_rate.num && frame_rate.den ? av_q2d({ frame_rate.den, frame_rate.num }) : 0);
        pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);
        ret = queuePicture(is, frame, pts, duration, frame->pkt_pos, is->viddec.pkt_serial);
        av_frame_unref(frame);

        if (ret < 0)
            goto the_end;
    }
the_end:

    av_frame_free(&frame);
    return 0;
}

int CVideoCtl::streamComponentOpen(VideoState* is, int stream_index)
{
    AVFormatContext* ic = is->ic;
    AVCodecContext* avctx;
    const AVCodec* codec;
    const char* forced_codec_name = NULL;
    AVDictionary* opts = NULL;
    const AVDictionaryEntry* t = NULL;
    int sample_rate;
    AVChannelLayout ch_layout;
    memset(&ch_layout, 0, sizeof(AVChannelLayout));
    int ret = 0;
    int stream_lowres = 0;

    if (stream_index < 0 || stream_index >= ic->nb_streams)
        return -1;

    avctx = avcodec_alloc_context3(NULL);
    if (!avctx)
        return AVERROR(ENOMEM);

    ret = avcodec_parameters_to_context(avctx, ic->streams[stream_index]->codecpar);
    if (ret < 0)
        goto fail;
    avctx->pkt_timebase = ic->streams[stream_index]->time_base;

    codec = avcodec_find_decoder(avctx->codec_id);

    switch (avctx->codec_type) {
    case AVMEDIA_TYPE_AUDIO: is->last_audio_stream = stream_index; break;
    case AVMEDIA_TYPE_SUBTITLE: is->last_subtitle_stream = stream_index; break;
    case AVMEDIA_TYPE_VIDEO: is->last_video_stream = stream_index; break;
    }
    if (forced_codec_name)
        codec = avcodec_find_decoder_by_name(forced_codec_name);
    if (!codec) {
        if (forced_codec_name) av_log(NULL, AV_LOG_WARNING,
            "No codec could be found with name '%s'\n", forced_codec_name);
        else                   av_log(NULL, AV_LOG_WARNING,
            "No decoder could be found for codec %s\n", avcodec_get_name(avctx->codec_id));
        ret = AVERROR(EINVAL);
        goto fail;
    }

    avctx->codec_id = codec->id;
    if (stream_lowres > codec->max_lowres) {
        av_log(avctx, AV_LOG_WARNING, "The maximum value for lowres supported by the decoder is %d\n",
            codec->max_lowres);
        stream_lowres = codec->max_lowres;
    }
    avctx->lowres = stream_lowres;

    opts = nullptr /*filter_codec_opts(codec_opts, avctx->codec_id, ic, ic->streams[stream_index], codec)*/;
    if (!av_dict_get(opts, "threads", NULL, 0))
        av_dict_set(&opts, "threads", "auto", 0);
    if (stream_lowres)
        av_dict_set_int(&opts, "lowres", stream_lowres, 0);
    if ((ret = avcodec_open2(avctx, codec, &opts)) < 0) {
        goto fail;
    }
    if ((t = av_dict_get(opts, "", NULL, AV_DICT_IGNORE_SUFFIX))) {
        av_log(NULL, AV_LOG_ERROR, "Option %s not found.\n", t->key);
        ret = AVERROR_OPTION_NOT_FOUND;
        goto fail;
    }

    is->eof = 0;
    ic->streams[stream_index]->discard = AVDISCARD_DEFAULT;
    switch (avctx->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
#if CONFIG_AVFILTER
    {
        AVFilterContext* sink;

        is->audio_filter_src.freq = avctx->sample_rate;
        ret = av_channel_layout_copy(&is->audio_filter_src.ch_layout, &avctx->ch_layout);
        if (ret < 0)
            goto fail;
        is->audio_filter_src.fmt = avctx->sample_fmt;
        if ((ret = configure_audio_filters(is, afilters, 0)) < 0)
            goto fail;
        sink = is->out_audio_filter;
        sample_rate = av_buffersink_get_sample_rate(sink);
        ret = av_buffersink_get_ch_layout(sink, &ch_layout);
        if (ret < 0)
            goto fail;
    }
#else
        sample_rate = avctx->sample_rate;
        ret = av_channel_layout_copy(&ch_layout, &avctx->ch_layout);
        if (ret < 0)
            goto fail;
#endif

        /* prepare audio output */
        if ((ret = audioOpen(is, &ch_layout, sample_rate, &is->audio_tgt)) < 0)
            goto fail;
        is->audio_hw_buf_size = ret;
        is->audio_src = is->audio_tgt;
        is->audio_buf_size = 0;
        is->audio_buf_index = 0;

        /* init averaging filter */
        is->audio_diff_avg_coef = exp(log(0.01) / AUDIO_DIFF_AVG_NB);
        is->audio_diff_avg_count = 0;
        /* since we do not have a precise anough audio FIFO fullness,
           we correct audio sync only if larger than this threshold */
        is->audio_diff_threshold = (double)(is->audio_hw_buf_size) / is->audio_tgt.bytes_per_sec;

        is->audio_stream = stream_index;
        is->audio_st = ic->streams[stream_index];

        if ((ret = decoder_init(&is->auddec, avctx, &is->audioq, is->continue_read_thread)) < 0)
            goto fail;
        if ((is->ic->iformat->flags & (AVFMT_NOBINSEARCH | AVFMT_NOGENSEARCH | AVFMT_NO_BYTE_SEEK)) && !is->ic->iformat->read_seek) {
            is->auddec.start_pts = is->audio_st->start_time;
            is->auddec.start_pts_tb = is->audio_st->time_base;
        }

        packet_queue_start(is->auddec.queue);
        is->auddec.decode_thread = std::thread(&CVideoCtl::audioThread, this, is);

        SDL_PauseAudioDevice(audioDev_, 0);
        break;
    case AVMEDIA_TYPE_VIDEO:
        is->video_stream = stream_index;
        is->video_st = ic->streams[stream_index];

        if ((ret = decoder_init(&is->viddec, avctx, &is->videoq, is->continue_read_thread)) < 0)
            goto fail;
        packet_queue_start(is->viddec.queue);
        is->viddec.decode_thread = std::thread(&CVideoCtl::videoThread, this, is);
        is->queue_attachments_req = 1;
        break;
    case AVMEDIA_TYPE_SUBTITLE:
        is->subtitle_stream = stream_index;
        is->subtitle_st = ic->streams[stream_index];

        if ((ret = decoder_init(&is->subdec, avctx, &is->subtitleq, is->continue_read_thread)) < 0)
            goto fail;
        packet_queue_start(is->subdec.queue);
        is->subdec.decode_thread = std::thread(&CVideoCtl::subtitleThread, this, is);
        break;
    default:
        break;
    }
    goto out;

fail:
    avcodec_free_context(&avctx);
out:
    av_channel_layout_uninit(&ch_layout);
    av_dict_free(&opts);

    return ret;
}

double CVideoCtl::getMasterClock(VideoState* is) const
{
    double val;

    switch (getMasterSyncType(is)) {
    case AV_SYNC_VIDEO_MASTER:
        val = getClock(&is->vidclk);
        break;
    case AV_SYNC_AUDIO_MASTER:
        val = getClock(&is->audclk);
        break;
    default:
        val = getClock(&is->extclk);
        break;
    }
    return val;
}

double CVideoCtl::getClock(Clock* c) const
{
    if (*c->queue_serial != c->serial)
        return NAN;
    if (c->paused) {
        return c->pts;
    }
    else {
        double time = av_gettime_relative() / 1000000.0;
        return c->pts_drift + time - (time - c->last_updated) * (1.0 - c->speed);
    }
}

void CVideoCtl::doExit(VideoState* is)
{
    if (is)
    {
        streamClose(is);
        is = nullptr;
    }
    if (renderer_)
    {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (window_)
    {
        window_ = nullptr;
    }
    emit SigStopFinished();
}

int CVideoCtl::uploadTexture(SDL_Texture* tex, AVFrame* frame, SwsContext** img_convert_ctx) const
{
    int ret = 0;
    switch (frame->format) {
    case AV_PIX_FMT_YUV420P:
        if (frame->linesize[0] < 0 || frame->linesize[1] < 0 || frame->linesize[2] < 0) {
            av_log(NULL, AV_LOG_ERROR, "Negative linesize is not supported for YUV.\n");
            return -1;
        }
        ret = SDL_UpdateYUVTexture(tex, NULL, frame->data[0], frame->linesize[0],
            frame->data[1], frame->linesize[1],
            frame->data[2], frame->linesize[2]);
        break;
    case AV_PIX_FMT_BGRA:
        if (frame->linesize[0] < 0) {
            ret = SDL_UpdateTexture(tex, NULL, frame->data[0] + frame->linesize[0] * (frame->height - 1), -frame->linesize[0]);
        }
        else {
            ret = SDL_UpdateTexture(tex, NULL, frame->data[0], frame->linesize[0]);
        }
        break;
    default:
        /* This should only happen if we are not using avfilter... */
        *img_convert_ctx = sws_getCachedContext(*img_convert_ctx,
            frame->width, frame->height, (AVPixelFormat)frame->format, frame->width, frame->height,
            AV_PIX_FMT_BGRA, SWS_BICUBIC, NULL, NULL, NULL);
        if (*img_convert_ctx != NULL) {
            uint8_t* pixels[4];
            int pitch[4];
            if (!SDL_LockTexture(tex, NULL, (void**)pixels, pitch)) {
                sws_scale(*img_convert_ctx, (const uint8_t* const*)frame->data, frame->linesize,
                    0, frame->height, pixels, pitch);
                SDL_UnlockTexture(tex);
            }
        }
        else {
            av_log(NULL, AV_LOG_FATAL, "Cannot initialize the conversion context\n");
            ret = -1;
        }
        break;
    }
    return ret;
}

int CVideoCtl::reallocTexture(SDL_Texture** texture, Uint32 new_format, int new_width, int new_height, SDL_BlendMode blendmode, int init_texture) const
{
    Uint32 format;
    int access, w, h;
    if (SDL_QueryTexture(*texture, &format, &access, &w, &h) < 0 || new_width != w || new_height != h || new_format != format) {
        void* pixels;
        int pitch;
        SDL_DestroyTexture(*texture);
        if (!(*texture = SDL_CreateTexture(renderer_, new_format, SDL_TEXTUREACCESS_STREAMING, new_width, new_height)))
            return -1;
        if (SDL_SetTextureBlendMode(*texture, blendmode) < 0)
            return -1;
        if (init_texture) {
            if (SDL_LockTexture(*texture, NULL, &pixels, &pitch) < 0)
                return -1;
            memset(pixels, 0, pitch * new_height);
            SDL_UnlockTexture(*texture);
        }
    }
    return 0;
}

void CVideoCtl::calculateDisplayRect(SDL_Rect* rect, int scr_xleft, int scr_ytop, int scr_width, int scr_height, int pic_width, int pic_height, AVRational pic_sar) const
{
    float aspect_ratio;
    int width, height, x, y;

    if (pic_sar.num == 0)
        aspect_ratio = 0;
    else
        aspect_ratio = av_q2d(pic_sar);

    if (aspect_ratio <= 0.0)
        aspect_ratio = 1.0;
    aspect_ratio *= (float)pic_width / (float)pic_height;

    /* XXX: we suppose the screen has a 1.0 pixel ratio */
    height = scr_height;
    width = lrint(height * aspect_ratio) & ~1;
    if (width > scr_width) {
        width = scr_width;
        height = lrint(width / aspect_ratio) & ~1;
    }
    x = (scr_width - width) / 2;
    y = (scr_height - height) / 2;
    rect->x = scr_xleft + x;
    rect->y = scr_ytop + y;
    rect->w = FFMAX(width, 1);
    rect->h = FFMAX(height, 1);
}

int CVideoCtl::isRealtime(AVFormatContext* s) const
{
    if (!strcmp(s->iformat->name, "rtp")|| !strcmp(s->iformat->name, "rtsp")|| !strcmp(s->iformat->name, "sdp"))
        return 1;
    if (s->pb && (!strncmp(s->url, "rtp:", 4)|| !strncmp(s->url, "udp:", 4)))
        return 1;
    return 0;
}

int CVideoCtl::getMasterSyncType(VideoState* is) const
{
    if (is->av_sync_type == AV_SYNC_VIDEO_MASTER) {
        if (is->video_st)
            return AV_SYNC_VIDEO_MASTER;
        else
            return AV_SYNC_AUDIO_MASTER;
    }
    else if (is->av_sync_type == AV_SYNC_AUDIO_MASTER) {
        if (is->audio_st)
            return AV_SYNC_AUDIO_MASTER;
        else
            return AV_SYNC_EXTERNAL_CLOCK;
    }
    else {
        return AV_SYNC_EXTERNAL_CLOCK;
    }
}

double CVideoCtl::vpDuration(VideoState* is, Frame* vp, Frame* nextvp) const
{
    if (vp->serial == nextvp->serial) {
        double duration = nextvp->pts - vp->pts;
        if (std::isnan(duration) || duration <= 0 || duration > is->max_frame_duration)
            return vp->duration;
        else
            return duration;
    }
    else {
        return 0.0;
    }
}

double CVideoCtl::computeTargetDelay(double delay, VideoState* is) const
{
    double sync_threshold, diff = 0;

    /* update delay to follow master synchronisation source */
    if (getMasterSyncType(is) != AV_SYNC_VIDEO_MASTER) {
        /* if video is slave, we try to correct big delays by
        duplicating or deleting a frame */
        diff = getClock(&is->vidclk) - getMasterClock(is);

        /* skip or repeat frame. We take into account the
        delay to compute the threshold. I still don't know
        if it is the best guess */
        sync_threshold = FFMAX(AV_SYNC_THRESHOLD_MIN, FFMIN(AV_SYNC_THRESHOLD_MAX, delay));
        if (!std::isnan(diff) && fabs(diff) < is->max_frame_duration) {
            if (diff <= -sync_threshold)
                delay = FFMAX(0, delay + diff);
            else if (diff >= sync_threshold && delay > AV_SYNC_FRAMEDUP_THRESHOLD)
                delay = delay + diff;
            else if (diff >= sync_threshold)
                delay = 2 * delay;
        }
    }

    av_log(NULL, AV_LOG_TRACE, "video: delay=%0.3f A-V=%f\n",
        delay, -diff);

    return delay;
}

void CVideoCtl::updateVideoPts(VideoState* is, double pts, int64_t pos, int serial) const
{
    setClock(&is->vidclk, pts, serial);
    syncClockToSlave(&is->extclk, &is->vidclk);
}

void CVideoCtl::togglePause(VideoState* is)
{
    streamTogglePause(is);
    is->step = 0;
}

void CVideoCtl::updateVolume(int sign, double step) 
{
    if (pCurStream_ == nullptr)
    {
        return;
    }
    double volume_level = pCurStream_->audio_volume ? (20 * log(pCurStream_->audio_volume / (double)SDL_MIX_MAXVOLUME) / log(10)) : -1000.0;
    int new_volume = lrint(SDL_MIX_MAXVOLUME * pow(10.0, (volume_level + sign * step) / 20.0));
    pCurStream_->audio_volume = av_clip(pCurStream_->audio_volume == new_volume ? (pCurStream_->audio_volume + sign) : new_volume, 0, SDL_MIX_MAXVOLUME);

    emit SigVideoVolume(pCurStream_->audio_volume * 1.0 / SDL_MIX_MAXVOLUME);
}

void CVideoCtl::refreshLoopWaitEvent(VideoState* is, SDL_Event* event) 
{
    double remaining_time = 0.0;
    SDL_PumpEvents();
    while (!SDL_PeepEvents(event, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT) && bPlayLoop_)
    {
        if (remaining_time > 0.0)
            av_usleep((int64_t)(remaining_time * 1000000.0));
        remaining_time = REFRESH_RATE;
        if (!is->paused || is->force_refresh)
            videoRefresh(is, &remaining_time);
        SDL_PumpEvents();
    }
}

void CVideoCtl::stepToNextFrame(VideoState* is) const
{
    /* if the stream is paused unpause it, then step */
    if (is->paused)
        streamTogglePause(is);
    is->step = 1;
}

int CVideoCtl::getVideoFrame(VideoState* is, AVFrame* frame) const
{
    int got_picture;

    if ((got_picture = decoder_decode_frame(&is->viddec, frame, NULL)) < 0)
        return -1;

    if (got_picture) {
        double dpts = NAN;

        if (frame->pts != AV_NOPTS_VALUE)
            dpts = av_q2d(is->video_st->time_base) * frame->pts;

        frame->sample_aspect_ratio = av_guess_sample_aspect_ratio(is->ic, is->video_st, frame);

        if (framedrop > 0 || (framedrop && getMasterSyncType(is) != AV_SYNC_VIDEO_MASTER)) {
            if (frame->pts != AV_NOPTS_VALUE) {
                double diff = dpts - getMasterClock(is);
                if (!isnan(diff) && fabs(diff) < AV_NOSYNC_THRESHOLD &&
                    diff - is->frame_last_filter_delay < 0 &&
                    is->viddec.pkt_serial == is->vidclk.serial &&
                    is->videoq.nb_packets) {
                    is->frame_drops_early++;
                    av_frame_unref(frame);
                    got_picture = 0;
                }
            }
        }
    }

    return got_picture;
}

int CVideoCtl::queuePicture(VideoState* is, AVFrame* src_frame, double pts, double duration, int64_t pos, int serial) const
{
    Frame* vp;

    if (!(vp = frame_queue_peek_writable(&is->pictq)))
        return -1;

    vp->sar = src_frame->sample_aspect_ratio;
    vp->uploaded = 0;

    vp->width = src_frame->width;
    vp->height = src_frame->height;
    vp->format = src_frame->format;

    vp->pts = pts;
    vp->duration = duration;
    vp->pos = pos;
    vp->serial = serial;

    av_frame_move_ref(vp->frame, src_frame);
    frame_queue_push(&is->pictq);
    return 0;
}

int CVideoCtl::synchronizeAudio(VideoState* is, int nb_samples) const
{
    int wanted_nb_samples = nb_samples;

    /* if not master, then we try to remove or add samples to correct the clock */
    if (getMasterSyncType(is) != AV_SYNC_AUDIO_MASTER) {
        double diff, avg_diff;
        int min_nb_samples, max_nb_samples;

        diff = getClock(&is->audclk) - getMasterClock(is);

        if (!std::isnan(diff) && fabs(diff) < AV_NOSYNC_THRESHOLD) {
            is->audio_diff_cum = diff + is->audio_diff_avg_coef * is->audio_diff_cum;
            if (is->audio_diff_avg_count < AUDIO_DIFF_AVG_NB) {
                /* not enough measures to have a correct estimate */
                is->audio_diff_avg_count++;
            }
            else {
                /* estimate the A-V difference */
                avg_diff = is->audio_diff_cum * (1.0 - is->audio_diff_avg_coef);

                if (fabs(avg_diff) >= is->audio_diff_threshold) {
                    wanted_nb_samples = nb_samples + (int)(diff * is->audio_src.freq);
                    min_nb_samples = ((nb_samples * (100 - SAMPLE_CORRECTION_PERCENT_MAX) / 100));
                    max_nb_samples = ((nb_samples * (100 + SAMPLE_CORRECTION_PERCENT_MAX) / 100));
                    wanted_nb_samples = av_clip(wanted_nb_samples, min_nb_samples, max_nb_samples);
                }
                av_log(NULL, AV_LOG_TRACE, "diff=%f adiff=%f sample_diff=%d apts=%0.3f %f\n",
                    diff, avg_diff, wanted_nb_samples - nb_samples,
                    is->audio_clock, is->audio_diff_threshold);
            }
        }
        else {
            /* too big difference : may be initial PTS errors, so
            reset A-V filter */
            is->audio_diff_avg_count = 0;
            is->audio_diff_cum = 0;
        }
    }

    return wanted_nb_samples;
}

int CVideoCtl::audioOpen(void* opaque, AVChannelLayout* wanted_channel_layout, int wanted_sample_rate, AudioParams* audio_hw_params)
{
    SDL_AudioSpec wanted_spec, spec;
    const char* env;
    static const int next_nb_channels[] = { 0, 0, 1, 6, 2, 6, 4, 6 };
    static const int next_sample_rates[] = { 0, 44100, 48000, 96000, 192000 };
    int next_sample_rate_idx = FF_ARRAY_ELEMS(next_sample_rates) - 1;
    int wanted_nb_channels = wanted_channel_layout->nb_channels;

    env = SDL_getenv("SDL_AUDIO_CHANNELS");
    if (env) {
        wanted_nb_channels = atoi(env);
        av_channel_layout_uninit(wanted_channel_layout);
        av_channel_layout_default(wanted_channel_layout, wanted_nb_channels);
    }
    if (wanted_channel_layout->order != AV_CHANNEL_ORDER_NATIVE) {
        av_channel_layout_uninit(wanted_channel_layout);
        av_channel_layout_default(wanted_channel_layout, wanted_nb_channels);
    }
    wanted_nb_channels = wanted_channel_layout->nb_channels;
    wanted_spec.channels = wanted_nb_channels;
    wanted_spec.freq = wanted_sample_rate;
    if (wanted_spec.freq <= 0 || wanted_spec.channels <= 0) {
        av_log(NULL, AV_LOG_ERROR, "Invalid sample rate or channel count!\n");
        return -1;
    }
    while (next_sample_rate_idx && next_sample_rates[next_sample_rate_idx] >= wanted_spec.freq)
        next_sample_rate_idx--;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.silence = 0;
    wanted_spec.samples = FFMAX(SDL_AUDIO_MIN_BUFFER_SIZE, 2 << av_log2(wanted_spec.freq / SDL_AUDIO_MAX_CALLBACKS_PER_SEC));
    wanted_spec.callback = sdlAudioCallback;
    wanted_spec.userdata = opaque;
    while (!(audioDev_ = SDL_OpenAudioDevice(NULL, 0, &wanted_spec, &spec, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE))) {
        av_log(NULL, AV_LOG_WARNING, "SDL_OpenAudio (%d channels, %d Hz): %s\n",
            wanted_spec.channels, wanted_spec.freq, SDL_GetError());
        wanted_spec.channels = next_nb_channels[FFMIN(7, wanted_spec.channels)];
        if (!wanted_spec.channels) {
            wanted_spec.freq = next_sample_rates[next_sample_rate_idx--];
            wanted_spec.channels = wanted_nb_channels;
            if (!wanted_spec.freq) {
                av_log(NULL, AV_LOG_ERROR,
                    "No more combinations to try, audio open failed\n");
                return -1;
            }
        }
        av_channel_layout_default(wanted_channel_layout, wanted_spec.channels);
    }
    if (spec.format != AUDIO_S16SYS) {
        av_log(NULL, AV_LOG_ERROR,
            "SDL advised audio format %d is not supported!\n", spec.format);
        return -1;
    }
    if (spec.channels != wanted_spec.channels) {
        av_channel_layout_uninit(wanted_channel_layout);
        av_channel_layout_default(wanted_channel_layout, spec.channels);
        if (wanted_channel_layout->order != AV_CHANNEL_ORDER_NATIVE) {
            av_log(NULL, AV_LOG_ERROR,
                "SDL advised channel count %d is not supported!\n", spec.channels);
            return -1;
        }
    }
    audio_hw_params->fmt = AV_SAMPLE_FMT_S16;
    audio_hw_params->freq = spec.freq;
    if (av_channel_layout_copy(&audio_hw_params->ch_layout, wanted_channel_layout) < 0)
        return -1;
    audio_hw_params->frame_size = av_samples_get_buffer_size(NULL, audio_hw_params->ch_layout.nb_channels, 1, audio_hw_params->fmt, 1);
    audio_hw_params->bytes_per_sec = av_samples_get_buffer_size(NULL, audio_hw_params->ch_layout.nb_channels, audio_hw_params->freq, audio_hw_params->fmt, 1);
    if (audio_hw_params->bytes_per_sec <= 0 || audio_hw_params->frame_size <= 0) {
        av_log(NULL, AV_LOG_ERROR, "av_samples_get_buffer_size failed\n");
        return -1;
    }
    return spec.size;
}

void CVideoCtl::videoRefresh(void* opaque, double* remaining_time)
{
    VideoState* is = (VideoState*)opaque;
    double time;

    Frame* sp, * sp2;

    double rdftspeed = 0.02;

    if (!is->paused && getMasterSyncType(is) == AV_SYNC_EXTERNAL_CLOCK && is->realtime)
        checkExternalClockSpeed(is);

    if (is->video_st) {
    retry:
        if (frame_queue_nb_remaining(&is->pictq) == 0) {
            // nothing to do, no picture to display in the queue
        }
        else {
            double last_duration, duration, delay;
            Frame* vp, * lastvp;

            /* dequeue the picture */
            lastvp = frame_queue_peek_last(&is->pictq);
            vp = frame_queue_peek(&is->pictq);

            if (vp->serial != is->videoq.serial) {
                frame_queue_next(&is->pictq);
                goto retry;
            }

            if (lastvp->serial != vp->serial)
                is->frame_timer = av_gettime_relative() / 1000000.0;

            if (is->paused)
                goto display;

            /* compute nominal last_duration */
            last_duration = vpDuration(is, lastvp, vp);
            delay = computeTargetDelay(last_duration, is);

            time = av_gettime_relative() / 1000000.0;
            if (time < is->frame_timer + delay) {
                *remaining_time = FFMIN(is->frame_timer + delay - time, *remaining_time);
                goto display;
            }

            is->frame_timer += delay;
            if (delay > 0 && time - is->frame_timer > AV_SYNC_THRESHOLD_MAX)
                is->frame_timer = time;

            SDL_LockMutex(is->pictq.mutex);
            if (!std::isnan(vp->pts))
                updateVideoPts(is, vp->pts, vp->pos, vp->serial);
            SDL_UnlockMutex(is->pictq.mutex);

            if (frame_queue_nb_remaining(&is->pictq) > 1) {
                Frame* nextvp = frame_queue_peek_next(&is->pictq);
                duration = vpDuration(is, vp, nextvp);
                if (!is->step && (framedrop > 0 || (framedrop && getMasterSyncType(is) != AV_SYNC_VIDEO_MASTER)) && time > is->frame_timer + duration) {
                    is->frame_drops_late++;
                    frame_queue_next(&is->pictq);
                    goto retry;
                }
            }

            if (is->subtitle_st) {
                while (frame_queue_nb_remaining(&is->subpq) > 0) {
                    sp = frame_queue_peek(&is->subpq);

                    if (frame_queue_nb_remaining(&is->subpq) > 1)
                        sp2 = frame_queue_peek_next(&is->subpq);
                    else
                        sp2 = NULL;

                    if (sp->serial != is->subtitleq.serial
                        || (is->vidclk.pts > (sp->pts + ((float)sp->sub.end_display_time / 1000)))
                        || (sp2 && is->vidclk.pts > (sp2->pts + ((float)sp2->sub.start_display_time / 1000))))
                    {
                        if (sp->uploaded) {
                            int i;
                            for (i = 0; i < sp->sub.num_rects; i++) {
                                AVSubtitleRect* sub_rect = sp->sub.rects[i];
                                uint8_t* pixels;
                                int pitch, j;

                                if (!SDL_LockTexture(is->sub_texture, (SDL_Rect*)sub_rect, (void**)&pixels, &pitch)) {
                                    for (j = 0; j < sub_rect->h; j++, pixels += pitch)
                                        memset(pixels, 0, sub_rect->w << 2);
                                    SDL_UnlockTexture(is->sub_texture);
                                }
                            }
                        }
                        frame_queue_next(&is->subpq);
                    }
                    else {
                        break;
                    }
                }
            }

            frame_queue_next(&is->pictq);
            is->force_refresh = 1;

            if (is->step && !is->paused)
                streamTogglePause(is);
        }
    display:
        /* display picture */
        if (is->force_refresh && is->pictq.rindex_shown)
            videoDisplay(is);
    }
    is->force_refresh = 0;

    emit SigVideoPlaySeconds(getMasterClock(is));
}

void CVideoCtl::setClock(Clock* c, double pts, int serial) const
{
    double time = av_gettime_relative() / 1000000.0;
    setClockAt(c, pts, serial, time);
}

void CVideoCtl::initClock(Clock* c, int* queue_serial) const
{
    c->speed = 1.0;
    c->paused = 0;
    c->queue_serial = queue_serial;
    setClock(c, NAN, -1);
}

void CVideoCtl::checkExternalClockSpeed(VideoState* is) const
{
    if (is->video_stream >= 0 && is->videoq.nb_packets <= EXTERNAL_CLOCK_MIN_FRAMES ||
        is->audio_stream >= 0 && is->audioq.nb_packets <= EXTERNAL_CLOCK_MIN_FRAMES) {
        setClockSpeed(&is->extclk, FFMAX(EXTERNAL_CLOCK_SPEED_MIN, is->extclk.speed - EXTERNAL_CLOCK_SPEED_STEP));
    }
    else if ((is->video_stream < 0 || is->videoq.nb_packets > EXTERNAL_CLOCK_MAX_FRAMES) &&
        (is->audio_stream < 0 || is->audioq.nb_packets > EXTERNAL_CLOCK_MAX_FRAMES)) {
        setClockSpeed(&is->extclk, FFMIN(EXTERNAL_CLOCK_SPEED_MAX, is->extclk.speed + EXTERNAL_CLOCK_SPEED_STEP));
    }
    else {
        double speed = is->extclk.speed;
        if (speed != 1.0)
            setClockSpeed(&is->extclk, speed + EXTERNAL_CLOCK_SPEED_STEP * (1.0 - speed) / fabs(1.0 - speed));
    }
}

void CVideoCtl::setClockSpeed(Clock* c, double speed) const
{
    setClock(c, getClock(c), c->serial);
    c->speed = speed;
}

void CVideoCtl::readThread(VideoState* is)
{
    //VideoState *is = (VideoState *)arg;
    AVFormatContext* ic = NULL;
    int err, i, ret;
    int st_index[AVMEDIA_TYPE_NB];
    AVPacket* pkt = NULL;
    int64_t stream_start_time;
    int pkt_in_play_range = 0;
    const AVDictionaryEntry* t;
    AVDictionary** opts = nullptr;
    int orig_nb_streams = 0;
    SDL_mutex* wait_mutex = SDL_CreateMutex();
    int scan_all_pmts_set = 0;
    int64_t pkt_ts;

    const char* wanted_stream_spec[AVMEDIA_TYPE_NB] = { 0 };

    if (!wait_mutex) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateMutex(): %s\n", SDL_GetError());
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    memset(st_index, -1, sizeof(st_index));
    is->eof = 0;


    pkt = av_packet_alloc();
    if (!pkt) {
        av_log(NULL, AV_LOG_FATAL, "Could not allocate packet.\n");
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    //构建 处理封装格式 结构体
    ic = avformat_alloc_context();
    if (!ic) {
        av_log(NULL, AV_LOG_FATAL, "Could not allocate context.\n");
        ret = AVERROR(ENOMEM);
        goto fail;
    }
    ic->interrupt_callback.callback = decodeInterruptCb;
    ic->interrupt_callback.opaque = is;

    //打开文件，获得封装等信息

    err = avformat_open_input(&ic, is->filename, nullptr, nullptr);
    if (err < 0) {
        //print_error(is->filename, err);
        ret = -1;
        goto fail;
    }

    is->ic = ic;


    av_format_inject_global_side_data(ic);


    orig_nb_streams = ic->nb_streams;
    //读取一部分视音频数据并且获得一些相关的信息
    err = avformat_find_stream_info(ic, opts);

    //     for (i = 0; i < orig_nb_streams; i++)
    //         av_dict_free(&opts[i]);
    //     av_freep(&opts);

    if (err < 0) {
        av_log(NULL, AV_LOG_WARNING,
            "%s: could not find codec parameters\n", is->filename);
        ret = -1;
        goto fail;
    }

    if (ic->pb)
        ic->pb->eof_reached = 0; // FIXME hack, ffplay maybe should not use avio_feof() to test for the end

    is->max_frame_duration = (ic->iformat->flags & AVFMT_TS_DISCONT) ? 10.0 : 3600.0;

    is->realtime = isRealtime(ic);


    emit SigVideoTotalSeconds(ic->duration / 1000000LL);


    for (i = 0; i < ic->nb_streams; i++) {
        AVStream* st = ic->streams[i];
        enum AVMediaType type = st->codecpar->codec_type;
        st->discard = AVDISCARD_ALL;
        if (type >= 0 && wanted_stream_spec[type] && st_index[type] == -1)
            if (avformat_match_stream_specifier(ic, st, wanted_stream_spec[type]) > 0)
                st_index[type] = i;
    }
    for (i = 0; i < AVMEDIA_TYPE_NB; i++) {
        if (wanted_stream_spec[(AVMediaType)i] && st_index[(AVMediaType)i] == -1) {
            av_log(NULL, AV_LOG_ERROR, "Stream specifier %s does not match any %s stream\n", wanted_stream_spec[(AVMediaType)i], av_get_media_type_string((AVMediaType)i));
            st_index[(AVMediaType)i] = INT_MAX;
        }
    }

    //获得视频、音频、字幕的流索引

    st_index[AVMEDIA_TYPE_VIDEO] =
        av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO,
            st_index[AVMEDIA_TYPE_VIDEO], -1, NULL, 0);

    st_index[AVMEDIA_TYPE_AUDIO] =
        av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO,
            st_index[AVMEDIA_TYPE_AUDIO],
            st_index[AVMEDIA_TYPE_VIDEO],
            NULL, 0);

    st_index[AVMEDIA_TYPE_SUBTITLE] =
        av_find_best_stream(ic, AVMEDIA_TYPE_SUBTITLE,
            st_index[AVMEDIA_TYPE_SUBTITLE],
            (st_index[AVMEDIA_TYPE_AUDIO] >= 0 ?
                st_index[AVMEDIA_TYPE_AUDIO] :
                st_index[AVMEDIA_TYPE_VIDEO]),
            NULL, 0);

    if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
        AVStream* st = ic->streams[st_index[AVMEDIA_TYPE_VIDEO]];
        AVCodecParameters* codecpar = st->codecpar;
        AVRational sar = av_guess_sample_aspect_ratio(ic, st, NULL);
    }

    /* open the streams */
    //打开音频流
    if (st_index[AVMEDIA_TYPE_AUDIO] >= 0) {
        streamComponentOpen(is, st_index[AVMEDIA_TYPE_AUDIO]);
    }

    //打开视频流
    ret = -1;
    if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
        ret = streamComponentOpen(is, st_index[AVMEDIA_TYPE_VIDEO]);
    }

    //打开字幕流
    if (st_index[AVMEDIA_TYPE_SUBTITLE] >= 0) {
        streamComponentOpen(is, st_index[AVMEDIA_TYPE_SUBTITLE]);
    }

    if (is->video_stream < 0 && is->audio_stream < 0) {
        av_log(NULL, AV_LOG_FATAL, "Failed to open file '%s' or configure filtergraph\n",
            is->filename);
        ret = -1;
        goto fail;
    }

    if (infinite_buffer < 0 && is->realtime)
        infinite_buffer = 1;

    //读取视频数据
    for (;;) {
        if (is->abort_request)
            break;
        if (is->paused != is->last_paused) {
            is->last_paused = is->paused;
            if (is->paused)
                is->read_pause_return = av_read_pause(ic);
            else
                av_read_play(ic);
        }

        if (is->seek_req) {
            int64_t seek_target = is->seek_pos;
            int64_t seek_min = is->seek_rel > 0 ? seek_target - is->seek_rel + 2 : INT64_MIN;
            int64_t seek_max = is->seek_rel < 0 ? seek_target - is->seek_rel - 2 : INT64_MAX;
            // FIXME the +-2 is due to rounding being not done in the correct direction in generation
            //      of the seek_pos/seek_rel variables

            ret = avformat_seek_file(is->ic, -1, seek_min, seek_target, seek_max, is->seek_flags);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR,
                    "%s: error while seeking\n", is->ic->url);
            }
            else {
                if (is->audio_stream >= 0)
                    packet_queue_flush(&is->audioq);
                if (is->subtitle_stream >= 0)
                    packet_queue_flush(&is->subtitleq);
                if (is->video_stream >= 0)
                    packet_queue_flush(&is->videoq);
                if (is->seek_flags & AVSEEK_FLAG_BYTE) {
                    setClock(&is->extclk, NAN, 0);
                }
                else {
                    setClock(&is->extclk, seek_target / (double)AV_TIME_BASE, 0);
                }
            }
            is->seek_req = 0;
            is->queue_attachments_req = 1;
            is->eof = 0;
            if (is->paused)
                stepToNextFrame(is);
        }
        if (is->queue_attachments_req) {
            if (is->video_st && is->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC) {
                if ((ret = av_packet_ref(pkt, &is->video_st->attached_pic)) < 0)
                    goto fail;
                packet_queue_put(&is->videoq, pkt);
                packet_queue_put_nullpacket(&is->videoq, pkt, is->video_stream);
            }
            is->queue_attachments_req = 0;
        }

        /* if the queue are full, no need to read more */
        if (infinite_buffer < 1 &&
            (is->audioq.size + is->videoq.size + is->subtitleq.size > MAX_QUEUE_SIZE
                || (streamHasEnoughPackets(is->audio_st, is->audio_stream, &is->audioq) &&
                    streamHasEnoughPackets(is->video_st, is->video_stream, &is->videoq) &&
                    streamHasEnoughPackets(is->subtitle_st, is->subtitle_stream, &is->subtitleq)))) {
            /* wait 10 ms */
            SDL_LockMutex(wait_mutex);
            SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 10);
            SDL_UnlockMutex(wait_mutex);
            continue;
        }
        if (!is->paused &&
            (!is->audio_st || (is->auddec.finished == is->audioq.serial && frame_queue_nb_remaining(&is->sampq) == 0)) &&
            (!is->video_st || (is->viddec.finished == is->videoq.serial && frame_queue_nb_remaining(&is->pictq) == 0))) {

            //播放结束
            emit SigStop();
            continue;
        }
        //按帧读取
        ret = av_read_frame(ic, pkt);
        if (ret < 0) {
            if ((ret == AVERROR_EOF || avio_feof(ic->pb)) && !is->eof) {
                if (is->video_stream >= 0)
                    packet_queue_put_nullpacket(&is->videoq, pkt, is->video_stream);
                if (is->audio_stream >= 0)
                    packet_queue_put_nullpacket(&is->audioq, pkt, is->audio_stream);
                if (is->subtitle_stream >= 0)
                    packet_queue_put_nullpacket(&is->subtitleq, pkt, is->subtitle_stream);
                is->eof = 1;
            }
            if (ic->pb && ic->pb->error)
                break;
            SDL_LockMutex(wait_mutex);
            SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 10);
            SDL_UnlockMutex(wait_mutex);
            continue;
        }
        else {
            is->eof = 0;
        }
        /* check if packet is in play range specified by user, then queue, otherwise discard */
        stream_start_time = ic->streams[pkt->stream_index]->start_time;
        pkt_ts = pkt->pts == AV_NOPTS_VALUE ? pkt->dts : pkt->pts;
        pkt_in_play_range = AV_NOPTS_VALUE == AV_NOPTS_VALUE ||
            (pkt_ts - (stream_start_time != AV_NOPTS_VALUE ? stream_start_time : 0)) *
            av_q2d(ic->streams[pkt->stream_index]->time_base) -
            (double)(0 != AV_NOPTS_VALUE ? 0 : 0) / 1000000
            <= ((double)AV_NOPTS_VALUE / 1000000);
        //按数据帧的类型存放至对应队列
        if (pkt->stream_index == is->audio_stream && pkt_in_play_range) {
            packet_queue_put(&is->audioq, pkt);
        }
        else if (pkt->stream_index == is->video_stream && pkt_in_play_range
            && !(is->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC)) {
            packet_queue_put(&is->videoq, pkt);
        }
        else if (pkt->stream_index == is->subtitle_stream && pkt_in_play_range) {
            packet_queue_put(&is->subtitleq, pkt);
        }
        else {
            av_packet_unref(pkt);
        }
    }

    ret = 0;
fail:
    if (ic && !is->ic)
        avformat_close_input(&ic);

    if (ret != 0) {
        SDL_Event event;

        event.type = FF_QUIT_EVENT;
        event.user.data1 = is;
        SDL_PushEvent(&event);
    }
    SDL_DestroyMutex(wait_mutex);
    return;
}

void CVideoCtl::loopThread(VideoState* curStream)
{
    SDL_Event event;
    double incr, pos, frac;
    bPlayLoop_ = true;

    while (bPlayLoop_)
    {
        double x;
        refreshLoopWaitEvent(curStream, &event);
        switch (event.type) {
        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) {
            case SDLK_s: // S: Step to next frame
                stepToNextFrame(curStream);
                break;
            case SDLK_a:
                streamCycleChannel(curStream, AVMEDIA_TYPE_AUDIO);
                break;
            case SDLK_v:
                streamCycleChannel(curStream, AVMEDIA_TYPE_VIDEO);
                break;
            case SDLK_c:
                streamCycleChannel(curStream, AVMEDIA_TYPE_VIDEO);
                streamCycleChannel(curStream, AVMEDIA_TYPE_AUDIO);
                streamCycleChannel(curStream, AVMEDIA_TYPE_SUBTITLE);
                break;
            case SDLK_t:
                streamCycleChannel(curStream, AVMEDIA_TYPE_SUBTITLE);
                break;

            default:
                break;
            }
            break;
        case SDL_WINDOWEVENT:
            //窗口大小改变事件
            switch (event.window.event) {
            case SDL_WINDOWEVENT_RESIZED:
                nScreenWidth_ = curStream->width = event.window.data1;
                nScreenHeight_ = curStream->height = event.window.data2;
            case SDL_WINDOWEVENT_EXPOSED:
                curStream->force_refresh = 1;
            }
            break;
        case SDL_QUIT:
        case FF_QUIT_EVENT:
            doExit(curStream);
            break;
        default:
            break;
        }
    }

    doExit(pCurStream_);
}

CVideoCtl::CVideoCtl(QObject *parent):QObject(parent)
{
	avdevice_register_all();
	//网络格式初始化
	avformat_network_init();
}

CVideoCtl& CVideoCtl::getInstance()
{
	static CVideoCtl videoCtr;
    videoCtr.init();
	return videoCtr;
}

CVideoCtl::~CVideoCtl()
{
	avformat_network_deinit();
	SDL_Quit();
}

bool CVideoCtl::startPlay(QString strFileName, WId widPlayWid)
{
    bPlayLoop_ = false;

    emit SigStartPlay(strFileName);//正式播放，发送给标题栏
    playWid_ = widPlayWid;
    VideoState* is;


    // is = streamOpen(strFileName.toLocal8Bit().data());
    is = streamOpen(strFileName.toStdString().c_str());
    if (!is) {
        av_log(NULL, AV_LOG_FATAL, "Failed to initialize VideoState!\n");
        doExit(pCurStream_);
    }

    pCurStream_ = is;
    //事件循环
    tPlayLoopThread_ = std::thread(&CVideoCtl::loopThread, this, is); 
    tPlayLoopThread_.detach();
    return true;
}

int CVideoCtl::audioDecodeFrame(VideoState* is) const
{
    int data_size, resampled_data_size;
    av_unused double audio_clock0;
    int wanted_nb_samples;
    Frame* af;

    if (is->paused)
        return -1;

    do {
#if defined(_WIN32)
        while (frame_queue_nb_remaining(&is->sampq) == 0) {
            if ((av_gettime_relative() - audio_callback_time) > 1000000LL * is->audio_hw_buf_size / is->audio_tgt.bytes_per_sec / 2)
                return -1;
            av_usleep(1000);
        }
#endif
        if (!(af = frame_queue_peek_readable(&is->sampq)))
            return -1;
        frame_queue_next(&is->sampq);
    } while (af->serial != is->audioq.serial);

    data_size = av_samples_get_buffer_size(NULL, af->frame->ch_layout.nb_channels,
        af->frame->nb_samples,
        (AVSampleFormat)af->frame->format, 1);

    wanted_nb_samples = synchronizeAudio(is, af->frame->nb_samples);

    if (af->frame->format != is->audio_src.fmt ||
        av_channel_layout_compare(&af->frame->ch_layout, &is->audio_src.ch_layout) ||
        af->frame->sample_rate != is->audio_src.freq ||
        (wanted_nb_samples != af->frame->nb_samples && !is->swr_ctx)) {
        swr_free(&is->swr_ctx);
        swr_alloc_set_opts2(&is->swr_ctx,
            &is->audio_tgt.ch_layout, is->audio_tgt.fmt, is->audio_tgt.freq,
            &af->frame->ch_layout, (AVSampleFormat)af->frame->format, af->frame->sample_rate,
            0, NULL);
        if (!is->swr_ctx || swr_init(is->swr_ctx) < 0) {
            av_log(NULL, AV_LOG_ERROR,
                "Cannot create sample rate converter for conversion of %d Hz %s %d channels to %d Hz %s %d channels!\n",
                af->frame->sample_rate, av_get_sample_fmt_name((AVSampleFormat)af->frame->format), af->frame->ch_layout.nb_channels,
                is->audio_tgt.freq, av_get_sample_fmt_name(is->audio_tgt.fmt), is->audio_tgt.ch_layout.nb_channels);
            swr_free(&is->swr_ctx);
            return -1;
        }
        if (av_channel_layout_copy(&is->audio_src.ch_layout, &af->frame->ch_layout) < 0)
            return -1;
        is->audio_src.freq = af->frame->sample_rate;
        is->audio_src.fmt = (AVSampleFormat)af->frame->format;
    }

    if (is->swr_ctx) {
        const uint8_t** in = (const uint8_t**)af->frame->extended_data;
        uint8_t** out = &is->audio_buf1;
        int out_count = (int64_t)wanted_nb_samples * is->audio_tgt.freq / af->frame->sample_rate + 256;
        int out_size = av_samples_get_buffer_size(NULL, is->audio_tgt.ch_layout.nb_channels, out_count, is->audio_tgt.fmt, 0);
        int len2;
        if (out_size < 0) {
            av_log(NULL, AV_LOG_ERROR, "av_samples_get_buffer_size() failed\n");
            return -1;
        }
        if (wanted_nb_samples != af->frame->nb_samples) {
            if (swr_set_compensation(is->swr_ctx, (wanted_nb_samples - af->frame->nb_samples) * is->audio_tgt.freq / af->frame->sample_rate,
                wanted_nb_samples * is->audio_tgt.freq / af->frame->sample_rate) < 0) {
                av_log(NULL, AV_LOG_ERROR, "swr_set_compensation() failed\n");
                return -1;
            }
        }
        av_fast_malloc(&is->audio_buf1, &is->audio_buf1_size, out_size);
        if (!is->audio_buf1)
            return AVERROR(ENOMEM);
        len2 = swr_convert(is->swr_ctx, out, out_count, in, af->frame->nb_samples);
        if (len2 < 0) {
            av_log(NULL, AV_LOG_ERROR, "swr_convert() failed\n");
            return -1;
        }
        if (len2 == out_count) {
            av_log(NULL, AV_LOG_WARNING, "audio buffer is probably too small\n");
            if (swr_init(is->swr_ctx) < 0)
                swr_free(&is->swr_ctx);
        }
        is->audio_buf = is->audio_buf1;
        resampled_data_size = len2 * is->audio_tgt.ch_layout.nb_channels * av_get_bytes_per_sample(is->audio_tgt.fmt);
    }
    else {
        is->audio_buf = af->frame->data[0];
        resampled_data_size = data_size;
    }

    audio_clock0 = is->audio_clock;
    /* update the audio clock with the pts */
    if (!isnan(af->pts))
        is->audio_clock = af->pts + (double)af->frame->nb_samples / af->frame->sample_rate;
    else
        is->audio_clock = NAN;
    is->audio_clock_serial = af->serial;
#ifdef DEBUG
    {
        static double last_clock;
        printf("audio: delay=%0.3f clock=%0.3f clock0=%0.3f\n",
            is->audio_clock - last_clock,
            is->audio_clock, audio_clock0);
        last_clock = is->audio_clock;
    }
#endif
    return resampled_data_size;
}

void CVideoCtl::updateSampleDisplay(VideoState* is, short* samples, int samples_size) const
{
    int size, len;

    size = samples_size / sizeof(short);
    while (size > 0) {
        len = SAMPLE_ARRAY_SIZE - is->sample_array_index;
        if (len > size)
            len = size;
        memcpy(is->sample_array + is->sample_array_index, samples, len * sizeof(short));
        samples += len;
        is->sample_array_index += len;
        if (is->sample_array_index >= SAMPLE_ARRAY_SIZE)
            is->sample_array_index = 0;
        size -= len;
    }
}

void CVideoCtl::setClockAt(Clock* c, double pts, int serial, double time) const
{
    c->pts = pts;
    c->last_updated = time;
    c->pts_drift = c->pts - time;
    c->serial = serial;
}

void CVideoCtl::syncClockToSlave(Clock* c, Clock* slave) const
{
    double clock = getClock(c);
    double slave_clock = getClock(slave);
    if (!std::isnan(slave_clock) && (std::isnan(clock) || fabs(clock - slave_clock) > AV_NOSYNC_THRESHOLD))
        setClock(c, slave_clock, slave->serial);
}

void CVideoCtl::onPlaySeek(double dPercent)
{
    if (pCurStream_ == nullptr)
    {
        return;
    }
    int64_t ts = dPercent * pCurStream_->ic->duration;
    if (pCurStream_->ic->start_time != AV_NOPTS_VALUE)
        ts += pCurStream_->ic->start_time;
    streamSeek(pCurStream_, ts, 0);
}

void CVideoCtl::onPlayVolume(double dPercent)
{
    nStartupVolume_ = dPercent * SDL_MIX_MAXVOLUME;
    if (pCurStream_ == nullptr)
    {
        return;
    }
    pCurStream_->audio_volume = nStartupVolume_;
}

void CVideoCtl::onSeekForward()
{
    if (pCurStream_ == nullptr)
        return;

    double incr = 5.0;
    double pos = getMasterClock(pCurStream_);
    if (std::isnan(pos))
        pos = (double)pCurStream_->seek_pos / AV_TIME_BASE;
    pos += incr;
    if (pCurStream_->ic->start_time != AV_NOPTS_VALUE && pos < pCurStream_->ic->start_time / (double)AV_TIME_BASE)
        pos = pCurStream_->ic->start_time / (double)AV_TIME_BASE;
    streamSeek(pCurStream_, (int64_t)(pos * AV_TIME_BASE), (int64_t)(incr * AV_TIME_BASE));
}

void CVideoCtl::onSeekBack()
{
    if (pCurStream_ == nullptr)
        return;

    double incr = -5.0;
    double pos = getMasterClock(pCurStream_);
    if (std::isnan(pos))
        pos = (double)pCurStream_->seek_pos / AV_TIME_BASE;
    pos += incr;
    if (pCurStream_->ic->start_time != AV_NOPTS_VALUE && pos < pCurStream_->ic->start_time / (double)AV_TIME_BASE)
        pos = pCurStream_->ic->start_time / (double)AV_TIME_BASE;
    streamSeek(pCurStream_, (int64_t)(pos * AV_TIME_BASE), (int64_t)(incr * AV_TIME_BASE));
}

void CVideoCtl::onAddVolume() 
{
    if (pCurStream_ == nullptr)
    {
        return;
    }
    updateVolume(1, SDL_VOLUME_STEP);
}

void CVideoCtl::onSubVolume()
{
    if (pCurStream_ == nullptr)
    {
        return;
    }
    updateVolume(-1, SDL_VOLUME_STEP);
}

void CVideoCtl::onPause()
{
    if (pCurStream_ == nullptr)
    {
        return;
    }
    togglePause(pCurStream_);
    emit SigPauseStat(pCurStream_->paused);
}

void CVideoCtl::onStop()
{
    bPlayLoop_ = false;
}
