#include "MainWnd.h"
#include "../common/unility.h"
#include <qlabel.h>
#include "cshow.h"
#include "ctrlbar.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <qtextedit.h>
#include <QStackedWidget>
#include <QMouseEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QCheckBox>
#include <QComboBox>
#include <QMediaPlayer>
#include <QAudioOutput>

#include "../cvideoctl.h"

MainWnd::MainWnd(QWidget *parent):QMainWindow(parent), pPlayWnd_(new CShow(this)),
    pSrtWnd_(new QLabel(this))
{
	ui.setupUi(this);
    //无边框、无系统菜单、 任务栏点击最小化
    //setWindowFlags(Qt::FramelessWindowHint | Qt::WindowMinimizeButtonHint);
    //设置任务栏图标
    setWindowIcon(QIcon("../res/Player.ico"));
    //加载样式
    QString qss = CGlobal::GetQssStr("../res/mainwid.css");
    setStyleSheet(qss);
    // 追踪鼠标 用于播放时隐藏鼠标
    setMouseTracking(true);
    
    //窗口建立
    QVBoxLayout *mainWnd = new QVBoxLayout(this);
    auto blackLine = new QWidget(this);
    blackLine->setStyleSheet("background-color:#000000");
    blackLine->setFixedHeight(2);   
   
    pSrtWnd_->setStyleSheet("font-size:16px");
    pSrtWnd_->setFixedHeight(65);
    pSrtWnd_->setAlignment(Qt::AlignCenter);
    auto ctrBar = new CtrlBar(this);
    auto audioEdit =initStackedWnd();
    mainWnd->addWidget(pPlayWnd_);    
    mainWnd->addWidget(ctrBar);
    mainWnd->addWidget(pSrtWnd_);
    mainWnd->addWidget(blackLine);    
    mainWnd->addLayout(audioEdit); 
    centralWidget()->setLayout(mainWnd);
    connectSignalSlots();


}

void MainWnd::init(QWidget * window)
{
    qDebug()<<"hahahaha============";

}

void MainWnd::speekerBtnClicked()
{
    QPushButton* button = qobject_cast<QPushButton*>(QObject::sender());
    auto name = button->text();
    strCurSpeeker_ = name;
    pPlayWnd_->setFocus();  

    for (auto & item :mSpeekerStye_)
    {
        if(item.first.contains(name))
        {
            QWidget *widget2 = new QWidget(this);
            QHBoxLayout *layout1 = new QHBoxLayout;
            QLabel *label2 = new QLabel("语\n音\n风\n格",this);
            label2->setFixedWidth(30);
            layout1->addWidget(label2);
            auto tempStyle = item.second.split(",");
            QGridLayout *grid = new QGridLayout;
            int buttonNumber=0;  
            for(int row = 0; row < 4; row++) {
                for(int col = 0; col < 4; col++) {            
                    buttonNumber = row * 4 + col;
                    if (buttonNumber==tempStyle.size())
                        break;
                    auto style = tempStyle[buttonNumber];
                    QPushButton *button = new QPushButton(style,this);
                    button->setFixedSize(120, 25); // 设置按钮大小为100x50
                    button->setStyleSheet("border: 1px solid black;");
                    grid->addWidget(button, row, col);
                    connect(button,&QPushButton::clicked,this,&MainWnd::styleBtnClicked);              
                }
                if (buttonNumber==tempStyle.size())
                        break;
            }
            layout1->addLayout(grid);
            widget2->setLayout(layout1);           
            pStackedWnd_->addWidget(widget2);
            pStackedWnd_->setCurrentIndex(1);           
            return;
        }  
    } 
    if (pAudioCheckBox_->isChecked())
    {
        playSpeekerVoice(name);
    }
    for (auto& item:vSpeakersApi_)
    {
        if (item.contains(name)&&nCurSrt_<nTotal_)
        {
            vSrt_[nCurSrt_].nameSsml=item;            
            break;
        }
    } 
   
}

void MainWnd::styleBtnClicked()
{
    QPushButton* button = qobject_cast<QPushButton*>(QObject::sender()); 
    if (pAudioCheckBox_->isChecked())
    {
        playSpeekerVoice(strCurSpeeker_+"_"+button->text());
        return;    
    }
    if(nCurSrt_<nTotal_)  
    {
        vSrt_[nCurSrt_].styleSsml= button->text();  
    } 
    
    auto temp = pStackedWnd_->currentWidget();
    pStackedWnd_->setCurrentIndex(0);
    temp->deleteLater();
    pPlayWnd_->setFocus();
}

