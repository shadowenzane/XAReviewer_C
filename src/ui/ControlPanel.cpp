#include "ControlPanel.h"
#include "Theme.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QSpinBox>
#include <QVBoxLayout>

ControlPanel::ControlPanel(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

void ControlPanel::buildUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // 患者信息
    infoLabel_ = new QLabel(this);
    infoLabel_->setWordWrap(true);
    infoLabel_->setStyleSheet(QStringLiteral("color: %1; padding: 4px;")
                                   .arg(QLatin1String(Theme::textDimColor())));
    layout->addWidget(infoLabel_);

    // 帧导航
    auto* navGroup = new QGroupBox(tr("帧导航"), this);
    auto* navLay = new QVBoxLayout(navGroup);

    auto* sliderRow = new QHBoxLayout();
    prevButton_ = new QPushButton(tr("◀"), navGroup);
    prevButton_->setFixedWidth(34);
    frameSlider_ = new QSlider(Qt::Horizontal, navGroup);
    frameSlider_->setRange(0, 0);
    nextButton_ = new QPushButton(tr("▶"), navGroup);
    nextButton_->setFixedWidth(34);
    sliderRow->addWidget(prevButton_);
    sliderRow->addWidget(frameSlider_, 1);
    sliderRow->addWidget(nextButton_);
    navLay->addLayout(sliderRow);

    auto* frameRow = new QHBoxLayout();
    frameRow->addWidget(new QLabel(tr("帧:"), navGroup));
    frameSpin_ = new QSpinBox(navGroup);
    frameSpin_->setRange(0, 0);
    frameSpin_->setSuffix(tr(" / %1").arg(0));
    playButton_ = new QPushButton(tr("播放"), navGroup);
    playButton_->setCheckable(true);
    frameRow->addWidget(frameSpin_, 1);
    frameRow->addWidget(playButton_);
    navLay->addLayout(frameRow);

    layout->addWidget(navGroup);

    // 窗宽窗位
    auto* wlGroup = new QGroupBox(tr("窗宽窗位"), this);
    auto* wlLay = new QGridLayout(wlGroup);
    wlLay->addWidget(new QLabel(tr("窗宽 (WW):"), wlGroup), 0, 0);
    wwSpin_ = new QDoubleSpinBox(wlGroup);
    wwSpin_->setRange(1.0, 65535.0);
    wwSpin_->setDecimals(1);
    wlLay->addWidget(wwSpin_, 0, 1);
    wlLay->addWidget(new QLabel(tr("窗位 (WC):"), wlGroup), 1, 0);
    wcSpin_ = new QDoubleSpinBox(wlGroup);
    wcSpin_->setRange(-32768.0, 32767.0);
    wcSpin_->setDecimals(1);
    wlLay->addWidget(wcSpin_, 1, 1);
    autoWindowButton_ = new QPushButton(tr("自动窗"), wlGroup);
    wlLay->addWidget(autoWindowButton_, 2, 0, 1, 2);
    layout->addWidget(wlGroup);

    // 伪彩 / 减影
    auto* fxGroup = new QGroupBox(tr("显示增强"), this);
    auto* fxLay = new QVBoxLayout(fxGroup);

    auto* pcRow = new QHBoxLayout();
    pseudoCheck_ = new QCheckBox(tr("伪彩"), fxGroup);
    colorMapCombo_ = new QComboBox(fxGroup);
    colorMapCombo_->addItems(QStringList()
                                 << QStringLiteral("JET") << QStringLiteral("HOT")
                                 << QStringLiteral("BONE") << QStringLiteral("RAINBOW")
                                 << QStringLiteral("COLOR") << QStringLiteral("LAVA")
                                 << QStringLiteral("COOL") << QStringLiteral("COPPER")
                                 << QStringLiteral("SPRING") << QStringLiteral("HSV"));
    pcRow->addWidget(pseudoCheck_, 1);
    pcRow->addWidget(colorMapCombo_, 1);
    fxLay->addLayout(pcRow);

    subtractionCheck_ = new QCheckBox(tr("减影模式（DSA）"), fxGroup);
    fxLay->addWidget(subtractionCheck_);
    auto* maskRow = new QHBoxLayout();
    maskRow->addWidget(new QLabel(tr("蒙片帧:"), fxGroup));
    maskFrameSpin_ = new QSpinBox(fxGroup);
    maskFrameSpin_->setRange(0, 0);
    maskRow->addWidget(maskFrameSpin_, 1);
    fxLay->addLayout(maskRow);
    auto* sigmaRow = new QHBoxLayout();
    sigmaRow->addWidget(new QLabel(tr("蒙片平滑 σ:"), fxGroup));
    maskSigmaSpin_ = new QDoubleSpinBox(fxGroup);
    maskSigmaSpin_->setRange(0.0, 10.0);
    maskSigmaSpin_->setSingleStep(0.5);
    maskSigmaSpin_->setValue(0.0);
    sigmaRow->addWidget(maskSigmaSpin_, 1);
    fxLay->addLayout(sigmaRow);
    layout->addWidget(fxGroup);

    // 增强 / 导出
    auto* outGroup = new QGroupBox(tr("处理与导出"), this);
    auto* outLay = new QVBoxLayout(outGroup);
    enhanceButton_ = new QPushButton(tr("超声增强 (CLAHE)"), outGroup);
    exportImageButton_ = new QPushButton(tr("导出图像 (PNG/JPG)"), outGroup);
    exportVideoButton_ = new QPushButton(tr("导出视频 (MP4)"), outGroup);
    outLay->addWidget(enhanceButton_);
    outLay->addWidget(exportImageButton_);
    outLay->addWidget(exportVideoButton_);
    layout->addWidget(outGroup);

    layout->addStretch(1);

    // 信号连接
    connect(frameSlider_, &QSlider::valueChanged, this, [this](int v) {
        if (!silent_) {
            silent_ = true;
            frameSpin_->setValue(v);
            silent_ = false;
            emit frameRequested(v);
        }
    });
    connect(frameSpin_, &QSpinBox::valueChanged, this, [this](int v) {
        if (!silent_) {
            silent_ = true;
            frameSlider_->setValue(v);
            silent_ = false;
            emit frameRequested(v);
        }
    });
    connect(prevButton_, &QPushButton::clicked, this,
            [this] { frameSlider_->setValue(frameSlider_->value() - 1); });
    connect(nextButton_, &QPushButton::clicked, this,
            [this] { frameSlider_->setValue(frameSlider_->value() + 1); });
    connect(playButton_, &QPushButton::toggled, this, &ControlPanel::playToggled);

    connect(wwSpin_, &QDoubleSpinBox::valueChanged, this, [this](double) {
        if (!silent_)
            emit windowLevelChanged(wwSpin_->value(), wcSpin_->value());
    });
    connect(wcSpin_, &QDoubleSpinBox::valueChanged, this, [this](double) {
        if (!silent_)
            emit windowLevelChanged(wwSpin_->value(), wcSpin_->value());
    });
    connect(autoWindowButton_, &QPushButton::clicked, this, [this] {
        // 置零触发自动窗
        silent_ = true;
        wwSpin_->setValue(0.0);
        wcSpin_->setValue(0.0);
        silent_ = false;
        emit windowLevelChanged(0.0, 0.0);
    });

    connect(pseudoCheck_, &QCheckBox::toggled, this, [this](bool on) {
        colorMapCombo_->setEnabled(on);
        emit pseudoColorChanged(on, colorMapCombo_->currentText());
    });
    connect(colorMapCombo_, &QComboBox::currentTextChanged, this, [this](const QString& s) {
        if (pseudoCheck_->isChecked())
            emit pseudoColorChanged(true, s);
    });
    connect(subtractionCheck_, &QCheckBox::toggled, this, [this](bool on) {
        maskFrameSpin_->setEnabled(on);
        maskSigmaSpin_->setEnabled(on);
        emit subtractionChanged(on, maskFrameSpin_->value(), maskSigmaSpin_->value());
    });
    connect(maskFrameSpin_, &QSpinBox::valueChanged, this, [this](int v) {
        if (subtractionCheck_->isChecked())
            emit subtractionChanged(true, v, maskSigmaSpin_->value());
    });
    connect(maskSigmaSpin_, &QDoubleSpinBox::valueChanged, this, [this](double s) {
        if (subtractionCheck_->isChecked())
            emit subtractionChanged(true, maskFrameSpin_->value(), s);
    });

    connect(enhanceButton_, &QPushButton::clicked, this,
            &ControlPanel::enhanceUltrasoundRequested);
    connect(exportImageButton_, &QPushButton::clicked, this,
            &ControlPanel::exportImageRequested);
    connect(exportVideoButton_, &QPushButton::clicked, this,
            &ControlPanel::exportVideoRequested);
}

