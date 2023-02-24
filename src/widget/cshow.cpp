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

    CVideoCtl::getInstance().onPlayVolume(0.95);
    qActionGroup_.addAction("加载文件");
    qActionGroup_.addAction("全屏");
    qActionGroup_.addAction("停止");
    qMenu_.addActions(qActionGroup_.actions());
    connect(&qActionGroup_, &QActionGroup::triggered, this, &CShow::onActionsTriggered);
    
}

CShow::~CShow(){}

void CShow::onStopFinished()
{
    update();
}


void CShow::onFrameDimensionsChanged(int nFrameWidth, int nFrameHeight)
{
    qDebug() << "Show::OnFrameDimensionsChanged" << nFrameWidth << nFrameHeight;
    nLastFrameWidth_ = nFrameWidth;
    nLastFrameHeight_ = nFrameHeight;
    changeShow();
}


void CShow::onPlay(QString strFile)
{
    CVideoCtl::getInstance().startPlay(strFile, winId());
}

void CShow::resizeEvent(QResizeEvent* event)
{
    Q_UNUSED(event);
    changeShow();
}


void CShow::mousePressEvent(QMouseEvent* event)
{
    if (event->buttons() & Qt::RightButton)
    {
        QPoint globalPos = event->globalPos();       
        qMenu_.exec(globalPos);   
    }
    QWidget::mousePressEvent(event);
}

void CShow::onActionsTriggered(QAction* action)
{
    QString strAction = action->text();
    if (strAction == "加载文件")
    {
        emit sigOpenFiles();
    }
    else if (strAction == "停止")
    {
        emit sigStop();
    }
    else if (strAction == "全屏")
    {
        emit sigFullScreen();
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

