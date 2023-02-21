#include <QDebug>
#include <QTime>
#include <QSettings>
 
#include "ctrlbar.h"
#include "ui_ctrlbar.h"
#include "../common/unility.h"

CtrlBar::CtrlBar(QWidget *parent):QWidget(parent),ui(new Ui::CtrlBar)
{
    ui->setupUi(this);
    m_dLastVolumePercent = 1.0;

}

CtrlBar::~CtrlBar()
{
    delete ui;
}

bool CtrlBar::Init()
{
    auto temp = CGlobal::GetQssStr("../res/ctrlbar.css");
    setStyleSheet(temp);

    CGlobal::SetIcon(ui->PlayOrPauseBtn, 12, QChar(0xf04b));
    CGlobal::SetIcon(ui->StopBtn, 12, QChar(0xf04d));
    CGlobal::SetIcon(ui->VolumeBtn, 12, QChar(0xf028));
    CGlobal::SetIcon(ui->PlaylistCtrlBtn, 12, QChar(0xf036));
    CGlobal::SetIcon(ui->ForwardBtn, 12, QChar(0xf051));
    CGlobal::SetIcon(ui->BackwardBtn, 12, QChar(0xf048));
    CGlobal::SetIcon(ui->SettingBtn, 12, QChar(0xf013));

    ui->PlaylistCtrlBtn->setToolTip("播放列表");
    ui->SettingBtn->setToolTip("设置");
    ui->VolumeBtn->setToolTip("静音");
    ui->ForwardBtn->setToolTip("下一个");
    ui->BackwardBtn->setToolTip("上一个");
    ui->StopBtn->setToolTip("停止");
    ui->PlayOrPauseBtn->setToolTip("播放");
    
    ConnectSignalSlots();

    double dPercent = -1.0;
    CGlobal::GetPlayVolume(dPercent);
    if (dPercent != -1.0)
    {
        emit SigPlayVolume(dPercent);
        OnVideopVolume(dPercent);
    }

    return true;

}

bool CtrlBar::ConnectSignalSlots()
{
    QList<bool> listRet;
    bool bRet;


    connect(ui->PlaylistCtrlBtn, &QPushButton::clicked, this, &CtrlBar::SigShowOrHidePlaylist);
    connect(ui->PlaySlider, &CustomSlider::SigCustomSliderValueChanged, this, &CtrlBar::OnPlaySliderValueChanged);
    connect(ui->VolumeSlider, &CustomSlider::SigCustomSliderValueChanged, this, &CtrlBar::OnVolumeSliderValueChanged);
    connect(ui->BackwardBtn, &QPushButton::clicked, this, &CtrlBar::SigBackwardPlay);
    connect(ui->ForwardBtn, &QPushButton::clicked, this, &CtrlBar::SigForwardPlay);

    return true;
}

void CtrlBar::OnVideoTotalSeconds(int nSeconds)
{
    m_nTotalPlaySeconds = nSeconds;

    int thh, tmm, tss;
    thh = nSeconds / 3600;
    tmm = (nSeconds % 3600) / 60;
    tss = (nSeconds % 60);
    QTime TotalTime(thh, tmm, tss);

    ui->VideoTotalTimeTimeEdit->setTime(TotalTime);
}


void CtrlBar::OnVideoPlaySeconds(double dSeconds)
{
    int nSeconds = int(dSeconds);
    int thh, tmm, tss;
    thh = nSeconds / 3600;
    tmm = (nSeconds % 3600) / 60;
    tss = (nSeconds % 60);
    QTime TotalTime(thh, tmm, tss);

    ui->VideoPlayTimeTimeEdit->setTime(TotalTime);

    ui->PlaySlider->setValue(nSeconds * 1.0 / m_nTotalPlaySeconds * MAX_SLIDER_VALUE);
}

void CtrlBar::OnVideopVolume(double dPercent)
{
    ui->VolumeSlider->setValue(dPercent * MAX_SLIDER_VALUE);
    m_dLastVolumePercent = dPercent;

    if (m_dLastVolumePercent == 0)
    {
        CGlobal::SetIcon(ui->VolumeBtn, 12, QChar(0xf026));
    }
    else
    {
        CGlobal::SetIcon(ui->VolumeBtn, 12, QChar(0xf028));
    }

    CGlobal::SavePlayVolume(dPercent);
}

void CtrlBar::OnPauseStat(bool bPaused)
{
    qDebug() << "CtrlBar::OnPauseStat" << bPaused;
    if (bPaused)
    {
        CGlobal::SetIcon(ui->PlayOrPauseBtn, 12, QChar(0xf04b));
        ui->PlayOrPauseBtn->setToolTip("播放");
    }
    else
    {
        CGlobal::SetIcon(ui->PlayOrPauseBtn, 12, QChar(0xf04c));
        ui->PlayOrPauseBtn->setToolTip("暂停");
    }
}

void CtrlBar::OnStopFinished()
{
    ui->PlaySlider->setValue(0);
    QTime StopTime(0, 0, 0);
    ui->VideoTotalTimeTimeEdit->setTime(StopTime);
    ui->VideoPlayTimeTimeEdit->setTime(StopTime);
    CGlobal::SetIcon(ui->PlayOrPauseBtn, 12, QChar(0xf04b));
    ui->PlayOrPauseBtn->setToolTip("播放");
}

void CtrlBar::OnPlaySliderValueChanged()
{
    double dPercent = ui->PlaySlider->value()*1.0 / ui->PlaySlider->maximum();
    emit SigPlaySeek(dPercent);
    qDebug() << dPercent;
}

void CtrlBar::OnVolumeSliderValueChanged()
{
    double dPercent = ui->VolumeSlider->value()*1.0 / ui->VolumeSlider->maximum();
    emit SigPlayVolume(dPercent);

    OnVideopVolume(dPercent);
}

void CtrlBar::on_PlayOrPauseBtn_clicked()
{
    emit SigPlayOrPause();
}

void CtrlBar::on_VolumeBtn_clicked()
{
    if (ui->VolumeBtn->text() == QChar(0xf028))
    {
        CGlobal::SetIcon(ui->VolumeBtn, 12, QChar(0xf026));
        ui->VolumeSlider->setValue(0);
        emit SigPlayVolume(0);
    }
    else
    {
        CGlobal::SetIcon(ui->VolumeBtn, 12, QChar(0xf028));
        ui->VolumeSlider->setValue(m_dLastVolumePercent * MAX_SLIDER_VALUE);
        emit SigPlayVolume(m_dLastVolumePercent);
    }

}

void CtrlBar::on_StopBtn_clicked()
{
    emit SigStop();
}

void CtrlBar::on_SettingBtn_clicked()
{
    //emit SigShowSetting();
}
