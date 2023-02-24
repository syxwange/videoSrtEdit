#pragma once

#include <QWidget>
#include <qtimer.h>
#include <qmenu.h>
#include <QActionGroup>
#include <qlabel.h>



class CShow : public QWidget
{
	Q_OBJECT

public:
	CShow(QWidget *parent = nullptr);
	~CShow();

    void onPlay(QString strFile);
    void onStopFinished();
    void onFrameDimensionsChanged(int nFrameWidth, int nFrameHeight);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    void onActionsTriggered(QAction* action);
    void changeShow();    

signals:
    void sigOpenFiles();///< 增加视频文件
    void sigFullScreen();//全屏播放
    void sigStop(); 


private:

    int nLastFrameWidth_ = 0; ///< 记录视频宽高
    int nLastFrameHeight_ = 0;

    QTimer timerShowCursor_;
    QLabel * label_;
    QMenu qMenu_;
    QActionGroup qActionGroup_;
};
