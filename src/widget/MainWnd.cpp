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

MainWnd::MainWnd(QWidget *parent):QMainWindow(parent)
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
    auto a1 = new QWidget(this);
    auto temp = new CShow(this);
    setAcceptDrops(true);
    setCentralWidget(temp);
    auto aa  = new QLabel("aaaaaa\n这是一个测试\ncccc",this);
    aa->setStyleSheet("font-size:20px");
    aa->setFixedHeight(70);
    aa->setAlignment(Qt::AlignCenter);
    auto ctrBar = new CtrlBar(this);
    ctrBar->Init();
    mainWnd->addWidget(temp);    
    mainWnd->addWidget(ctrBar);
    mainWnd->addWidget(blackLine);
    mainWnd->addWidget(aa);
    mainWnd->addWidget(blackLine);
    auto hLayout = new QHBoxLayout(this);

    auto grid =getStackedWnd(this);
    auto textEdit =  new QTextEdit(this);
    textEdit->setFixedSize(300,70);
    
    hLayout->addWidget(grid);
    hLayout->addWidget(textEdit);
    mainWnd->addLayout(hLayout);
    a1->setLayout(mainWnd);
    setCentralWidget(a1);

    temp->onPlay(QString(u8"D:/videoWorks/tempFile/123.mp4"));

}

void MainWnd::init(QWidget * window)
{

}

void MainWnd::speekerBtnClicked()
{
    QPushButton* button = qobject_cast<QPushButton*>(QObject::sender());
    if (button != nullptr) {
        qDebug() << "Button clicked: " << button->text();
    }
}

QStackedWidget*  MainWnd::getStackedWnd(QWidget * window)
{

    QStackedWidget *stackedWidget = new QStackedWidget(window);
    stackedWidget->setFixedSize(450,110);
    // 第一个窗口
    QWidget *widget1 = new QWidget;
    QHBoxLayout *layout1 = new QHBoxLayout;
    QLabel *label1 = new QLabel("语\n音\n角\n色",window);
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
    connect(vSpeekerBtn_[0],&QPushButton::clicked,[this](){this->sigChangeStackWnd(1);});
    connect(this,&MainWnd::sigChangeStackWnd,stackedWidget,&QStackedWidget::setCurrentIndex);
    grid->setSpacing(0);
    layout1->addWidget(label1);
    layout1->addLayout(grid);
    widget1->setLayout(layout1);

    // 第二个窗口
    QWidget *widget2 = new QWidget;
    QVBoxLayout *layout2 = new QVBoxLayout;
    QLabel *label2 = new QLabel("语\n音\n风\n格",window);
    label2->setFixedWidth(30);
    layout2->addWidget(label2);
    widget2->setLayout(layout2);

    // 第三个窗口
    QWidget *widget3 = new QWidget;
    QVBoxLayout *layout3 = new QVBoxLayout;
    QLabel *label3 = new QLabel("Window 3");
    layout3->addWidget(label3);
    widget3->setLayout(layout3);

    // 将三个窗口添加到StackedWidget中
    stackedWidget->addWidget(widget1);
    stackedWidget->addWidget(widget2);
    stackedWidget->addWidget(widget3);
 
    return stackedWidget;

}

MainWnd::~MainWnd()
{
}