void MainWnd::openFiles()
{
    QStringList fileNames = QFileDialog::getOpenFileNames(this,tr("Open Files"),"/",
        tr("video json Files (*.json *.mp4 *.3gpp *.3gp *.webm);;All Files (*.*)"));
  
    foreach(QString fileName, fileNames) 
    {
        if (fileName.contains("json"))
        {
            QFile f(fileName);
            if (!f.open(QIODeviceBase::ReadOnly | QIODeviceBase::Text))      
                break;
           
            auto jsonStr = f.readAll();
            auto doc = QJsonDocument::fromJson(jsonStr);
            auto aJson = doc.array();
            f.close();
            for (int i =0;i< aJson.count();i++)
            {
                auto temp = aJson[i].toObject();
                auto s = int(temp["start"].toDouble()*1000);
                auto e = int(temp["end"].toDouble()*1000);
                auto t = temp["text"].toString();   
                vSrt_.emplace_back(USrt{s,e,t,"",""});            
            }
            nTotal_ = vSrt_.size();
            if(nCurSrt_<nTotal_)  
                pSrtWnd_->setText(vSrt_[nCurSrt_].text);  
        }            
        else
        {           
            strFilmName_ = fileName;
            emit sigPlayVideo(fileName);
        }
    } 

}

QHBoxLayout *  MainWnd::initStackedWnd()
{      
    pStackedWnd_ = new QStackedWidget(this);
    auto pSpeekerWnd = new QWidget(this);
    pStackedWnd_->setFixedSize(500,110);
    QHBoxLayout *layout1 = new QHBoxLayout;
    QLabel *label1 = new QLabel("语\n音\n角\n色",this);
    label1->setFixedWidth(30);
        
    QGridLayout *grid = new QGridLayout;     
    for(int row = 0; row < 4; row++) {
        for(int col = 0; col < 5; col++) {            
            int buttonNumber = row * 5 + col;
            auto name = vSpeakersApi_[buttonNumber].split("-")[2].split("N")[0];
            QPushButton *button = new QPushButton(name,this);
            button->setFixedSize(85, 25); // 设置按钮大小为100x50
            button->setStyleSheet("border: 1px solid black;");
            grid->addWidget(button, row, col);
            connect(button,&QPushButton::clicked,this,&MainWnd::speekerBtnClicked);
   
        }
    }

    grid->setSpacing(0);
    layout1->addWidget(label1);
    layout1->addLayout(grid);
    pSpeekerWnd->setLayout(layout1);
    pStackedWnd_->addWidget(pSpeekerWnd);

    auto hLayout = new QHBoxLayout(this);
    pTextEdit_ =  new QTextEdit(this);
    pTextEdit_->setEnabled(false);
    pTextEdit_->setStyleSheet("border: 1px solid black;");
    pTextEdit_->setFixedSize(400,96);    
    pAudioCheckBox_ =  new QCheckBox("试听声音",this);
    auto saveSrtBtn =  new QPushButton("保存字幕",this);
    pComboBox_ = new QComboBox(this); // create a QComboBox object
    auto l1 = new QLabel("默认声音",this);
    for (auto &item : mSpeekerStye_)
    {
        auto name = item.first.split("-")[2].split("N")[0];
        pComboBox_->addItem(name);
    }
    pComboBox_->setFixedWidth(95);
    pComboBox_->setCurrentText("Xiaoxiao");
    auto w1 = new QWidget(this);
    w1->setFixedSize(100,96);
    auto a2 = new QVBoxLayout(this);
    a2->addWidget(pAudioCheckBox_);
    a2->addWidget(l1);
    a2->addWidget(pComboBox_);
    l1->setAlignment(Qt::AlignCenter);
    w1->setLayout(a2);
    hLayout->addStretch();
    hLayout->addWidget(w1);
    hLayout->addWidget(pStackedWnd_);
    hLayout->addWidget(pTextEdit_);
    hLayout->addWidget(saveSrtBtn);
    hLayout->addStretch();
    connect(saveSrtBtn,&QPushButton::clicked,this,&MainWnd::saveSrt);
    return hLayout;
}

void MainWnd::connectSignalSlots()
{
    connect(pPlayWnd_,&CShow::sigOpenFiles,this,&MainWnd::openFiles);
    connect(this,&MainWnd::sigPlayVideo ,pPlayWnd_,&CShow::onPlay);
    connect(this,&MainWnd::sigEditSrt ,this,&MainWnd::editSrt);

    connect(&CVideoCtl::getInstance(), &CVideoCtl::SigVideoPlaySeconds, this, &MainWnd::showSrt);
    connect(&CVideoCtl::getInstance(), &CVideoCtl::SigStopFinished, this, &MainWnd::saveSrt);
    connect(this, &MainWnd::sigPlayOrPause, &CVideoCtl::getInstance(), &CVideoCtl::onPause);
}

void MainWnd::showSrt(double dSeconds)
{
    int nSeconds = int(dSeconds * 1000);    
    if (nCurSrt_ < nTotal_)
    {
        if (nSeconds>vSrt_[nCurSrt_].end)
        {
            nCurSrt_ += 1;
            pSrtWnd_->setText(vSrt_[nCurSrt_].text);  
            
        }
    }    
}

