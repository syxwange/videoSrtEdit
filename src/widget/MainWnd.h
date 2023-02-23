#pragma once

#include <QMainWindow>
#include "ui_MainWnd.h"
#include <vector>
#include <map>

struct USrt
{
    int start = 0;
    int end = 0;
    QString text;
    QString  nameSsml;
    QString  styleSsml;
};


class QPushButton;
class QStackedWidget;
class CShow;
class QLabel;
class QCheckBox;
class QHBoxLayout;

class MainWnd : public QMainWindow
{
	Q_OBJECT

public:
	MainWnd(QWidget *parent = nullptr);
	~MainWnd();

	void init(QWidget * window);
	void speekerBtnClicked();
    void styleBtnClicked();
    void openFiles();
    void saveSrt();
protected:
 
private:
	QHBoxLayout *  initStackedWnd();
    void connectSignalSlots();
    void showSrt(double dSeconds);

signals:
	void sigChangeStackWnd(int n);
    void sigSrtFileName(QString name);
    void sigVideoFileName(QString name);

private:
    std::vector<USrt> vSrt_; 
    int nCurSrt_ = 0;
    int nTotal_ = 0 ;
    QString strFilmName_;

    CShow * pPlayWnd_;
    QLabel* pSrtWnd_;
    QCheckBox * pAudioCheckBox_;

	Ui::MainWndClass ui;
	bool bPlaying_ = false; 
	bool bMoveDrag = false;
	bool bFullScreenPlay_ = false;

	int nShadowWidth_ = 0;
    std::vector<QPushButton *> vSpeekerBtn_; 
    QString strCurSpeeker_;
    QStackedWidget* pStackedWnd_;

	std::map<QString,QString> mSpeekerStye_{ 
    {"zh-CN-XiaoxiaoNeural","assistant, chat, customerservice, newscast, affectionate, angry, calm, cheerful, disgruntled, fearful, gentle, lyrical, sad, serious, poetry-reading"},
    {"zh-CN-XiaohanNeural","calm, fearful, cheerful, disgruntled, serious, angry, sad, gentle, affectionate, embarrassed"},
    {"zh-CN-XiaomoNeural","embarrassed, calm, fearful, cheerful, disgruntled, serious, angry, sad, depressed, affectionate, gentle, envious"},
    {"zh-CN-XiaoruiNeural","calm, fearful, angry, sad"},    
    {"zh-CN-XiaoxuanNeural","calm, fearful, cheerful, disgruntled, serious, angry, gentle, depressed"},
    {"zh-CN-XiaozhenNeural","angry, disgruntled, cheerful, fearful, sad, seriou"},
    {"zh-CN-YunjianNeural","narration-relaxed, sports-commentary, sports-commentary-excited"},
    {"zh-CN-YunxiaNeural","calm, fearful, cheerful, angry, sad"},
    {"zh-CN-YunxiNeural","narration-relaxed, embarrassed, fearful, cheerful, disgruntled, serious, angry, sad, depressed, chat, assistant, newscast"},
    {"zh-CN-YunyangNeural","customerservice, narration-professional, newscast-casual"},
    {"zh-CN-YunyeNeural","embarrassed, calm, fearful, cheerful, disgruntled, serious, angry, sad"},
    };	

	std::vector<QString> vSpeakersApi_{
        "zh-CN-XiaochenNeural",
        "zh-CN-XiaohanNeural",
        "zh-CN-XiaomoNeural",
        "zh-CN-XiaoqiuNeural",
        "zh-CN-XiaoruiNeural",
        "zh-CN-XiaoshuangNeural",
        "zh-CN-XiaoxiaoNeural",
        "zh-CN-XiaoxuanNeural",
        "zh-CN-XiaoyanNeural",
        "zh-CN-XiaoyouNeural",    
        "zh-CN-YunjianNeural",
        "zh-CN-YunxiaNeural",
        "zh-CN-YunxiNeural",
        "zh-CN-YunyangNeural",
        "zh-CN-YunyeNeural",
        // "zh-HK-HiuGaaiNeural",
        "zh-HK-HiuMaanNeural",
        "zh-HK-WanLungNeural",
        "zh-TW-HsiaoChenNeural",
        "zh-TW-HsiaoYuNeural",
        "zh-TW-YunJheNeural"
   };
	
};
