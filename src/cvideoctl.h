#pragma once

#include <QObject>
#include <QThread>
#include <QString>
#include "common/unility.h"
#include "common/datactl.h"


class CVideoCtl  : public QObject
{
	Q_OBJECT

public:
	static CVideoCtl& getInstance();
	~CVideoCtl();

	bool startPlay(QString strFileName, WId widPlayWid);
	int audioDecodeFrame(VideoState* is) const;
	void updateSampleDisplay(VideoState* is, short* samples, int samples_size) const;
	void setClockAt(Clock* c, double pts, int serial, double time) const;
	void syncClockToSlave(Clock* c, Clock* slave) const;


	void onPlaySeek(double dPercent);
	void onPlayVolume(double dPercent);
	void onSeekForward();
	void onSeekBack();
	void onAddVolume() ;
	void onSubVolume();
	void onPause();
	void onStop();

signals:
	void SigPlayMsg(QString strMsg);//< 错误信息
	void SigFrameDimensionsChanged(int nFrameWidth, int nFrameHeight); //<视频宽高发生变化
	void SigVideoTotalSeconds(int nSeconds);
	void SigVideoPlaySeconds(double nSeconds);
	void SigVideoVolume(double dPercent);
	void SigPauseStat(bool bPaused);
	void SigStop();
	void SigStopFinished();//停止播放完成
	void SigStartPlay(QString strFileName);



private:
	bool init();
	VideoState* streamOpen(const char* filename);
	void streamClose(VideoState* is);
	//打开流
	int streamComponentOpen(VideoState* is, int stream_index);	
	//关闭流对应的解码器等
	void streamComponentClose(VideoState* is, int stream_index);
	void streamTogglePause(VideoState* is) const;
	void streamCycleChannel(VideoState* is, int codec_type);
	int streamHasEnoughPackets(AVStream* st, int stream_id, PacketQueue* queue) const;
	void streamSeek(VideoState* is, int64_t pos, int64_t rel) const;

	void videoImageDisplay(VideoState* is);
	int videoOpen(VideoState* is);
	void videoDisplay(VideoState* is) ;
	int subtitleThread(void* arg) const;
	int audioThread(void* arg) const;
	int videoThread(void* arg) const;
	//读取线程
	void readThread(VideoState* curStream);
	void loopThread(VideoState* curStream);
	void refreshLoopWaitEvent(VideoState* is, SDL_Event* event);

	void stepToNextFrame(VideoState* is) const;
	//从视频队列中获取数据，并解码数据，得到可显示的视频帧
	int getVideoFrame(VideoState* is, AVFrame* frame) const;
	int queuePicture(VideoState* is, AVFrame* src_frame, double pts, double duration, int64_t pos, int serial) const;
	int synchronizeAudio(VideoState* is, int nb_samples) const;
	int audioOpen(void* opaque, AVChannelLayout* wanted_channel_layout, int wanted_sample_rate, struct AudioParams* audio_hw_params);
	void videoRefresh(void* opaque, double* remaining_time);

	double getMasterClock(VideoState* is) const;
	double getClock(Clock* c) const;
	void setClock(Clock* c, double pts, int serial) const;
	void initClock(Clock* c, int* queue_serial) const;
	void checkExternalClockSpeed(VideoState* is) const;
	void setClockSpeed(Clock* c, double speed) const;

	void doExit(VideoState* is);
	int uploadTexture(SDL_Texture* tex, AVFrame* frame, struct SwsContext** img_convert_ctx) const;
	int reallocTexture(SDL_Texture** texture, Uint32 new_format, int new_width, int new_height, SDL_BlendMode blendmode, int init_texture) const;
	void calculateDisplayRect(SDL_Rect* rect, int scr_xleft, int scr_ytop, int scr_width, int scr_height, int pic_width, int pic_height, AVRational pic_sar) const;
	int isRealtime(AVFormatContext* s) const;
	int getMasterSyncType(VideoState* is) const;
	double vpDuration(VideoState* is, Frame* vp, Frame* nextvp) const;
	double computeTargetDelay(double delay, VideoState* is) const;
	void updateVideoPts(VideoState* is, double pts, int64_t pos, int serial) const;

	void togglePause(VideoState* is);
	void updateVolume(int sign, double step) ;
private:
	explicit CVideoCtl(QObject* parent = nullptr);

	bool bInited_ = false;
	bool bPlayLoop_ = false;
	VideoState* pCurStream_ = nullptr;

	SDL_Window* window_ = nullptr;
	SDL_Renderer* renderer_ = nullptr;
	SDL_RendererInfo rendererInfo_ = { 0 };
	SDL_AudioDeviceID audioDev_;
	WId playWid_;//播放窗口

	int nScreenWidth_ = 0;
	int nScreenHeight_ = 0;
	int nStartupVolume_ = 30;

	std::thread tPlayLoopThread_;

	int nFrameW_ = 0;
	int nFrameH_ = 0;
};
