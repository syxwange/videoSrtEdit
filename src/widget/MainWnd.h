#pragma once

#include <QMainWindow>
#include "ui_MainWnd.h"
#include <vector>
#include <map>

class QPushButton;
class QStackedWidget;
class MainWnd : public QMainWindow
{
	Q_OBJECT

public:
	MainWnd(QWidget *parent = nullptr);
	~MainWnd();

	void init(QWidget * window);
	void speekerBtnClicked();
private:
	QStackedWidget*  getStackedWnd(QWidget * window);

signals:
	void sigChangeStackWnd(int n);

private:
	Ui::MainWndClass ui;
	bool bPlaying_ = false; 
	bool bMoveDrag = false;
	bool bFullScreenPlay_ = false;

	int nShadowWidth_ = 0;

	std::map<std::string,std::string> mSpeekerStye_{ 
    {"zh-CN-XiaohanNeural","calm, fearful, cheerful, disgruntled, serious, angry, sad, gentle, affectionate, embarrassed"},
    {"zh-CN-XiaomoNeural","embarrassed, calm, fearful, cheerful, disgruntled, serious, angry, sad, depressed, affectionate, gentle, envious"},
    {"zh-CN-XiaoruiNeural","calm, fearful, angry, sad"},
    {"zh-CN-XiaoshuangNeural","chat"},
    {"zh-CN-XiaoxiaoNeural","assistant, chat, customerservice, newscast, affectionate, angry, calm, cheerful, disgruntled, fearful, gentle, lyrical, sad, serious, poetry-reading"},
    {"zh-CN-XiaoxuanNeural","calm, fearful, cheerful, disgruntled, serious, angry, gentle, depressed"},
    {"zh-CN-XiaozhenNeural","angry, disgruntled, cheerful, fearful, sad, seriou"},
    {"zh-CN-YunjianNeural","narration-relaxed, sports-commentary, sports-commentary-excited"},
    {"zh-CN-YunxiaNeural","calm, fearful, cheerful, angry, sad"},
    {"zh-CN-YunxiNeural","narration-relaxed, embarrassed, fearful, cheerful, disgruntled, serious, angry, sad, depressed, chat, assistant, newscast"},
    {"zh-CN-YunyangNeural","customerservice, narration-professional, newscast-casual"},
    {"zh-CN-YunyeNeural","embarrassed, calm, fearful, cheerful, disgruntled, serious, angry, sad"},
    };

	std::vector<QPushButton *> vSpeekerBtn_; 

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
