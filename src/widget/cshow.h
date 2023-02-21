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

    bool init();
    void onPlay(QString strFile);
    void onStopFinished();
    void onFrameDimensionsChanged(int nFrameWidth, int nFrameHeight);

protected:
	void dropEvent(QDropEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    void onActionsTriggered(QAction* action);
    void changeShow();

signals:
    void SigOpenFile(QString strFileName);///< 增加视频文件
    //SigPlay可删掉，没有发现其它的引用
    void SigPlay(QString strFile); ///<播放

    void SigFullScreen();//全屏播放
    void SigPlayOrPause();
    void SigStop();
    void SigShowMenu();

    void SigSeekForward();
    void SigSeekBack();
    void SigAddVolume();
    void SigSubVolume();

private:

    int nLastFrameWidth_ = 0; ///< 记录视频宽高
    int nLastFrameHeight_ = 0;

    QTimer timerShowCursor_;
    QLabel * label_;
    QMenu qMenu_;
    QActionGroup qActionGroup_;
};
