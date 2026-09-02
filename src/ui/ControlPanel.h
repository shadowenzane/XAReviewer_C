#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QWidget>

#include "../core/DsaSequence.h"

// 右侧控制面板：帧导航 / 窗宽窗位 / 伪彩 / 减影 / 导出
// 对应 Python ControlPanel
class ControlPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ControlPanel(QWidget* parent = nullptr);

    void setFrameCount(int count);
    void setPatientInfo(const PatientInfo& info);

    // 静默更新（不触发信号，供多视图同步回写）
    void setFrameSilently(int frame);
    void setWindowLevelSilently(double ww, double wc);

    int frame() const { return frameSlider_->value(); }
    RenderOptions renderOptions() const;

signals:
    // 用户交互触发的变更
    void frameRequested(int frame);
    void windowLevelChanged(double ww, double wc);
    void playToggled(bool playing);
    void pseudoColorChanged(bool enabled, const QString& colorMap);
    void subtractionChanged(bool enabled, int maskFrame, double maskSigma);
    void exportImageRequested();
    void exportVideoRequested();
    void enhanceUltrasoundRequested();

public slots:
    void play(bool on);
    void setPlayState(bool playing) { playButton_->setChecked(playing); }

private:
    void buildUi();
    void updateInfoLabel();

    // 帧导航
    QSlider* frameSlider_ = nullptr;
    QSpinBox* frameSpin_ = nullptr;
    QPushButton* playButton_ = nullptr;
    QPushButton* prevButton_ = nullptr;
    QPushButton* nextButton_ = nullptr;

    // 窗宽窗位
    QDoubleSpinBox* wwSpin_ = nullptr;
    QDoubleSpinBox* wcSpin_ = nullptr;
    QPushButton* autoWindowButton_ = nullptr;

    // 伪彩 / 减影
    QCheckBox* pseudoCheck_ = nullptr;
    QComboBox* colorMapCombo_ = nullptr;
    QCheckBox* subtractionCheck_ = nullptr;
    QSpinBox* maskFrameSpin_ = nullptr;
    QDoubleSpinBox* maskSigmaSpin_ = nullptr;

    // 增强 / 导出
    QPushButton* enhanceButton_ = nullptr;
    QPushButton* exportImageButton_ = nullptr;
    QPushButton* exportVideoButton_ = nullptr;

    // 患者信息
    QLabel* infoLabel_ = nullptr;
    PatientInfo info_;

    bool silent_ = false; // 静默更新标志
};