void ControlPanel::setFrameCount(int count)
{
    const int maxIdx = qMax(0, count - 1);
    frameSlider_->setRange(0, maxIdx);
    frameSpin_->setRange(0, maxIdx);
    frameSpin_->setSuffix(tr(" / %1").arg(count));
    maskFrameSpin_->setRange(0, maxIdx);
}

void ControlPanel::setPatientInfo(const PatientInfo& info)
{
    info_ = info;
    updateInfoLabel();
}

void ControlPanel::updateInfoLabel()
{
    QStringList parts;
    if (!info_.patientId.isEmpty())
        parts << tr("ID: %1").arg(info_.patientId);
    if (!info_.patientName.isEmpty())
        parts << info_.patientName;
    if (!info_.modality.isEmpty())
        parts << info_.modality;
    if (!info_.studyDate.isEmpty())
        parts << info_.studyDate;
    if (!info_.seriesDescription.isEmpty())
        parts << info_.seriesDescription;
    if (info_.pixelSpacingX > 0.0)
        parts << tr("%1×%2 px").arg(info_.width).arg(info_.height)
           + tr(" %1mm").arg(info_.pixelSpacingX, 0, 'f', 3);
    infoLabel_->setText(parts.join(QStringLiteral("\n")));
}

void ControlPanel::setFrameSilently(int frame)
{
    silent_ = true;
    frameSlider_->setValue(frame);
    frameSpin_->setValue(frame);
    silent_ = false;
}

void ControlPanel::setWindowLevelSilently(double ww, double wc)
{
    silent_ = true;
    wwSpin_->setValue(ww);
    wcSpin_->setValue(wc);
    silent_ = false;
}

RenderOptions ControlPanel::renderOptions() const
{
    RenderOptions opt;
    // ww=0（自动窗按钮）触发自动窗
    opt.windowWidth = wwSpin_->value() == 0.0 ? 0.0 : wwSpin_->value();
    opt.windowCenter = wcSpin_->value();
    opt.pseudoColor = pseudoCheck_->isChecked();
    opt.colorMap = colorMapCombo_->currentText();
    opt.subtraction = subtractionCheck_->isChecked();
    opt.maskFrame = maskFrameSpin_->value();
    opt.maskSmoothSigma = maskSigmaSpin_->value();
    return opt;
}

void ControlPanel::play(bool on)
{
    playButton_->setChecked(on);
}
