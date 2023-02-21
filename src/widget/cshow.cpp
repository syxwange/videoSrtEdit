#include "cshow.h"
#include <qurl.h>
#include <QKeyEvent>
#include <QDropEvent>
#include <qmimedata.h>
#include <QMutex>
#include "../cvideoctl.h"

QMutex g_show_rect_mutex;

CShow::CShow(QWidget *parent):QWidget(parent),qActionGroup_(this),qMenu_(this)
{
    setAcceptDrops(true);
    setMouseTracking(true);

    setUpdatesEnabled(false);
    //VideoCtl::GetInstance()->OnPlayVolume(0.95);
    CVideoCtl::getInstance().onPlayVolume(0.95);
    qActionGroup_.addAction("全屏");
    qActionGroup_.addAction("暂停");
    qActionGroup_.addAction("停止");
    qMenu_.addActions(qActionGroup_.actions());
}

CShow::~CShow(){}

void CShow::onStopFinished()
{
    update();
}

void CShow::dropEvent(QDropEvent* event)
{
    QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty())   
        return;

    QString strFileName = urls[0].toLocalFile();
    qDebug() << strFileName;
    emit SigOpenFile(strFileName);    
}

void CShow::onFrameDimensionsChanged(int nFrameWidth, int nFrameHeight)
{
    qDebug() << "Show::OnFrameDimensionsChanged" << nFrameWidth << nFrameHeight;
    nLastFrameWidth_ = nFrameWidth;
    nLastFrameHeight_ = nFrameHeight;
    changeShow();
}

bool CShow::init()
{
    //SigPlay可删掉，没有发现其它的引用
    connect(this, &CShow::SigPlay, this, &CShow::onPlay);
    connect(&qActionGroup_, &QActionGroup::triggered, this, &CShow::onActionsTriggered);
    return true;
}

void CShow::onPlay(QString strFile)
{
    //VideoCtl::GetInstance()->StartPlay(strFile,  winId());
    CVideoCtl::getInstance().startPlay(strFile, winId());
}

void CShow::dragEnterEvent(QDragEnterEvent* event)
{
    event->acceptProposedAction();
}

void CShow::resizeEvent(QResizeEvent* event)
{
    Q_UNUSED(event);
    changeShow();
}

void CShow::keyReleaseEvent(QKeyEvent* event)
{
    qDebug() << "in CShow::keyPressEvent:" << event->key();
    switch (event->key())
    {
    case Qt::Key_Return://全屏
        SigFullScreen();
        break;
    case Qt::Key_Left://后退5s
        emit SigSeekBack();
        break;
    case Qt::Key_Right://前进5s
        qDebug() << "前进5s";
        emit SigSeekForward();
        break;
    case Qt::Key_Up://增加10音量
        emit SigAddVolume();
        break;
    case Qt::Key_Down://减少10音量
        emit SigSubVolume();
        break;   
    default:
        QWidget::keyPressEvent(event);
        break;
    }
}

void CShow::mousePressEvent(QMouseEvent* event)
{
    if (event->buttons() & Qt::RightButton)
    {
        emit SigShowMenu();
    }
    QWidget::mousePressEvent(event);
}

void CShow::onActionsTriggered(QAction* action)
{
    QString strAction = action->text();
    if (strAction == "全屏")
    {
        emit SigFullScreen();
    }
    else if (strAction == "停止")
    {
        emit SigStop();
    }
    else if (strAction == "暂停" || strAction == "播放")
    {
        emit SigPlayOrPause();
    }
}

void CShow::changeShow()
{
    g_show_rect_mutex.lock();
    if (nLastFrameWidth_ == 0 && nLastFrameHeight_ == 0)
    {
        setGeometry(0, 0, width(), height());
    }
    else
    {
        float aspect_ratio;
        int width, height, x, y;
        int scr_width = this->width();
        int scr_height = this->height();

        aspect_ratio = (float)nLastFrameWidth_ / (float)nLastFrameHeight_;

        height = scr_height;
        width = lrint(height * aspect_ratio) & ~1;
        if (width > scr_width)
        {
            width = scr_width;
            height = lrint(width / aspect_ratio) & ~1;
        }
        x = (scr_width - width) / 2;
        y = (scr_height - height) / 2;
        setGeometry(x, y, width, height);     
    }
    g_show_rect_mutex.unlock();
}