void MainWnd::editSrt()
{
    if (isEditDing_ && nCurSrt_<nTotal_)
    {
        isEditDing_= false;
        pPlayWnd_->setFocus();
        vSrt_[nCurSrt_].text= pTextEdit_->toPlainText();
        pTextEdit_->clear();
        emit sigPlayOrPause();
    }
    else
    {       
        auto temp = pSrtWnd_->text().split("\n");
        if (temp.size()>1)
        {
            isEditDing_= true;
            pTextEdit_->setEnabled(true);
            pTextEdit_->setFocus();
            pTextEdit_->setText(temp[1]);         
        }            
    }
     qDebug()<<"isEditding"<<isEditDing_;   
   
    
}

void MainWnd::saveSrt()
{

    QJsonDocument document;
    QJsonArray srtArray;
  
    for (auto& item : vSrt_)
    {
        auto tempText = item.text.split("\n");
        if (tempText.size()>1)
        { 
            item.text= tempText[1];         
        }   
        if(item.nameSsml.isEmpty())  
            item.nameSsml=pComboBox_->currentText();  
   
        QJsonObject temp;
        temp.insert("start", item.start);
        temp.insert("end", item.end);
        temp.insert("text", item.text);
        temp.insert("nameSsml", item.nameSsml);
        temp.insert("styleSsml", item.styleSsml);
        srtArray.append(temp);
    }
    document.setArray(srtArray);
    QByteArray bytes = document.toJson(QJsonDocument::Indented);
    QFileInfo fileInfo(strFilmName_);    
    auto fileName = fileInfo.baseName() + "Edit.json";
    QFile file(fileInfo.path()+"/"+ fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
    {
        QTextStream iStream(&file);        
        iStream << bytes; 
        pSrtWnd_->setText("保存修改后的字幕数据到："+fileInfo.path()+"/" + fileName);
    }
    file.close();   
    pPlayWnd_->setFocus();
}

void MainWnd::playSpeekerVoice(QString name)
{    
    if (!pMediaPlayer_)
    {
        pMediaPlayer_ = new QMediaPlayer;
        auto audioOutput = new QAudioOutput(this);
        audioOutput->setVolume(50);
        pMediaPlayer_->setAudioOutput(audioOutput);
    }   
    auto mp3File = "speekerAudio/"+name+".mp3";
    pMediaPlayer_->setSource(QUrl::fromLocalFile(mp3File));
    pMediaPlayer_->play();
}

void MainWnd::keyReleaseEvent(QKeyEvent *event)
{
    qDebug() << "MainWid::keyPressEvent:" << event->key();
	switch (event->key())
	{
	case Qt::Key_Return://编辑字幕
        emit sigEditSrt();
		break;
    case Qt::Key_Space://暂停
    {
        if (isEditDing_)
            break;
        emit sigPlayOrPause();
            break;
    }
        
    // case Qt::Key_Left://后退5s
    //     emit sigSeekBack();
    //     break;
    // case Qt::Key_Right://前进5s
    //     qDebug() << "前进5s";
    //     emit sigSeekForward();
    //     break;
    // case Qt::Key_Up://增加10音量
    //     emit sigAddVolume();
    //     break;
    // case Qt::Key_Down://减少10音量
    //     emit sigSubVolume();
    //     break;    
    // case Qt::Key_F: //采用deepl翻译做字幕
    // {
    //     auto srtCurstrList = m_vSrt[m_dictCount].text.split("\n");
    //     m_curSrt.text = srtCurstrList[2];
    //     break;
    // }
    // case Qt::Key_G://最高兴的情感
    //     m_curSrt.ssml.emotion = m_emotions[0];       
    //     break;
    // case Qt::Key_D://欢快，讨人喜欢的情感
    //     m_curSrt.ssml.emotion = m_emotions[1];
    //     break;
    // case Qt::Key_S://有点伤感
    //     m_curSrt.ssml.emotion = m_emotions[3];
    //     break;
    // case Qt::Key_A://悲伤
    //     m_curSrt.ssml.emotion = m_emotions[4];
    //     break;
    // case Qt::Key_H://愤怒
    //     m_curSrt.ssml.emotion = m_emotions[5];
    //     break;
    // case Qt::Key_R://选择女性第二个说话
    //     m_curSrt.ssml.name = m_speekWoman[1];
    //     break;
    // case Qt::Key_E://选择女性第三个说话
    //     m_curSrt.ssml.name = m_speekWoman[2];
    //     break;
    // case Qt::Key_W://选择女性第四个说话
    //     m_curSrt.ssml.name = m_speekWoman[3];
    //     break;
    // case Qt::Key_Q://选择女性第五个说话
    //     m_curSrt.ssml.name = m_speekWoman[4];
    //     break;
    // case Qt::Key_V://选择男性第二个说话
    //     m_curSrt.ssml.name = m_speekMan[1];
    //     break;
    // case Qt::Key_C://选择男性第三个说话
    //     m_curSrt.ssml.name = m_speekMan[2];
    //     break;
    // case Qt::Key_X://选择男性第四个说话
    //     m_curSrt.ssml.name = m_speekMan[3];
    //     break;
    // case Qt::Key_Z://选择男性第五个说话
    //     m_curSrt.ssml.name = m_speekMan[4];
    //     break;        
	default:
		break;
	}
}

MainWnd::~MainWnd()
{    
}
