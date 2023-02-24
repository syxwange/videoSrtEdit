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
    
    
    QVBoxLayout *mainWnd = new QVBoxLayout(this);
    auto blackLine = new QWidget(this);
    blackLine->setStyleSheet("background-color:#000000");
    blackLine->setFixedHeight(2);    
    
    pSrtWnd_->setStyleSheet("font-size:18px");
    pSrtWnd_->setFixedHeight(70);
    pSrtWnd_->setAlignment(Qt::AlignCenter);
    auto ctrBar = new CtrlBar(this);
    ctrBar->Init();
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
    if (button == nullptr) {       
        return;
    }  
    auto name = button->text();
    qDebug() << "Button clicked: " << button->text();
    QWidget *widget2 = new QWidget(this);
    QHBoxLayout *layout1 = new QHBoxLayout;
    QLabel *label2 = new QLabel("语\n音\n风\n格",this);
    label2->setFixedWidth(30);
    layout1->addWidget(label2); 
    for (auto & item :mSpeekerStye_)
    {
        if(item.first.contains(name))
        {
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
   
}

void MainWnd::styleBtnClicked()
{
     QPushButton* button = qobject_cast<QPushButton*>(QObject::sender());
    if (button == nullptr) {       
        return;
    }
    qDebug() << "Button clicked: " << button->text();
    auto strStyle= button->text();
    auto temp = pStackedWnd_->currentWidget();
    pStackedWnd_->setCurrentIndex(0);
    temp->deleteLater();
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
                auto s = int(temp["start"].toDouble()*100);
                auto e = int(temp["end"].toDouble()*100);
                auto t = temp["text"].toString();   
                vSrt_.emplace_back(USrt{s,e,t,"zh-CN-XiaoxiaoNeural","gentle"});            
            }
            nTotal_ = vSrt_.size();
        }            
        else
        {
            QFileInfo fileInfo(fileName);
            strFilmName_ = fileInfo.baseName();
            emit sigVideoFileName(fileName);
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
            vSpeekerBtn_.push_back(button);
        }
    }

    grid->setSpacing(0);
    layout1->addWidget(label1);
    layout1->addLayout(grid);
    pSpeekerWnd->setLayout(layout1);
    pStackedWnd_->addWidget(pSpeekerWnd);

    auto hLayout = new QHBoxLayout(this);
    auto textEdit =  new QTextEdit(this);
    textEdit->setStyleSheet("border: 1px solid black;");
    textEdit->setFixedSize(400,96);    
    pAudioCheckBox_ =  new QCheckBox("试听声音",this);
    auto saveSrtBtn =  new QPushButton("保存字幕",this);
    auto *comboBox = new QComboBox(this); // create a QComboBox object
    auto l1 = new QLabel("默认声音",this);
    for (auto &item : mSpeekerStye_)
    {
        auto name = item.first.split("-")[2].split("N")[0];
        comboBox->addItem(name);
    }
    comboBox->setFixedWidth(95);
    comboBox->setCurrentText("Xiaoxiao");
    auto w1 = new QWidget(this);
    w1->setFixedSize(100,96);
    auto a2 = new QVBoxLayout(this);
    a2->addWidget(pAudioCheckBox_);
    a2->addWidget(l1);
    a2->addWidget(comboBox);
    l1->setAlignment(Qt::AlignCenter);
    w1->setLayout(a2);
    hLayout->addStretch();
    hLayout->addWidget(w1);
    hLayout->addWidget(pStackedWnd_);
    hLayout->addWidget(textEdit);
    hLayout->addLayout(a2);
    hLayout->addStretch();
    return hLayout;
}

void MainWnd::connectSignalSlots()
{
    connect(pPlayWnd_,&CShow::sigOpenFiles,this,&MainWnd::openFiles);
    connect(this,&MainWnd::sigVideoFileName ,pPlayWnd_,&CShow::onPlay);

    connect(&CVideoCtl::getInstance(), &CVideoCtl::SigVideoPlaySeconds, this, &MainWnd::showSrt);
    connect(&CVideoCtl::getInstance(), &CVideoCtl::SigStopFinished, this, &MainWnd::saveSrt);

}

void MainWnd::showSrt(double dSeconds)
{
    int nSeconds = int(dSeconds * 100);    
    if (nCurSrt_ < nTotal_)
    {
        if (nSeconds>vSrt_[nCurSrt_].end)
        {
            nCurSrt_ += 1;
            pSrtWnd_->setText(vSrt_[nCurSrt_].text);  
        }
    }    
}

void MainWnd::saveSrt()
{
    QJsonDocument document;
    QJsonArray srtArray;
  
    for (auto& item : vSrt_)
    {
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
    auto fileName = strFilmName_ + "Edit.json";
    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
    {
        QTextStream iStream(&file);        
        iStream << bytes;        
        pSrtWnd_->setText("保存修改后的字幕数据到：" + fileName);
    }
    file.close();   
}

MainWnd::~MainWnd()
{    
}
