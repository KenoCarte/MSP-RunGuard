#include "MainWindow.h"

#include <QCoreApplication>
#include <QClipboard>
#include <QCheckBox>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFileSystemWatcher>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPixmap>
#include <QProcess>
#include <QProcessEnvironment>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSizePolicy>
#include <QSettings>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStatusBar>
#include <QScrollArea>
#include <QTextStream>
#include <QVBoxLayout>
#include <QCloseEvent>
#include <QDirIterator>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QColor>
#include <QComboBox>
#include <QTimer>

namespace {
constexpr bool kCompactUi = true;

// 日志高亮器：按关键字给日志行着色，便于快速定位错误/告警。
class LogHighlighter : public QSyntaxHighlighter {
public:
    explicit LogHighlighter(QTextDocument* parent)
        : QSyntaxHighlighter(parent) {
        failFormat_.setForeground(QColor("#c62828"));
        failFormat_.setFontWeight(QFont::Bold);

        warnFormat_.setForeground(QColor("#ef6c00"));
        warnFormat_.setFontWeight(QFont::DemiBold);

        okFormat_.setForeground(QColor("#2e7d32"));
        okFormat_.setFontWeight(QFont::DemiBold);

        infoFormat_.setForeground(QColor("#1565c0"));
    }

protected:
    void highlightBlock(const QString& text) override {
        const QString upper = text.toUpper();
        if (upper.contains("[FAIL]") || upper.contains("[STDERR]") || upper.contains("ERROR")) {
            setFormat(0, text.length(), failFormat_);
            return;
        }
        if (upper.contains("[WARN]") || upper.contains("WARNING")) {
            setFormat(0, text.length(), warnFormat_);
            return;
        }
        if (upper.contains("[OK]") || upper.contains("DONE") || upper.contains("SUCCESS")) {
            setFormat(0, text.length(), okFormat_);
            return;
        }
        if (upper.contains("[INFO]") || upper.contains("[STDOUT]") || upper.contains("[VIDEO]")) {
            setFormat(0, text.length(), infoFormat_);
            return;
        }
    }

private:
    QTextCharFormat failFormat_;
    QTextCharFormat warnFormat_;
    QTextCharFormat okFormat_;
    QTextCharFormat infoFormat_;
};
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    // 初始化 UI 与默认配置。
    buildUi();
    loadDefaults();
    loadUiSettings();

    // 主进程：用于异步执行 Python 推理/分析脚本。
    process_ = new QProcess(this);
    connect(process_, &QProcess::finished, this, &MainWindow::onInferenceFinished);
    connect(process_, &QProcess::readyReadStandardOutput, this, &MainWindow::onProcessStdoutReady);
    connect(process_, &QProcess::readyReadStandardError, this, &MainWindow::onProcessStderrReady);

    // 自动监听 summary.json 变化，实现风险面板自动刷新。
    summaryWatcher_ = new QFileSystemWatcher(this);
    connect(summaryWatcher_, &QFileSystemWatcher::fileChanged, this, &MainWindow::onSummaryFileChanged);
    connect(summaryEdit_, &QLineEdit::textChanged, this, &MainWindow::onSummaryPathChanged);
    refreshSummaryWatcher();

    // 轮询读取后端写出的预览图片，实现应用内实时画面。
    livePreviewTimer_ = new QTimer(this);
    livePreviewTimer_->setInterval(220);
    connect(livePreviewTimer_, &QTimer::timeout, this, &MainWindow::refreshLivePreview);
}

QString MainWindow::findProjectRoot() {
    // 从可执行目录向上回溯，直到找到项目锚点文件。
    QDir dir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 8; ++i) {
        if (QFileInfo::exists(dir.filePath("infer/infer_once.py"))) {
            return dir.absolutePath();
        }
        if (!dir.cdUp()) {
            break;
        }
    }
    return QDir::currentPath();
}

void MainWindow::buildUi() {
    setWindowTitle(QStringLiteral("Pose Risk Analysis Workbench"));
    resize(1180, 820);
    setMinimumSize(1040, 740);

    QWidget* central = new QWidget(this);
    setCentralWidget(central);

    auto* mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(12, 12, 12, 10);
    mainLayout->setSpacing(10);

    auto* configGroup = new QGroupBox(QStringLiteral("Runtime Settings"), this);
    auto* configRootLayout = new QVBoxLayout(configGroup);
    configRootLayout->setContentsMargins(8, 10, 8, 8);
    configRootLayout->setSpacing(8);

    auto* configScroll = new QScrollArea(this);
    configScroll->setWidgetResizable(true);
    configScroll->setFrameShape(QFrame::NoFrame);
    configScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    configScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    configScroll->setMinimumHeight(160);
    configScroll->setMaximumHeight(290);

    auto* configPanelsWidget = new QWidget(this);
    auto* configPanelsLayout = new QVBoxLayout(configPanelsWidget);
    configPanelsLayout->setContentsMargins(0, 0, 0, 0);
    configPanelsLayout->setSpacing(8);

    auto* basicPanel = new QGroupBox(QStringLiteral("Basic Parameters"), this);
    basicPanel->setCheckable(true);
    basicPanel->setChecked(true);
    auto* basicPanelLayout = new QVBoxLayout(basicPanel);
    basicPanelLayout->setContentsMargins(8, 10, 8, 8);
    auto* basicContent = new QWidget(this);
    auto* basicLayout = new QFormLayout(basicContent);
    basicLayout->setHorizontalSpacing(10);
    basicLayout->setVerticalSpacing(6);
    basicPanelLayout->addWidget(basicContent);

    auto* advancedPanel = new QGroupBox(QStringLiteral("Advanced Parameters"), this);
    advancedPanel->setCheckable(true);
    advancedPanel->setChecked(false);
    auto* advancedPanelLayout = new QVBoxLayout(advancedPanel);
    advancedPanelLayout->setContentsMargins(8, 10, 8, 8);
    auto* advancedContent = new QWidget(this);
    auto* advancedLayout = new QFormLayout(advancedContent);
    advancedLayout->setHorizontalSpacing(10);
    advancedLayout->setVerticalSpacing(6);
    advancedPanelLayout->addWidget(advancedContent);
    advancedContent->setVisible(false);

    connect(basicPanel, &QGroupBox::toggled, basicContent, &QWidget::setVisible);
    connect(advancedPanel, &QGroupBox::toggled, advancedContent, &QWidget::setVisible);

    // 统一的“路径输入 + 浏览按钮”行构造器。
    auto buildPathRow = [this](QLineEdit*& edit, const QString& defaultText, const QObject* receiver, const char* member) {
        QWidget* rowWidget = new QWidget(this);
        auto* rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(8);

        edit = new QLineEdit(defaultText, this);
        edit->setMinimumHeight(28);
        auto* btn = new QPushButton(QStringLiteral("Browse"), this);
        btn->setMinimumHeight(28);
        connect(btn, SIGNAL(clicked()), receiver, member);

        rowLayout->addWidget(edit, 1);
        rowLayout->addWidget(btn);
        return rowWidget;
    };

    basicLayout->addRow(QStringLiteral("Python"), buildPathRow(pythonEdit_, "python3", this, SLOT(browsePythonPath())));
    basicLayout->addRow(QStringLiteral("Script"), buildPathRow(scriptEdit_, "", this, SLOT(browseScriptPath())));
    basicLayout->addRow(QStringLiteral("Config"), buildPathRow(cfgEdit_, "", this, SLOT(browseCfgPath())));
    basicLayout->addRow(QStringLiteral("Weight"), buildPathRow(weightEdit_, "", this, SLOT(browseWeightPath())));
    basicLayout->addRow(QStringLiteral("Video"), buildPathRow(videoEdit_, "", this, SLOT(browseVideoPath())));
    basicLayout->addRow(QStringLiteral("Analysis Out Dir"), buildPathRow(analysisOutDirEdit_, "", this, SLOT(browseAnalysisOutDir())));

    advancedLayout->addRow(QStringLiteral("Risk Config"), buildPathRow(riskConfigEdit_, "", this, SLOT(browseRiskConfigPath())));
    advancedLayout->addRow(QStringLiteral("Summary JSON"), buildPathRow(summaryEdit_, "", this, SLOT(browseSummaryPath())));

    auto* inputImageLabel = new QLabel(QStringLiteral("Input Image"), this);
    auto* inputImageRow = buildPathRow(inputEdit_, "", this, SLOT(browseInputImage()));
    auto* outputImageLabel = new QLabel(QStringLiteral("Output Image"), this);
    auto* outputImageRow = buildPathRow(outputEdit_, "", this, SLOT(browseOutputImage()));
    advancedLayout->addRow(inputImageLabel, inputImageRow);
    advancedLayout->addRow(outputImageLabel, outputImageRow);

    auto* videoOptRow = new QWidget(this);
    auto* videoOptLayout = new QHBoxLayout(videoOptRow);
    videoOptLayout->setContentsMargins(0, 0, 0, 0);
    videoOptLayout->setSpacing(8);
    auto* sourceModeLabel = new QLabel(QStringLiteral("Source"), this);
    sourceModeCombo_ = new QComboBox(this);
    sourceModeCombo_->setMinimumHeight(30);
    sourceModeCombo_->addItem(QStringLiteral("Video File"), QStringLiteral("video"));
    sourceModeCombo_->addItem(QStringLiteral("Camera"), QStringLiteral("camera"));
    connect(sourceModeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onSourceModeChanged);
    auto* cameraLabel = new QLabel(QStringLiteral("Camera Index"), this);
    cameraIndexSpin_ = new QSpinBox(this);
    cameraIndexSpin_->setMinimumHeight(30);
    cameraIndexSpin_->setRange(0, 8);
    cameraIndexSpin_->setValue(0);
    auto* maxSecondsLabel = new QLabel(QStringLiteral("Max Seconds"), this);
    maxSecondsSpin_ = new QSpinBox(this);
    maxSecondsSpin_->setMinimumHeight(30);
    maxSecondsSpin_->setRange(1, 600);
    maxSecondsSpin_->setValue(20);
    auto* frameStrideLabel = new QLabel(QStringLiteral("Frame Stride"), this);
    frameStrideSpin_ = new QSpinBox(this);
    frameStrideSpin_->setMinimumHeight(30);
    frameStrideSpin_->setRange(1, 20);
    frameStrideSpin_->setValue(1);
    saveOverlayCheck_ = new QCheckBox(QStringLiteral("Save Overlay Video"), this);
    saveOverlayCheck_->setChecked(true);
    showLiveCheck_ = new QCheckBox(QStringLiteral("Show Live Preview"), this);
    showLiveCheck_->setChecked(false);
    videoOptLayout->addWidget(sourceModeLabel);
    videoOptLayout->addWidget(sourceModeCombo_);
    videoOptLayout->addSpacing(8);
    videoOptLayout->addWidget(cameraLabel);
    videoOptLayout->addWidget(cameraIndexSpin_);
    videoOptLayout->addSpacing(8);
    videoOptLayout->addWidget(maxSecondsLabel);
    videoOptLayout->addWidget(maxSecondsSpin_);
    videoOptLayout->addSpacing(8);
    videoOptLayout->addWidget(frameStrideLabel);
    videoOptLayout->addWidget(frameStrideSpin_);
    videoOptLayout->addSpacing(12);
    videoOptLayout->addWidget(saveOverlayCheck_);
    videoOptLayout->addWidget(showLiveCheck_);
    videoOptLayout->addStretch(1);
    auto* basicActionRow = new QWidget(this);
    auto* basicActionLayout = new QHBoxLayout(basicActionRow);
    basicActionLayout->setContentsMargins(0, 0, 0, 0);
    basicActionLayout->setSpacing(8);

    auto* advancedActionRow = new QWidget(this);
    auto* advancedActionLayout = new QHBoxLayout(advancedActionRow);
    advancedActionLayout->setContentsMargins(0, 0, 0, 0);
    advancedActionLayout->setSpacing(8);

    validateButton_ = new QPushButton(QStringLiteral("Validate Config"), this);
    validateButton_->setMinimumHeight(32);
    connect(validateButton_, &QPushButton::clicked, this, &MainWindow::validateConfiguration);

    batchButton_ = new QPushButton(QStringLiteral("Batch Inference"), this);
    batchButton_->setMinimumHeight(32);
    connect(batchButton_, &QPushButton::clicked, this, &MainWindow::runBatchInference);

    cancelBatchButton_ = new QPushButton(QStringLiteral("Cancel Batch"), this);
    cancelBatchButton_->setMinimumHeight(32);
    cancelBatchButton_->setEnabled(false);
    connect(cancelBatchButton_, &QPushButton::clicked, this, &MainWindow::cancelBatchInference);

    detectCameraButton_ = new QPushButton(QStringLiteral("Detect Camera"), this);
    detectCameraButton_->setMinimumHeight(32);
    connect(detectCameraButton_, &QPushButton::clicked, this, &MainWindow::detectCameraIndex);

    runButton_ = new QPushButton(QStringLiteral("Run Inference"), this);
    runButton_->setMinimumHeight(32);
    connect(runButton_, &QPushButton::clicked, this, &MainWindow::runInference);

    runVideoButton_ = new QPushButton(QStringLiteral("Run Video Analysis"), this);
    runVideoButton_->setObjectName(QStringLiteral("primaryButton"));
    runVideoButton_->setMinimumHeight(32);
    connect(runVideoButton_, &QPushButton::clicked, this, &MainWindow::runVideoAnalysis);

    saveLogButton_ = new QPushButton(QStringLiteral("Save Log"), this);
    saveLogButton_->setMinimumHeight(32);
    connect(saveLogButton_, &QPushButton::clicked, this, &MainWindow::saveLogToFile);

    loadSummaryButton_ = new QPushButton(QStringLiteral("Load Summary"), this);
    loadSummaryButton_->setMinimumHeight(32);
    connect(loadSummaryButton_, &QPushButton::clicked, this, &MainWindow::loadRiskSummary);

    copyAdviceButton_ = new QPushButton(QStringLiteral("Copy Advice"), this);
    copyAdviceButton_->setMinimumHeight(32);
    connect(copyAdviceButton_, &QPushButton::clicked, this, &MainWindow::copyRiskAdvice);

    basicActionLayout->addWidget(detectCameraButton_);
    basicActionLayout->addWidget(copyAdviceButton_);
    basicActionLayout->addStretch(1);
    basicActionLayout->addWidget(runVideoButton_);

    advancedActionLayout->addWidget(validateButton_);
    advancedActionLayout->addWidget(batchButton_);
    advancedActionLayout->addWidget(cancelBatchButton_);
    advancedActionLayout->addWidget(loadSummaryButton_);
    advancedActionLayout->addWidget(runButton_);
    advancedActionLayout->addWidget(saveLogButton_);
    advancedLayout->addRow(QString(), advancedActionRow);

    configPanelsLayout->addWidget(basicPanel);
    configPanelsLayout->addWidget(advancedPanel);
    configPanelsLayout->addStretch(1);

    configScroll->setWidget(configPanelsWidget);
    configRootLayout->addWidget(configScroll);

    auto* quickControlGroup = new QGroupBox(QStringLiteral("Quick Controls"), this);
    auto* quickControlLayout = new QVBoxLayout(quickControlGroup);
    quickControlLayout->setContentsMargins(8, 8, 8, 8);
    quickControlLayout->setSpacing(6);
    quickControlLayout->addWidget(videoOptRow);
    quickControlLayout->addWidget(basicActionRow);
    operationHintLabel_ = new QLabel(QStringLiteral("Status: Ready. Configure source and press Run Video Analysis."), this);
    operationHintLabel_->setStyleSheet("color:#334155; background:#eef5ff; border:1px solid #c7dbff; border-radius:6px; padding:4px 8px;");
    quickControlLayout->addWidget(operationHintLabel_);

    auto* previewGroup = new QGroupBox(QStringLiteral("Preview"), this);
    auto* previewLayout = new QHBoxLayout(previewGroup);

    auto* inputBox = new QGroupBox(QStringLiteral("Input"), this);
    auto* inputBoxLayout = new QVBoxLayout(inputBox);
    inputPreviewLabel_ = new QLabel(QStringLiteral("No input image"), this);
    inputPreviewLabel_->setMinimumSize(420, 240);
    inputPreviewLabel_->setAlignment(Qt::AlignCenter);
    inputPreviewLabel_->setStyleSheet("border:1px solid #ccc; background:#fafafa;");
    inputBoxLayout->addWidget(inputPreviewLabel_);

    auto* outputBox = new QGroupBox(QStringLiteral("Output"), this);
    auto* outputBoxLayout = new QVBoxLayout(outputBox);
    outputPreviewLabel_ = new QLabel(QStringLiteral("No output image"), this);
    outputPreviewLabel_->setMinimumSize(420, 240);
    outputPreviewLabel_->setAlignment(Qt::AlignCenter);
    outputPreviewLabel_->setStyleSheet("border:1px solid #ccc; background:#fafafa;");
    outputBoxLayout->addWidget(outputPreviewLabel_);

    previewLayout->addWidget(inputBox, 1);
    previewLayout->addWidget(outputBox, 1);

    if (kCompactUi) {
        // 紧凑模式只保留当前演示主线（视频/摄像头分析）。
        inputImageLabel->setVisible(false);
        inputImageRow->setVisible(false);
        outputImageLabel->setVisible(false);
        outputImageRow->setVisible(false);
        validateButton_->setVisible(false);
        runButton_->setVisible(false);
        previewGroup->setVisible(false);
        batchButton_->setVisible(false);
        cancelBatchButton_->setVisible(false);
        loadSummaryButton_->setVisible(false);
        saveLogButton_->setVisible(false);
    }

    mainLayout->addWidget(configGroup, 0);
    mainLayout->addWidget(quickControlGroup, 0);

    auto* bottomRow = new QHBoxLayout();
    bottomRow->setSpacing(10);
    mainLayout->addLayout(bottomRow, 1);

    auto* riskGroup = new QGroupBox(QStringLiteral("Assessment & Advice"), this);
    auto* riskLayout = new QFormLayout(riskGroup);
    riskLayout->setHorizontalSpacing(8);
    riskLayout->setVerticalSpacing(3);
    auto* levelKeyLabel = new QLabel(QStringLiteral("Level"), this);
    auto* scoreKeyLabel = new QLabel(QStringLiteral("Score"), this);
    auto* flagsKeyLabel = new QLabel(QStringLiteral("Flags"), this);
    auto* adviceKeyLabel = new QLabel(QStringLiteral("Advice"), this);
    levelKeyLabel->setStyleSheet("font-size: 11px; color: #4b5563;");
    scoreKeyLabel->setStyleSheet("font-size: 11px; color: #4b5563;");
    flagsKeyLabel->setStyleSheet("font-size: 11px; color: #4b5563;");
    adviceKeyLabel->setStyleSheet("font-size: 11px; color: #4b5563;");
    levelKeyLabel->setFixedWidth(44);
    scoreKeyLabel->setFixedWidth(44);
    flagsKeyLabel->setFixedWidth(44);
    adviceKeyLabel->setFixedWidth(44);
    riskLevelValueLabel_ = new QLabel(this);
    riskScoreValueLabel_ = new QLabel(this);
    riskFlagsValueLabel_ = new QLabel(this);
    riskLevelValueLabel_->setStyleSheet("font-size: 11px; font-weight: 700; background:#f3f6fb; border:1px solid #e2e8f0; border-radius:6px; padding:1px 6px;");
    riskScoreValueLabel_->setStyleSheet("font-size: 11px; font-weight: 700; background:#f3f6fb; border:1px solid #e2e8f0; border-radius:6px; padding:1px 6px;");
    riskFlagsValueLabel_->setStyleSheet("font-size: 11px; background:#f8fafc; border:1px solid #e2e8f0; border-radius:6px; padding:1px 6px;");
    riskLevelValueLabel_->setFixedHeight(24);
    riskScoreValueLabel_->setFixedHeight(24);
    riskFlagsValueLabel_->setFixedHeight(24);
    riskFlagsValueLabel_->setWordWrap(true);
    riskLevelValueLabel_->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    riskScoreValueLabel_->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    riskFlagsValueLabel_->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    riskAdviceView_ = new QPlainTextEdit(this);
    riskAdviceView_->setReadOnly(true);
    riskAdviceView_->setMinimumHeight(260);
    riskAdviceView_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    riskLayout->addRow(levelKeyLabel, riskLevelValueLabel_);
    riskLayout->addRow(scoreKeyLabel, riskScoreValueLabel_);
    riskLayout->addRow(flagsKeyLabel, riskFlagsValueLabel_);
    riskLayout->addRow(adviceKeyLabel, riskAdviceView_);

    auto* logGroup = new QGroupBox(QStringLiteral("Run Log"), this);
    auto* logLayout = new QVBoxLayout(logGroup);

    auto* liveGroup = new QGroupBox(QStringLiteral("Live Monitor"), this);
    auto* liveLayout = new QVBoxLayout(liveGroup);
    liveLayout->setContentsMargins(8, 12, 8, 8);
    livePreviewLabel_ = new QLabel(QStringLiteral("Live preview will appear during analysis."), this);
    livePreviewLabel_->setMinimumSize(640, 180);
    livePreviewLabel_->setAlignment(Qt::AlignCenter);
    livePreviewLabel_->setStyleSheet("border:1px solid #d0d7de; background:#ffffff;");
    liveLayout->addWidget(livePreviewLabel_);

    auto* batchStatusRow = new QWidget(this);
    auto* batchStatusLayout = new QHBoxLayout(batchStatusRow);
    batchStatusLayout->setContentsMargins(0, 0, 0, 0);
    batchStatusLayout->setSpacing(8);
    batchStatusLabel_ = new QLabel(QStringLiteral("Batch idle"), this);
    batchProgressBar_ = new QProgressBar(this);
    batchProgressBar_->setRange(0, 100);
    batchProgressBar_->setValue(0);
    batchStatusLayout->addWidget(batchStatusLabel_);
    batchStatusLayout->addWidget(batchProgressBar_, 1);
    logLayout->addWidget(batchStatusRow);

    logView_ = new QPlainTextEdit(this);
    logView_->setReadOnly(true);
    logView_->setMaximumBlockCount(500);
    new LogHighlighter(logView_->document());
    logLayout->addWidget(logView_);

    auto* rightColumn = new QWidget(this);
    auto* rightLayout = new QVBoxLayout(rightColumn);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(10);
    rightLayout->addWidget(liveGroup, 2);
    rightLayout->addWidget(logGroup, 1);

    bottomRow->addWidget(riskGroup, 1);
    bottomRow->addWidget(rightColumn, 2);

    statusBar()->showMessage(QStringLiteral("Ready"));
    setRiskPanelDefaults();
    onSourceModeChanged(sourceModeCombo_ ? sourceModeCombo_->currentIndex() : 0);

    setStyleSheet(
        // 轻量统一主题：提升可读性并减少控件视觉噪声。
        "QWidget { background: #f4f7fb; color: #1f2a37; }"
        "QGroupBox { border: 1px solid #d3dbe7; border-radius: 10px; margin-top: 8px; font-weight: 600; background: #fbfdff; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 6px; }"
        "QLineEdit, QSpinBox, QComboBox, QPlainTextEdit { background: #ffffff; border: 1px solid #c5cfdd; border-radius: 7px; selection-background-color: #93c5fd; }"
        "QPushButton { background: #edf3fb; border: 1px solid #c5d4e8; border-radius: 7px; padding: 4px 10px; }"
        "QPushButton:hover { background: #e2ecfb; }"
        "QPushButton#primaryButton { background: #2563eb; color: #ffffff; border: 1px solid #1d4ed8; font-weight: 700; }"
        "QPushButton#primaryButton:hover { background: #1d4ed8; }"
        "QPushButton:disabled { background: #eef2f7; color: #9ba7b7; border-color: #dbe3ef; }"
        "QProgressBar { border: 1px solid #c8d0dc; border-radius: 7px; text-align: center; background: #ffffff; }"
        "QProgressBar::chunk { background: #3b82f6; border-radius: 6px; }"
    );
}

void MainWindow::loadDefaults() {
    // 当用户尚未配置时，自动填充一组可运行的默认路径。
    const QString root = findProjectRoot();
    if (scriptEdit_->text().isEmpty()) {
        scriptEdit_->setText(QDir(root).filePath("infer/infer_once.py"));
    }
    if (cfgEdit_->text().isEmpty()) {
        cfgEdit_->setText(QDir(root).filePath("experiments/vgg19_368x368_sgd.yaml"));
    }
    if (weightEdit_->text().isEmpty()) {
        weightEdit_->setText(QDir(root).filePath("network/weight/best_pose.pth"));
    }
    if (riskConfigEdit_->text().isEmpty()) {
        riskConfigEdit_->setText(QDir(root).filePath("analysis/risk_config.json"));
    }
    if (summaryEdit_->text().isEmpty()) {
        summaryEdit_->setText(QDir(root).filePath("analysis/summary.json"));
    }

    const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    outputEdit_->setText(QDir(tempDir).filePath("pose_out_qt.jpg"));
    if (analysisOutDirEdit_->text().isEmpty()) {
        analysisOutDirEdit_->setText(QDir(tempDir).filePath(QStringLiteral("pose_analysis_case")));
    }
}

void MainWindow::loadUiSettings() {
    // 从 QSettings 恢复上次会话参数，方便重复实验。
    QSettings settings(QStringLiteral("PoseQtClient"), QStringLiteral("PoseQtClient"));
    const QString python = settings.value(QStringLiteral("python_path")).toString();
    const QString script = settings.value(QStringLiteral("script_path")).toString();
    const QString cfg = settings.value(QStringLiteral("cfg_path")).toString();
    const QString weight = settings.value(QStringLiteral("weight_path")).toString();
    const QString video = settings.value(QStringLiteral("video_path")).toString();
    const QString analysisOutDir = settings.value(QStringLiteral("analysis_out_dir")).toString();
    const QString riskConfig = settings.value(QStringLiteral("risk_config_path")).toString();
    const QString summary = settings.value(QStringLiteral("summary_path")).toString();
    const QString input = settings.value(QStringLiteral("input_path")).toString();
    const QString output = settings.value(QStringLiteral("output_path")).toString();
    const int frameStride = settings.value(QStringLiteral("frame_stride"), 1).toInt();
    const QString sourceMode = settings.value(QStringLiteral("source_mode"), QStringLiteral("video")).toString();
    const int cameraIndex = settings.value(QStringLiteral("camera_index"), 0).toInt();
    const int maxSeconds = settings.value(QStringLiteral("max_seconds"), 20).toInt();
    const bool saveOverlay = settings.value(QStringLiteral("save_overlay"), true).toBool();
    const bool showLive = settings.value(QStringLiteral("show_live"), false).toBool();

    if (!python.isEmpty()) pythonEdit_->setText(python);
    if (!script.isEmpty()) scriptEdit_->setText(script);
    if (!cfg.isEmpty()) cfgEdit_->setText(cfg);
    if (!weight.isEmpty()) weightEdit_->setText(weight);
    if (!video.isEmpty()) videoEdit_->setText(video);
    if (!analysisOutDir.isEmpty()) analysisOutDirEdit_->setText(analysisOutDir);
    if (!riskConfig.isEmpty()) riskConfigEdit_->setText(riskConfig);
    if (!summary.isEmpty()) summaryEdit_->setText(summary);
    if (!input.isEmpty()) inputEdit_->setText(input);
    if (!output.isEmpty()) outputEdit_->setText(output);
    if (frameStrideSpin_) frameStrideSpin_->setValue(frameStride < 1 ? 1 : frameStride);
    if (sourceModeCombo_) {
        const int idx = sourceModeCombo_->findData(sourceMode);
        sourceModeCombo_->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    if (cameraIndexSpin_) cameraIndexSpin_->setValue(cameraIndex < 0 ? 0 : cameraIndex);
    if (maxSecondsSpin_) maxSecondsSpin_->setValue(maxSeconds < 1 ? 20 : maxSeconds);
    if (saveOverlayCheck_) saveOverlayCheck_->setChecked(saveOverlay);
    if (showLiveCheck_) showLiveCheck_->setChecked(showLive);

    if (QFileInfo::exists(inputEdit_->text().trimmed())) {
        showImageToLabel(inputEdit_->text().trimmed(), inputPreviewLabel_);
    }
    if (QFileInfo::exists(outputEdit_->text().trimmed())) {
        showImageToLabel(outputEdit_->text().trimmed(), outputPreviewLabel_);
    }
}

void MainWindow::saveUiSettings() const {
    // 持久化关键路径与模式参数。
    QSettings settings(QStringLiteral("PoseQtClient"), QStringLiteral("PoseQtClient"));
    settings.setValue(QStringLiteral("python_path"), pythonEdit_->text().trimmed());
    settings.setValue(QStringLiteral("script_path"), scriptEdit_->text().trimmed());
    settings.setValue(QStringLiteral("cfg_path"), cfgEdit_->text().trimmed());
    settings.setValue(QStringLiteral("weight_path"), weightEdit_->text().trimmed());
    settings.setValue(QStringLiteral("video_path"), videoEdit_->text().trimmed());
    settings.setValue(QStringLiteral("analysis_out_dir"), analysisOutDirEdit_->text().trimmed());
    settings.setValue(QStringLiteral("risk_config_path"), riskConfigEdit_->text().trimmed());
    settings.setValue(QStringLiteral("summary_path"), summaryEdit_->text().trimmed());
    settings.setValue(QStringLiteral("input_path"), inputEdit_->text().trimmed());
    settings.setValue(QStringLiteral("output_path"), outputEdit_->text().trimmed());
    settings.setValue(QStringLiteral("frame_stride"), frameStrideSpin_ ? frameStrideSpin_->value() : 1);
    settings.setValue(QStringLiteral("source_mode"), sourceModeCombo_ ? sourceModeCombo_->currentData().toString() : QStringLiteral("video"));
    settings.setValue(QStringLiteral("camera_index"), cameraIndexSpin_ ? cameraIndexSpin_->value() : 0);
    settings.setValue(QStringLiteral("max_seconds"), maxSecondsSpin_ ? maxSecondsSpin_->value() : 20);
    settings.setValue(QStringLiteral("save_overlay"), saveOverlayCheck_ ? saveOverlayCheck_->isChecked() : true);
    settings.setValue(QStringLiteral("show_live"), showLiveCheck_ ? showLiveCheck_->isChecked() : false);
}

void MainWindow::setUiRunning(bool running) {
    runButton_->setEnabled(!running);
    if (runVideoButton_) runVideoButton_->setEnabled(!running);
    if (validateButton_) validateButton_->setEnabled(!running);
    if (detectCameraButton_) detectCameraButton_->setEnabled(!running);
    if (batchButton_) batchButton_->setEnabled(!running && !batchCancelRequested_);
    if (running) {
        setOperationHint(QStringLiteral("Status: Running analysis. Please wait for summary output."));
    } else {
        setOperationHint(QStringLiteral("Status: Idle. You can update settings and run again."));
    }
}

void MainWindow::setOperationHint(const QString& hint, bool isError) {
    if (!operationHintLabel_) {
        return;
    }
    operationHintLabel_->setText(hint);
    if (isError) {
        operationHintLabel_->setStyleSheet("color:#991b1b; background:#fee2e2; border:1px solid #fecaca; border-radius:6px; padding:4px 8px;");
    } else {
        operationHintLabel_->setStyleSheet("color:#334155; background:#eef5ff; border:1px solid #c7dbff; border-radius:6px; padding:4px 8px;");
    }
}

QString MainWindow::formatEtaSeconds(qint64 seconds) {
    if (seconds < 0) {
        return QStringLiteral("--:--");
    }
    const qint64 mm = seconds / 60;
    const qint64 ss = seconds % 60;
    return QStringLiteral("%1:%2")
        .arg(mm, 2, 10, QLatin1Char('0'))
        .arg(ss, 2, 10, QLatin1Char('0'));
}

bool MainWindow::validatePaths(QString* errorMessage) const {
    // 基础路径校验：提前失败，避免启动后才报错。
    const QString pythonPath = pythonEdit_->text().trimmed();
    const QString scriptPath = scriptEdit_->text().trimmed();
    const QString cfgPath = cfgEdit_->text().trimmed();
    const QString weightPath = weightEdit_->text().trimmed();

    if (pythonPath.isEmpty() || scriptPath.isEmpty() || cfgPath.isEmpty() || weightPath.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("Please fill Python/Script/Config/Weight.");
        return false;
    }

    if (!QFileInfo::exists(scriptPath)) {
        if (errorMessage) *errorMessage = QStringLiteral("Script not found: %1").arg(scriptPath);
        return false;
    }
    if (!QFileInfo::exists(cfgPath)) {
        if (errorMessage) *errorMessage = QStringLiteral("Config not found: %1").arg(cfgPath);
        return false;
    }
    if (!QFileInfo::exists(weightPath)) {
        if (errorMessage) *errorMessage = QStringLiteral("Weight not found: %1").arg(weightPath);
        return false;
    }
    return true;
}

QStringList MainWindow::buildInferenceArgs(const QString& inputPath, const QString& outputPath) const {
    QStringList args;
    args << scriptEdit_->text().trimmed()
         << "--cfg" << cfgEdit_->text().trimmed()
         << "--weight" << weightEdit_->text().trimmed()
         << "--input" << inputPath
         << "--output" << outputPath
         << "--device" << "auto";
    return args;
}

void MainWindow::appendLog(const QString& line) {
    if (!logView_) return;
    logView_->appendPlainText(line);
}

void MainWindow::setRiskPanelDefaults() {
    if (riskLevelValueLabel_) riskLevelValueLabel_->setText(QStringLiteral("N/A"));
    if (riskLevelValueLabel_) riskLevelValueLabel_->setStyleSheet(QString());
    if (riskScoreValueLabel_) riskScoreValueLabel_->setText(QStringLiteral("N/A"));
    if (riskFlagsValueLabel_) riskFlagsValueLabel_->setText(QStringLiteral("N/A"));
    if (riskAdviceView_) {
        riskAdviceView_->setPlainText(
            QStringLiteral("No risk summary loaded.\n"
                           "Run video analysis first, then the panel will show\n"
                           "1) risk overview\n"
                           "2) key movement issues\n"
                           "3) actionable training advice."));
    }
}

void MainWindow::showImageToLabel(const QString& imagePath, QLabel* targetLabel) {
    QImageReader reader(imagePath);
    const QImage image = reader.read();
    if (image.isNull()) {
        targetLabel->setText(QStringLiteral("Cannot read image\n%1").arg(imagePath));
        targetLabel->setPixmap(QPixmap());
        return;
    }

    const QPixmap pix = QPixmap::fromImage(image).scaled(
        targetLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    targetLabel->setPixmap(pix);
}

void MainWindow::refreshLivePreview() {
    // 实时预览由后端周期写图，前端这里只负责读取和缩放展示。
    if (livePreviewPath_.isEmpty() || !livePreviewLabel_) {
        return;
    }
    const QFileInfo fi(livePreviewPath_);
    if (!fi.exists()) {
        return;
    }

    QImageReader reader(livePreviewPath_);
    const QImage image = reader.read();
    if (image.isNull()) {
        return;
    }

    const QPixmap pix = QPixmap::fromImage(image).scaled(
        livePreviewLabel_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    livePreviewLabel_->setPixmap(pix);
}

void MainWindow::browseInputImage() {
    const QString file = QFileDialog::getOpenFileName(
        this, QStringLiteral("Select input image"), inputEdit_->text(),
        QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.webp)"));
    if (!file.isEmpty()) {
        inputEdit_->setText(file);
        showImageToLabel(file, inputPreviewLabel_);
    }
}

void MainWindow::browseVideoPath() {
    const QString file = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Select running video"),
        videoEdit_->text(),
        QStringLiteral("Videos (*.mp4 *.avi *.mov *.mkv *.webm)"));
    if (!file.isEmpty()) {
        videoEdit_->setText(file);
    }
}

void MainWindow::browseAnalysisOutDir() {
    const QString dir = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("Select analysis output directory"),
        analysisOutDirEdit_->text());
    if (!dir.isEmpty()) {
        analysisOutDirEdit_->setText(dir);
    }
}

void MainWindow::browseRiskConfigPath() {
    const QString file = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Select risk_config.json"),
        riskConfigEdit_->text(),
        QStringLiteral("JSON (*.json)"));
    if (!file.isEmpty()) {
        riskConfigEdit_->setText(file);
    }
}

void MainWindow::browseOutputImage() {
    const QString file = QFileDialog::getSaveFileName(
        this, QStringLiteral("Select output image"), outputEdit_->text(),
        QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp)"));
    if (!file.isEmpty()) {
        outputEdit_->setText(file);
    }
}

void MainWindow::browsePythonPath() {
    const QString file = QFileDialog::getOpenFileName(this, QStringLiteral("Select Python executable"), pythonEdit_->text());
    if (!file.isEmpty()) {
        pythonEdit_->setText(file);
    }
}

void MainWindow::browseScriptPath() {
    const QString file = QFileDialog::getOpenFileName(
        this, QStringLiteral("Select infer_once.py"), scriptEdit_->text(),
        QStringLiteral("Python (*.py)"));
    if (!file.isEmpty()) {
        scriptEdit_->setText(file);
    }
}

void MainWindow::browseCfgPath() {
    const QString file = QFileDialog::getOpenFileName(
        this, QStringLiteral("Select cfg yaml"), cfgEdit_->text(),
        QStringLiteral("YAML (*.yaml *.yml)"));
    if (!file.isEmpty()) {
        cfgEdit_->setText(file);
    }
}

void MainWindow::browseWeightPath() {
    const QString file = QFileDialog::getOpenFileName(
        this, QStringLiteral("Select weight file"), weightEdit_->text(),
        QStringLiteral("PyTorch Weights (*.pth *.pt)"));
    if (!file.isEmpty()) {
        weightEdit_->setText(file);
    }
}

void MainWindow::browseSummaryPath() {
    const QString file = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Select summary.json"),
        summaryEdit_->text(),
        QStringLiteral("JSON (*.json)"));
    if (!file.isEmpty()) {
        summaryEdit_->setText(file);
        refreshSummaryWatcher();
    }
}

void MainWindow::runInference() {
    if (process_->state() != QProcess::NotRunning) {
        QMessageBox::information(this, QStringLiteral("Info"), QStringLiteral("Inference is already running."));
        return;
    }

    const QString pythonPath = pythonEdit_->text().trimmed();
    const QString scriptPath = scriptEdit_->text().trimmed();
    const QString cfgPath = cfgEdit_->text().trimmed();
    const QString weightPath = weightEdit_->text().trimmed();
    const QString inputPath = inputEdit_->text().trimmed();
    const QString outputPath = outputEdit_->text().trimmed();

    QString error;
    if (!validatePaths(&error)) {
        QMessageBox::warning(this, QStringLiteral("Invalid arguments"), error);
        return;
    }

    if (inputPath.isEmpty() || outputPath.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Invalid arguments"), QStringLiteral("Input/Output image path is required for single-image inference."));
        return;
    }
    if (!QFileInfo::exists(inputPath)) {
        QMessageBox::warning(this, QStringLiteral("Invalid path"), QStringLiteral("Input image does not exist."));
        return;
    }

    QDir().mkpath(QFileInfo(outputPath).absolutePath());
    const QStringList args = buildInferenceArgs(inputPath, outputPath);

    // 无缓冲输出，保证日志实时显示到 GUI。
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PYTHONUNBUFFERED", "1");
    process_->setProcessEnvironment(env);
    QDir workDir = QFileInfo(scriptPath).absoluteDir();
    workDir.cdUp();
    process_->setWorkingDirectory(workDir.absolutePath());

    setUiRunning(true);
    statusBar()->showMessage(QStringLiteral("Running inference..."));
    inferTimer_.start();
    taskMode_ = TaskMode::ImageInference;
    processStdoutBuffer_.clear();
    processStderrBuffer_.clear();
    saveUiSettings();
    appendLog(QStringLiteral("[RUN] input=%1 output=%2").arg(inputPath, outputPath));

    process_->start(pythonPath, args);
}

void MainWindow::runVideoAnalysis() {
    if (process_->state() != QProcess::NotRunning) {
        QMessageBox::information(this, QStringLiteral("Info"), QStringLiteral("A process is already running."));
        return;
    }

    const QString pythonPath = pythonEdit_->text().trimmed();
    const QString cfgPath = cfgEdit_->text().trimmed();
    const QString weightPath = weightEdit_->text().trimmed();
    const QString sourceMode = sourceModeCombo_ ? sourceModeCombo_->currentData().toString() : QStringLiteral("video");
    const QString videoPath = videoEdit_->text().trimmed();
    const int cameraIndex = cameraIndexSpin_ ? cameraIndexSpin_->value() : 0;
    const int maxSeconds = maxSecondsSpin_ ? maxSecondsSpin_->value() : 20;
    QString outDir = analysisOutDirEdit_->text().trimmed();
    const QString riskConfigPath = riskConfigEdit_->text().trimmed();

    if (pythonPath.isEmpty() || cfgPath.isEmpty() || weightPath.isEmpty() || outDir.isEmpty()) {
        setOperationHint(QStringLiteral("Status: Missing required runtime paths."), true);
        QMessageBox::warning(this, QStringLiteral("Invalid arguments"), QStringLiteral("Please fill Python/cfg/weight/out-dir."));
        return;
    }
    if ((pythonPath.contains('/') || pythonPath.contains('\\')) && !QFileInfo::exists(pythonPath)) {
        setOperationHint(QStringLiteral("Status: Python executable path is invalid."), true);
        QMessageBox::warning(this, QStringLiteral("Invalid Python"), QStringLiteral("Python executable does not exist."));
        return;
    }

    // Preflight dependency check to provide actionable guidance before starting long jobs.
    QProcess depProbe;
    depProbe.setProcessEnvironment(QProcessEnvironment::systemEnvironment());
    depProbe.start(pythonPath, QStringList() << "-c" << "import torch,cv2,numpy;print('deps-ok')");
    if (!depProbe.waitForStarted(8000)) {
        setOperationHint(QStringLiteral("Status: Failed to start Python preflight check."), true);
        QMessageBox::warning(this, QStringLiteral("Preflight failed"), QStringLiteral("Cannot start Python process for dependency check."));
        return;
    }
    if (!depProbe.waitForFinished(15000) || depProbe.exitCode() != 0) {
        const QString depErr = QString::fromLocal8Bit(depProbe.readAllStandardError()).trimmed();
        appendLog(QStringLiteral("[VIDEO][PRECHECK][FAIL] %1").arg(depErr));
        setOperationHint(QStringLiteral("Status: Dependency precheck failed. Check log for missing package."), true);
        QMessageBox::warning(
            this,
            QStringLiteral("Dependency check failed"),
            QStringLiteral("Python dependencies are not ready.\n\nDetails:\n%1").arg(depErr.isEmpty() ? QStringLiteral("Unknown import error.") : depErr));
        return;
    }

    if (!QFileInfo::exists(cfgPath) || !QFileInfo::exists(weightPath)) {
        setOperationHint(QStringLiteral("Status: cfg/weight file path invalid."), true);
        QMessageBox::warning(this, QStringLiteral("Invalid path"), QStringLiteral("cfg/weight path contains non-existing item."));
        return;
    }
    if (sourceMode == QStringLiteral("video") && (videoPath.isEmpty() || !QFileInfo::exists(videoPath))) {
        setOperationHint(QStringLiteral("Status: Video source is missing or invalid."), true);
        QMessageBox::warning(this, QStringLiteral("Invalid video"), QStringLiteral("Video path is empty or does not exist."));
        return;
    }
    if (sourceMode == QStringLiteral("camera")) {
        const QString devPath = QStringLiteral("/dev/video%1").arg(cameraIndex);
        if (!QFileInfo::exists(devPath)) {
            QMessageBox::warning(
                this,
                QStringLiteral("Camera unavailable"),
                QStringLiteral("Camera device %1 not found.\n\n"
                               "If you are in WSL, webcam may not be passed through by default.\n"
                               "Try one of these:\n"
                               "1) Use Video File mode; or\n"
                               "2) Enable WSL camera/device passthrough and ensure /dev/video* exists.")
                    .arg(devPath));
            appendLog(QStringLiteral("[VIDEO][WARN] camera device missing: %1").arg(devPath));
            setOperationHint(QStringLiteral("Status: Camera device is unavailable in current environment."), true);
            return;
        }
    }
    if (!riskConfigPath.isEmpty() && !QFileInfo::exists(riskConfigPath)) {
        setOperationHint(QStringLiteral("Status: risk config path invalid."), true);
        QMessageBox::warning(this, QStringLiteral("Invalid risk config"), QStringLiteral("risk config file does not exist."));
        return;
    }

    const QString root = findProjectRoot();
    const QString analysisScript = QDir(root).filePath("analysis/run_temporal_analysis.py");
    if (!QFileInfo::exists(analysisScript)) {
        setOperationHint(QStringLiteral("Status: analysis script missing."), true);
        QMessageBox::warning(this, QStringLiteral("Missing script"), QStringLiteral("analysis/run_temporal_analysis.py not found."));
        return;
    }

    // If the selected output directory already contains files, create a timestamped subfolder.
    QDir baseOut(outDir);
    if (baseOut.exists()) {
        const QStringList entries = baseOut.entryList(QDir::NoDotAndDotDot | QDir::AllEntries);
        if (!entries.isEmpty()) {
            outDir = baseOut.filePath(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
        }
    }
    QDir().mkpath(outDir);
    analysisOutDirEdit_->setText(outDir);
    // 与后端约定的实时预览输出文件。
    livePreviewPath_ = QDir(outDir).filePath("live_preview.jpg");

    QStringList args;
    args << analysisScript
         << "--cfg" << cfgPath
         << "--weight" << weightPath
         << "--out-dir" << outDir
         << "--device" << "auto"
         << "--frame-stride" << QString::number(frameStrideSpin_ ? frameStrideSpin_->value() : 1);
    if (sourceMode == QStringLiteral("video")) {
        args << "--video" << videoPath;
    } else {
        args << "--camera-index" << QString::number(cameraIndex)
             << "--max-seconds" << QString::number(maxSeconds);
    }
    if (!riskConfigPath.isEmpty()) {
        args << "--risk-config" << riskConfigPath;
    }
    if (saveOverlayCheck_ && saveOverlayCheck_->isChecked()) {
        args << "--save-overlay-video";
    }
    if (showLiveCheck_ && showLiveCheck_->isChecked() && sourceMode == QStringLiteral("camera")) {
        args << "--show-live";
    }
    args << "--preview-image" << livePreviewPath_
         << "--preview-interval" << "3";

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PYTHONUNBUFFERED", "1");
    process_->setProcessEnvironment(env);
    process_->setWorkingDirectory(root);

    taskMode_ = TaskMode::VideoAnalysis;
    inferTimer_.start();
    setUiRunning(true);
    setRiskPanelDefaults();
    processStdoutBuffer_.clear();
    processStderrBuffer_.clear();
    if (batchProgressBar_) {
        // Busy indicator for long-running video analysis when frame-level progress is unknown.
        batchProgressBar_->setRange(0, 0);
    }
    if (batchStatusLabel_) {
        batchStatusLabel_->setText(QStringLiteral("Video analysis running..."));
    }
    if (livePreviewLabel_) {
        livePreviewLabel_->setText(QStringLiteral("Waiting for preview frames..."));
        livePreviewLabel_->setPixmap(QPixmap());
    }
    if (livePreviewTimer_) {
        livePreviewTimer_->start();
    }
    saveUiSettings();
    appendLog(QStringLiteral("[VIDEO][RUN] source=%1 input=%2 out_dir=%3 stride=%4 max_seconds=%5")
                  .arg(sourceMode)
                  .arg(sourceMode == QStringLiteral("video") ? videoPath : QStringLiteral("camera:%1").arg(cameraIndex))
                  .arg(outDir)
                  .arg(frameStrideSpin_ ? frameStrideSpin_->value() : 1)
                  .arg(maxSeconds));
    setOperationHint(QStringLiteral("Status: Video analysis started. Waiting for live preview and summary..."));
    if (showLiveCheck_ && showLiveCheck_->isChecked() && sourceMode == QStringLiteral("camera")) {
        appendLog(QStringLiteral("[VIDEO] live preview enabled (press q in preview window to stop early)."));
    }
    statusBar()->showMessage(QStringLiteral("Running video temporal analysis..."));

    process_->start(pythonPath, args);
}

void MainWindow::onSourceModeChanged(int) {
    const QString sourceMode = sourceModeCombo_ ? sourceModeCombo_->currentData().toString() : QStringLiteral("video");
    const bool isCamera = (sourceMode == QStringLiteral("camera"));

    if (showLiveCheck_) {
        showLiveCheck_->setEnabled(isCamera);
        if (!isCamera) {
            showLiveCheck_->setChecked(false);
            showLiveCheck_->setToolTip(QStringLiteral("Live preview is available only in Camera mode."));
        } else {
            showLiveCheck_->setToolTip(QStringLiteral("Show realtime overlay window during camera analysis."));
        }
    }
}

void MainWindow::detectCameraIndex() {
    const QString pythonPath = pythonEdit_ ? pythonEdit_->text().trimmed() : QString();
    if (pythonPath.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Detect Camera"), QStringLiteral("Please set Python path first."));
        return;
    }

    QString probeCode;
    // 通过 open+read 双判定过滤 metadata 节点，减少“能打开但无图像帧”的误判。
    probeCode += "import cv2\n";
    probeCode += "ok=[]\n";
    probeCode += "for i in range(9):\n";
    probeCode += "    c=cv2.VideoCapture(i, cv2.CAP_V4L2)\n";
    probeCode += "    o=c.isOpened()\n";
    probeCode += "    r=False\n";
    probeCode += "    w=0\n";
    probeCode += "    h=0\n";
    probeCode += "    if o:\n";
    probeCode += "        r,f=c.read()\n";
    probeCode += "        if r and f is not None:\n";
    probeCode += "            h,w=f.shape[:2]\n";
    probeCode += "    c.release()\n";
    probeCode += "    print(f'{i}:open={o},read={r},w={w},h={h}')\n";
    probeCode += "    if o and r and w>0 and h>0:\n";
    probeCode += "        ok.append(i)\n";
    probeCode += "print('USABLE=' + (','.join(map(str,ok)) if ok else 'NONE'))\n";

    QProcess p;
    p.setProcessEnvironment(QProcessEnvironment::systemEnvironment());
    p.start(pythonPath, QStringList() << "-c" << probeCode);

    if (!p.waitForStarted(8000)) {
        QMessageBox::warning(this, QStringLiteral("Detect Camera"), QStringLiteral("Failed to start Python process."));
        return;
    }
    p.waitForFinished(30000);

    const QString out = QString::fromLocal8Bit(p.readAllStandardOutput());
    const QString err = QString::fromLocal8Bit(p.readAllStandardError());

    if (!err.trimmed().isEmpty()) {
        appendLog(QStringLiteral("[CAM_DETECT][STDERR] %1").arg(err.trimmed()));
    }
    if (!out.trimmed().isEmpty()) {
        for (const QString& ln : out.split('\n')) {
            const QString line = ln.trimmed();
            if (!line.isEmpty()) {
                appendLog(QStringLiteral("[CAM_DETECT] %1").arg(line));
            }
        }
    }

    QRegularExpression usableRx(QStringLiteral("USABLE=([0-9,]+|NONE)"));
    QRegularExpressionMatch m = usableRx.match(out);
    if (!m.hasMatch()) {
        QMessageBox::warning(this, QStringLiteral("Detect Camera"), QStringLiteral("Detection output parse failed. Check Run Log."));
        return;
    }

    const QString usable = m.captured(1);
    if (usable == QStringLiteral("NONE")) {
        QMessageBox::information(this, QStringLiteral("Detect Camera"), QStringLiteral("No usable camera index found (open+read).\nCheck WSL camera passthrough and permissions."));
        return;
    }

    const QString first = usable.split(',').first();
    bool ok = false;
    const int idx = first.toInt(&ok);
    if (!ok) {
        QMessageBox::warning(this, QStringLiteral("Detect Camera"), QStringLiteral("Failed to parse detected camera index."));
        return;
    }

    if (cameraIndexSpin_) {
        cameraIndexSpin_->setValue(idx);
    }
    QMessageBox::information(this, QStringLiteral("Detect Camera"), QStringLiteral("Detected usable index: %1\nAll usable: %2").arg(idx).arg(usable));
    statusBar()->showMessage(QStringLiteral("Camera index set to %1").arg(idx), 4000);
}

void MainWindow::onProcessStdoutReady() {
    if (!process_) return;
    const QString chunk = QString::fromLocal8Bit(process_->readAllStandardOutput());
    if (chunk.isEmpty()) return;

    processStdoutBuffer_.append(chunk);

    // 采用“按行刷新”策略，避免半行日志打断界面显示。
    QString pending = processStdoutBuffer_;
    const int lastNewline = pending.lastIndexOf('\n');
    if (lastNewline < 0) {
        return;
    }

    const QString flushed = pending.left(lastNewline);
    processStdoutBuffer_ = pending.mid(lastNewline + 1);

    const QStringList lines = flushed.split('\n');
    for (const QString& raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty()) continue;
        if (taskMode_ == TaskMode::VideoAnalysis) {
            appendLog(QStringLiteral("[VIDEO][STDOUT] %1").arg(line));

            QRegularExpression rxProcessed(QStringLiteral("processed_frames=([0-9]+)"));
            QRegularExpressionMatch m = rxProcessed.match(line);
            if (m.hasMatch()) {
                const QString frames = m.captured(1);
                if (batchStatusLabel_) {
                    batchStatusLabel_->setText(QStringLiteral("Video analysis running... processed_frames=%1").arg(frames));
                }
                statusBar()->showMessage(QStringLiteral("Video running: processed_frames=%1").arg(frames));
            }
        } else {
            appendLog(QStringLiteral("[STDOUT] %1").arg(line));
        }
    }
}

void MainWindow::onProcessStderrReady() {
    if (!process_) return;
    const QString chunk = QString::fromLocal8Bit(process_->readAllStandardError());
    if (chunk.isEmpty()) return;

    processStderrBuffer_.append(chunk);

    // stderr 与 stdout 同样按行缓冲，保证日志结构一致。
    QString pending = processStderrBuffer_;
    const int lastNewline = pending.lastIndexOf('\n');
    if (lastNewline < 0) {
        return;
    }

    const QString flushed = pending.left(lastNewline);
    processStderrBuffer_ = pending.mid(lastNewline + 1);

    const QStringList lines = flushed.split('\n');
    for (const QString& raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty()) continue;
        if (taskMode_ == TaskMode::VideoAnalysis) {
            appendLog(QStringLiteral("[VIDEO][STDERR] %1").arg(line));
        } else {
            appendLog(QStringLiteral("[STDERR] %1").arg(line));
        }
    }
}

void MainWindow::validateConfiguration() {
    QString error;
    if (!validatePaths(&error)) {
        QMessageBox::warning(this, QStringLiteral("Validation failed"), error);
        appendLog(QStringLiteral("[CHECK][FAIL] %1").arg(error));
        return;
    }

    appendLog(QStringLiteral("[CHECK][OK] script=%1").arg(scriptEdit_->text().trimmed()));
    appendLog(QStringLiteral("[CHECK][OK] cfg=%1").arg(cfgEdit_->text().trimmed()));
    appendLog(QStringLiteral("[CHECK][OK] weight=%1").arg(weightEdit_->text().trimmed()));
    appendLog(QStringLiteral("[CHECK][OK] input=%1").arg(inputEdit_->text().trimmed()));
    QMessageBox::information(this, QStringLiteral("Validation"), QStringLiteral("All required paths are valid."));
}

void MainWindow::saveLogToFile() {
    if (!logView_ || logView_->toPlainText().trimmed().isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Info"), QStringLiteral("No log content to save."));
        return;
    }

    const QString suggested = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                                  .filePath(QStringLiteral("pose_qt_log.txt"));
    const QString path = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("Save run log"),
        suggested,
        QStringLiteral("Text (*.txt);;All files (*)"));
    if (path.isEmpty()) {
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("Save failed"), QStringLiteral("Cannot open file: %1").arg(path));
        return;
    }

    QTextStream out(&file);
    out << logView_->toPlainText();
    file.close();
    statusBar()->showMessage(QStringLiteral("Log saved: %1").arg(path), 5000);
}

void MainWindow::runBatchInference() {
    if (process_->state() != QProcess::NotRunning) {
        QMessageBox::information(this, QStringLiteral("Info"), QStringLiteral("Inference is already running."));
        return;
    }

    QString error;
    if (!validatePaths(&error)) {
        QMessageBox::warning(this, QStringLiteral("Invalid arguments"), error);
        return;
    }

    const QString inDir = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("Select input folder"),
        QFileInfo(inputEdit_->text().trimmed()).absolutePath());
    if (inDir.isEmpty()) return;

    const QString outDir = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("Select output folder"),
        QFileInfo(outputEdit_->text().trimmed()).absolutePath());
    if (outDir.isEmpty()) return;

    QStringList nameFilters;
    nameFilters << "*.jpg" << "*.jpeg" << "*.png" << "*.bmp" << "*.webp";
    QDirIterator it(inDir, nameFilters, QDir::Files, QDirIterator::Subdirectories);
    QStringList files;
    while (it.hasNext()) files << it.next();

    if (files.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Batch"), QStringLiteral("No images found in selected input folder."));
        return;
    }

    appendLog(QStringLiteral("[BATCH] start total=%1 in=%2 out=%3").arg(files.size()).arg(inDir, outDir));
    batchCancelRequested_ = false;
    if (cancelBatchButton_) cancelBatchButton_->setEnabled(true);
    if (batchButton_) batchButton_->setEnabled(false);
    if (batchProgressBar_) {
        batchProgressBar_->setRange(0, files.size());
        batchProgressBar_->setValue(0);
    }
    if (batchStatusLabel_) {
        batchStatusLabel_->setText(QStringLiteral("Batch running: 0/%1, ETA --:--").arg(files.size()));
    }
    setUiRunning(true);
    saveUiSettings();
    QElapsedTimer batchTimer;
    batchTimer.start();

    const QString pythonPath = pythonEdit_->text().trimmed();
    const QString scriptPath = scriptEdit_->text().trimmed();
    QDir workDir = QFileInfo(scriptPath).absoluteDir();
    workDir.cdUp();
    const QDir inputRoot(inDir);
    const QDir outputRoot(outDir);

    int success = 0;
    int failed = 0;
    for (int i = 0; i < files.size(); ++i) {
        if (batchCancelRequested_) {
            appendLog(QStringLiteral("[BATCH] canceled by user at %1/%2").arg(i).arg(files.size()));
            break;
        }

        const QString inputPath = files.at(i);
        const QFileInfo fi(inputPath);

        const QString relPath = inputRoot.relativeFilePath(inputPath);
        const QFileInfo relFi(relPath);
        const QString relDir = relFi.path() == QStringLiteral(".") ? QString() : relFi.path();
        const QString outFileName = fi.completeBaseName() + QStringLiteral("_pose.") + fi.suffix();
        const QString outputPath = relDir.isEmpty()
            ? outputRoot.filePath(outFileName)
            : outputRoot.filePath(QDir(relDir).filePath(outFileName));
        // 输出目录保持输入相对层级，便于回溯原始样本。
        QDir().mkpath(QFileInfo(outputPath).absolutePath());

        QProcess batchProc;
        activeBatchProcess_ = &batchProc;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("PYTHONUNBUFFERED", "1");
        batchProc.setProcessEnvironment(env);
        batchProc.setWorkingDirectory(workDir.absolutePath());

        const QStringList args = buildInferenceArgs(inputPath, outputPath);
        batchProc.start(pythonPath, args);
        if (!batchProc.waitForStarted(10000)) {
            activeBatchProcess_ = nullptr;
            ++failed;
            appendLog(QStringLiteral("[BATCH][FAIL][%1/%2] start failed: %3").arg(i + 1).arg(files.size()).arg(inputPath));
            if (batchProgressBar_) batchProgressBar_->setValue(i + 1);
            continue;
        }

        while (batchProc.state() != QProcess::NotRunning) {
            if (batchCancelRequested_) {
                batchProc.terminate();
                if (!batchProc.waitForFinished(1000)) {
                    batchProc.kill();
                    batchProc.waitForFinished(1000);
                }
                break;
            }
            batchProc.waitForFinished(120);
            QCoreApplication::processEvents();
        }
        activeBatchProcess_ = nullptr;

        const QString out = QString::fromLocal8Bit(batchProc.readAllStandardOutput()).trimmed();
        const QString err = QString::fromLocal8Bit(batchProc.readAllStandardError()).trimmed();

        if (batchProc.exitStatus() == QProcess::NormalExit && batchProc.exitCode() == 0) {
            ++success;
            appendLog(QStringLiteral("[BATCH][OK][%1/%2] %3 => %4").arg(i + 1).arg(files.size()).arg(inputPath, outputPath));
            if (!out.isEmpty()) appendLog(QStringLiteral("  stdout: %1").arg(out));
        } else {
            ++failed;
            appendLog(QStringLiteral("[BATCH][FAIL][%1/%2] %3").arg(i + 1).arg(files.size()).arg(inputPath));
            if (!out.isEmpty()) appendLog(QStringLiteral("  stdout: %1").arg(out));
            if (!err.isEmpty()) appendLog(QStringLiteral("  stderr: %1").arg(err));
        }

        if (batchProgressBar_) batchProgressBar_->setValue(i + 1);

        const int processed = i + 1;
        const qint64 elapsedMs = batchTimer.elapsed();
        const qint64 avgMs = processed > 0 ? elapsedMs / processed : 0;
        const qint64 etaSec = ((files.size() - processed) * avgMs) / 1000;
        if (batchStatusLabel_) {
            batchStatusLabel_->setText(
                QStringLiteral("Batch running: %1/%2, ETA %3")
                    .arg(processed)
                    .arg(files.size())
                    .arg(formatEtaSeconds(etaSec)));
        }

        QCoreApplication::processEvents();
    }

    setUiRunning(false);
    if (cancelBatchButton_) cancelBatchButton_->setEnabled(false);
    if (batchButton_) batchButton_->setEnabled(true);
    activeBatchProcess_ = nullptr;

    if (batchCancelRequested_) {
        statusBar()->showMessage(QStringLiteral("Batch canceled. success=%1 failed=%2").arg(success).arg(failed), 8000);
        appendLog(QStringLiteral("[BATCH] canceled success=%1 failed=%2").arg(success).arg(failed));
        if (batchStatusLabel_) {
            batchStatusLabel_->setText(QStringLiteral("Batch canceled: success=%1 failed=%2").arg(success).arg(failed));
        }
        batchCancelRequested_ = false;
        return;
    }

    statusBar()->showMessage(QStringLiteral("Batch done. success=%1 failed=%2").arg(success).arg(failed), 8000);
    appendLog(QStringLiteral("[BATCH] done success=%1 failed=%2").arg(success).arg(failed));
    if (batchStatusLabel_) {
        batchStatusLabel_->setText(QStringLiteral("Batch done: success=%1 failed=%2").arg(success).arg(failed));
    }

    if (success > 0) {
        QMessageBox::information(this, QStringLiteral("Batch completed"), QStringLiteral("success=%1, failed=%2").arg(success).arg(failed));
    } else {
        QMessageBox::warning(this, QStringLiteral("Batch completed"), QStringLiteral("All batch jobs failed."));
    }
    batchCancelRequested_ = false;
}

void MainWindow::cancelBatchInference() {
    if (!cancelBatchButton_ || !cancelBatchButton_->isEnabled()) {
        return;
    }

    batchCancelRequested_ = true;
    appendLog(QStringLiteral("[BATCH] cancel requested"));
    statusBar()->showMessage(QStringLiteral("Cancel requested..."));
    if (batchStatusLabel_) {
        batchStatusLabel_->setText(QStringLiteral("Cancel requested..."));
    }

    if (activeBatchProcess_ && activeBatchProcess_->state() != QProcess::NotRunning) {
        activeBatchProcess_->terminate();
        if (!activeBatchProcess_->waitForFinished(1000)) {
            activeBatchProcess_->kill();
            activeBatchProcess_->waitForFinished(1000);
        }
    }
}

void MainWindow::loadRiskSummary() {
    const QString summaryPath = summaryEdit_->text().trimmed();
    loadRiskSummaryFromPath(summaryPath, true);
}

void MainWindow::onSummaryPathChanged(const QString&) {
    refreshSummaryWatcher();
}

void MainWindow::onSummaryFileChanged(const QString& path) {
    // Re-add watch path because some editors replace file atomically.
    if (summaryWatcher_ && QFileInfo::exists(path) && !summaryWatcher_->files().contains(path)) {
        summaryWatcher_->addPath(path);
    }
    if (summaryEdit_ && summaryEdit_->text().trimmed() == path) {
        loadRiskSummaryFromPath(path, false);
    }
}

void MainWindow::refreshSummaryWatcher() {
    // 刷新监听列表，避免旧路径残留。
    if (!summaryWatcher_) return;
    const QStringList existing = summaryWatcher_->files();
    if (!existing.isEmpty()) {
        summaryWatcher_->removePaths(existing);
    }
    const QString path = summaryEdit_ ? summaryEdit_->text().trimmed() : QString();
    if (!path.isEmpty() && QFileInfo::exists(path)) {
        summaryWatcher_->addPath(path);
    }
}

bool MainWindow::loadRiskSummaryFromPath(const QString& summaryPath, bool interactive) {
    if (summaryPath.isEmpty() || !QFileInfo::exists(summaryPath)) {
        if (interactive) {
            QMessageBox::warning(this, QStringLiteral("Summary"), QStringLiteral("summary.json path is invalid."));
        }
        return false;
    }

    QFile f(summaryPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (interactive) {
            QMessageBox::warning(this, QStringLiteral("Summary"), QStringLiteral("Cannot open summary file."));
        }
        return false;
    }
    const QByteArray raw = f.readAll();
    f.close();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        if (interactive) {
            QMessageBox::warning(this, QStringLiteral("Summary"), QStringLiteral("Invalid JSON format."));
        }
        return false;
    }

    const QJsonObject root = doc.object();
    // 只读取 analysis 节点，保持与后端 summary 结构解耦。
    const QJsonObject analysis = root.value(QStringLiteral("analysis")).toObject();
    if (analysis.isEmpty()) {
        if (interactive) {
            QMessageBox::warning(this, QStringLiteral("Summary"), QStringLiteral("Missing analysis section in summary.json."));
        }
        return false;
    }

    const QString level = analysis.value(QStringLiteral("risk_level")).toString(QStringLiteral("unknown"));
    const QJsonValue scoreVal = analysis.value(QStringLiteral("risk_score"));
    QString scoreText = QStringLiteral("N/A");
    if (scoreVal.isDouble()) {
        scoreText = QString::number(scoreVal.toDouble(), 'f', 3);
    }

    QStringList flags;
    const QJsonArray flagsArr = analysis.value(QStringLiteral("risk_flags")).toArray();
    for (const QJsonValue& v : flagsArr) {
        if (v.isString()) flags << v.toString();
    }

    QStringList advice;
    const QJsonArray adviceArr = analysis.value(QStringLiteral("advice")).toArray();
    for (const QJsonValue& v : adviceArr) {
        if (v.isString()) advice << v.toString();
    }

    // Fallback for legacy summaries containing non-ASCII advice that may render as tofu.
    bool hasNonAsciiAdvice = false;
    QRegularExpression nonAsciiRx(QStringLiteral("[^\\x00-\\x7F]"));
    for (const QString& a : advice) {
        if (nonAsciiRx.match(a).hasMatch()) {
            hasNonAsciiAdvice = true;
            break;
        }
    }
    if (hasNonAsciiAdvice) {
        QStringList fallback;
        for (const QString& f : flags) {
            if (f == QStringLiteral("forward_trunk_lean_risk")) {
                fallback << QStringLiteral("Excessive forward trunk lean. Reduce pace and strengthen core stability.");
            } else if (f == QStringLiteral("left_right_knee_asymmetry_risk")) {
                fallback << QStringLiteral("Left-right knee motion asymmetry detected. Add unilateral stability and strength training.");
            } else if (f == QStringLiteral("left_knee_alignment_risk")) {
                fallback << QStringLiteral("Left knee-ankle alignment risk is elevated. Monitor landing knee path and adjust cadence.");
            } else if (f == QStringLiteral("right_knee_alignment_risk")) {
                fallback << QStringLiteral("Right knee-ankle alignment risk is elevated. Monitor landing knee path and adjust cadence.");
            } else if (f == QStringLiteral("low_pose_confidence")) {
                fallback << QStringLiteral("Pose confidence is low. Improve camera angle, lighting, and resolution.");
            }
        }
        if (fallback.isEmpty()) {
            fallback << QStringLiteral("Current risk is low. Keep training and continue monitoring.");
        }
        advice = fallback;
        appendLog(QStringLiteral("[SUMMARY] non-ascii advice detected, switched to English fallback."));
    }

    riskLevelValueLabel_->setText(level);
    if (level == QStringLiteral("high")) {
        riskLevelValueLabel_->setStyleSheet(QStringLiteral("font-size:12px; color:#b91c1c; font-weight:700; background:#fee2e2; border:1px solid #fecaca; border-radius:6px; padding:2px 6px;"));
    } else if (level == QStringLiteral("medium")) {
        riskLevelValueLabel_->setStyleSheet(QStringLiteral("font-size:12px; color:#b45309; font-weight:700; background:#ffedd5; border:1px solid #fed7aa; border-radius:6px; padding:2px 6px;"));
    } else if (level == QStringLiteral("low")) {
        riskLevelValueLabel_->setStyleSheet(QStringLiteral("font-size:12px; color:#166534; font-weight:700; background:#dcfce7; border:1px solid #bbf7d0; border-radius:6px; padding:2px 6px;"));
    } else {
        riskLevelValueLabel_->setStyleSheet(QStringLiteral("font-size:12px; font-weight:700; background:#f3f6fb; border:1px solid #e2e8f0; border-radius:6px; padding:2px 6px;"));
    }

    const QString levelLower = level.toLower();
    QString levelDesc;
    if (levelLower == QStringLiteral("high")) {
        levelDesc = QStringLiteral("High - urgent correction needed");
    } else if (levelLower == QStringLiteral("medium")) {
        levelDesc = QStringLiteral("Medium - monitor and improve this week");
    } else if (levelLower == QStringLiteral("low")) {
        levelDesc = QStringLiteral("Low - maintain good movement quality");
    } else {
        levelDesc = QStringLiteral("Unknown");
    }

    QStringList detailedAdvice;
    detailedAdvice << QStringLiteral("Overall Assessment");
    detailedAdvice << QStringLiteral("- Risk Level: %1").arg(levelDesc);
    detailedAdvice << QStringLiteral("- Risk Score: %1").arg(scoreText);
    detailedAdvice << QStringLiteral("- Current Flags: %1").arg(flags.isEmpty() ? QStringLiteral("none") : flags.join(QStringLiteral(", ")));

    const QJsonObject explainability = analysis.value(QStringLiteral("explainability")).toObject();
    const QJsonArray contributions = explainability.value(QStringLiteral("score_contributions")).toArray();
    const QJsonArray metricChecks = explainability.value(QStringLiteral("metric_checks")).toArray();
    if (!explainability.isEmpty()) {
        detailedAdvice << QString();
        detailedAdvice << QStringLiteral("Why This Score");
        if (!contributions.isEmpty()) {
            detailedAdvice << QStringLiteral("- Score Contributions:");
            for (const QJsonValue& item : contributions) {
                const QJsonObject obj = item.toObject();
                detailedAdvice << QStringLiteral("  * %1: +%2")
                                    .arg(obj.value(QStringLiteral("flag")).toString())
                                    .arg(QString::number(obj.value(QStringLiteral("weight")).toDouble(), 'f', 3));
            }
        } else {
            detailedAdvice << QStringLiteral("- No rule was triggered, score remains baseline (0.000).");
        }

        if (!metricChecks.isEmpty()) {
            detailedAdvice << QStringLiteral("- Metric vs Threshold:");
            for (const QJsonValue& item : metricChecks) {
                const QJsonObject obj = item.toObject();
                const QJsonValue value = obj.value(QStringLiteral("value"));
                const QString valueText = value.isDouble() ? QString::number(value.toDouble(), 'f', 3) : QStringLiteral("N/A");
                const QString relation = obj.value(QStringLiteral("relation")).toString();
                const QString trig = obj.value(QStringLiteral("triggered")).toBool() ? QStringLiteral("TRIGGERED") : QStringLiteral("ok");
                detailedAdvice << QStringLiteral("  * %1 = %2 (%3 %4) -> %5")
                                    .arg(obj.value(QStringLiteral("metric")).toString())
                                    .arg(valueText)
                                    .arg(relation)
                                    .arg(QString::number(obj.value(QStringLiteral("threshold")).toDouble(), 'f', 3))
                                    .arg(trig);
            }
        }
    }

    if (!flags.isEmpty()) {
        int idx = 1;
        detailedAdvice << QString();
        detailedAdvice << QStringLiteral("Targeted Corrections");
        for (const QString& f : flags) {
            if (f == QStringLiteral("forward_trunk_lean_risk")) {
                detailedAdvice << QStringLiteral("%1. Trunk Lean Control").arg(idx++);
                detailedAdvice << QStringLiteral("   - Signal: Forward lean is excessive during support phase.");
                detailedAdvice << QStringLiteral("   - Action: Lower speed 10-15%, keep chest lifted, tighten core before foot strike.");
                detailedAdvice << QStringLiteral("   - Drill: 3 sets of 30s wall-lean posture hold + 2 sets of slow high-knee runs.");
            } else if (f == QStringLiteral("left_right_knee_asymmetry_risk")) {
                detailedAdvice << QStringLiteral("%1. Left-Right Symmetry").arg(idx++);
                detailedAdvice << QStringLiteral("   - Signal: Knee movement differs between sides.");
                detailedAdvice << QStringLiteral("   - Action: Add unilateral strength training and reduce fatigue load on weak side.");
                detailedAdvice << QStringLiteral("   - Drill: Split squat 3x8 each side, single-leg bridge 3x10 each side.");
            } else if (f == QStringLiteral("left_knee_alignment_risk")) {
                detailedAdvice << QStringLiteral("%1. Left Knee Alignment").arg(idx++);
                detailedAdvice << QStringLiteral("   - Signal: Left knee path may collapse inward/outward.");
                detailedAdvice << QStringLiteral("   - Action: Focus on knee-over-toe alignment at landing and push-off.");
                detailedAdvice << QStringLiteral("   - Drill: Lateral band walk 3x12 + single-leg squat to box 3x6.");
            } else if (f == QStringLiteral("right_knee_alignment_risk")) {
                detailedAdvice << QStringLiteral("%1. Right Knee Alignment").arg(idx++);
                detailedAdvice << QStringLiteral("   - Signal: Right knee tracking is unstable under load.");
                detailedAdvice << QStringLiteral("   - Action: Improve hip-knee-ankle alignment and shorten stride temporarily.");
                detailedAdvice << QStringLiteral("   - Drill: Step-down control 3x8 + resisted terminal knee extension 3x12.");
            } else if (f == QStringLiteral("low_pose_confidence")) {
                detailedAdvice << QStringLiteral("%1. Capture Quality").arg(idx++);
                detailedAdvice << QStringLiteral("   - Signal: Pose confidence is low, result reliability is reduced.");
                detailedAdvice << QStringLiteral("   - Action: Increase lighting, keep full body in frame, avoid motion blur.");
                detailedAdvice << QStringLiteral("   - Drill: Re-record with fixed camera height at hip level and side/front view.");
            } else {
                detailedAdvice << QStringLiteral("%1. %2").arg(idx++).arg(f);
                detailedAdvice << QStringLiteral("   - Action: Check this metric trend in the next 3 sessions and adjust training load.");
            }
        }
    } else {
        detailedAdvice << QString();
        detailedAdvice << QStringLiteral("Targeted Corrections");
        detailedAdvice << QStringLiteral("- No obvious high-risk flag was detected in this sample.");
        detailedAdvice << QStringLiteral("- Continue regular technique check every 3-5 training sessions.");
    }

    detailedAdvice << QString();
    detailedAdvice << QStringLiteral("Training Plan (Next 7 Days)");
    if (levelLower == QStringLiteral("high")) {
        detailedAdvice << QStringLiteral("- Day 1-2: Reduce high-impact training volume by 30-40%.");
        detailedAdvice << QStringLiteral("- Day 3-5: Add technique drills first, then low-intensity running.");
        detailedAdvice << QStringLiteral("- Day 6-7: Re-test with camera and compare key flags.");
    } else if (levelLower == QStringLiteral("medium")) {
        detailedAdvice << QStringLiteral("- Keep normal volume but reduce pace peaks and fatigue accumulation.");
        detailedAdvice << QStringLiteral("- Insert 10-15 minutes corrective drills before each session.");
        detailedAdvice << QStringLiteral("- Re-check movement quality within one week.");
    } else {
        detailedAdvice << QStringLiteral("- Maintain current plan and add light stability work 2-3 times/week.");
        detailedAdvice << QStringLiteral("- Keep collecting clips under consistent camera setup for trend tracking.");
    }

    if (!advice.isEmpty()) {
        detailedAdvice << QString();
        detailedAdvice << QStringLiteral("Model Notes");
        for (const QString& a : advice) {
            detailedAdvice << QStringLiteral("- %1").arg(a);
        }
    }

    riskScoreValueLabel_->setText(scoreText);
    riskFlagsValueLabel_->setText(flags.isEmpty() ? QStringLiteral("none") : flags.join(QStringLiteral(" | ")));
    riskAdviceView_->setPlainText(detailedAdvice.join(QStringLiteral("\n")));

    appendLog(QStringLiteral("[SUMMARY] loaded: %1 level=%2 score=%3").arg(summaryPath, level, scoreText));
    const QJsonObject perfObj = root.value(QStringLiteral("performance")).toObject();
    if (!perfObj.isEmpty()) {
        appendLog(QStringLiteral("[PERF] infer_ms=%1 pose_ms=%2 feature_ms=%3 fps=%4")
                      .arg(perfObj.value(QStringLiteral("avg_infer_ms")).toDouble())
                      .arg(perfObj.value(QStringLiteral("avg_pose_decode_ms")).toDouble())
                      .arg(perfObj.value(QStringLiteral("avg_feature_ms")).toDouble())
                      .arg(perfObj.value(QStringLiteral("throughput_fps")).toDouble()));
    }
    statusBar()->showMessage(interactive ? QStringLiteral("Risk summary loaded.") : QStringLiteral("Risk summary auto-updated."), 5000);
    setOperationHint(QStringLiteral("Status: Summary loaded successfully. Review Advice and diagnostics."));
    saveUiSettings();
    refreshSummaryWatcher();
    return true;
}

void MainWindow::copyRiskAdvice() {
    if (!riskAdviceView_) {
        return;
    }
    const QString txt = riskAdviceView_->toPlainText().trimmed();
    if (txt.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Advice"), QStringLiteral("No advice to copy."));
        return;
    }
    QGuiApplication::clipboard()->setText(txt);
    statusBar()->showMessage(QStringLiteral("Advice copied to clipboard."), 3000);
}

void MainWindow::onInferenceFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    setUiRunning(false);
    if (livePreviewTimer_) {
        // 任务结束后停止轮询，避免空读文件。
        livePreviewTimer_->stop();
    }

    // Flush any remaining buffered partial lines.
    if (!processStdoutBuffer_.trimmed().isEmpty()) {
        if (taskMode_ == TaskMode::VideoAnalysis) {
            appendLog(QStringLiteral("[VIDEO][STDOUT] %1").arg(processStdoutBuffer_.trimmed()));
        } else {
            appendLog(QStringLiteral("[STDOUT] %1").arg(processStdoutBuffer_.trimmed()));
        }
    }
    if (!processStderrBuffer_.trimmed().isEmpty()) {
        if (taskMode_ == TaskMode::VideoAnalysis) {
            appendLog(QStringLiteral("[VIDEO][STDERR] %1").arg(processStderrBuffer_.trimmed()));
        } else {
            appendLog(QStringLiteral("[STDERR] %1").arg(processStderrBuffer_.trimmed()));
        }
    }
    processStdoutBuffer_.clear();
    processStderrBuffer_.clear();

    const QString stdOut = QString::fromLocal8Bit(process_->readAllStandardOutput());
    const QString stdErr = QString::fromLocal8Bit(process_->readAllStandardError());

    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        if (taskMode_ == TaskMode::VideoAnalysis) {
            const QString outDir = analysisOutDirEdit_->text().trimmed();
            const QString summaryPath = QDir(outDir).filePath("summary.json");
            const QString overlayPath = QDir(outDir).filePath("overlay.mp4");
            if (!stdOut.trimmed().isEmpty()) appendLog(QStringLiteral("[VIDEO][OK] %1").arg(stdOut.trimmed()));
            if (!stdErr.trimmed().isEmpty()) appendLog(QStringLiteral("[VIDEO][STDERR] %1").arg(stdErr.trimmed()));

            if (QFileInfo::exists(summaryPath)) {
                summaryEdit_->setText(summaryPath);
                refreshSummaryWatcher();
                loadRiskSummaryFromPath(summaryPath, false);
            } else {
                appendLog(QStringLiteral("[VIDEO][WARN] summary missing: %1").arg(summaryPath));
            }
            if (QFileInfo::exists(overlayPath)) {
                appendLog(QStringLiteral("[VIDEO] overlay saved: %1").arg(overlayPath));
            }

            const qint64 elapsedMs = inferTimer_.isValid() ? inferTimer_.elapsed() : -1;
            if (batchProgressBar_) {
                batchProgressBar_->setRange(0, 100);
                batchProgressBar_->setValue(100);
            }
            if (batchStatusLabel_) {
                QRegularExpression rxProcessed(QStringLiteral("processed_frames=([0-9]+)"));
                QRegularExpressionMatch m = rxProcessed.match(stdOut);
                if (m.hasMatch()) {
                    batchStatusLabel_->setText(QStringLiteral("Video done: processed_frames=%1").arg(m.captured(1)));
                } else {
                    batchStatusLabel_->setText(QStringLiteral("Video analysis done"));
                }
            }
            if (elapsedMs >= 0) {
                statusBar()->showMessage(QStringLiteral("Video analysis done. e2e_ms=%1").arg(elapsedMs), 8000);
            } else {
                statusBar()->showMessage(QStringLiteral("Video analysis done."), 8000);
            }
            setOperationHint(QStringLiteral("Status: Video analysis completed successfully."));
            taskMode_ = TaskMode::None;
            return;
        }

        const QString outputPath = outputEdit_->text().trimmed();
        showImageToLabel(outputPath, outputPreviewLabel_);
        const qint64 elapsedMs = inferTimer_.isValid() ? inferTimer_.elapsed() : -1;
        QRegularExpression rx(QStringLiteral("cost_ms=([0-9]+(?:\\.[0-9]+)?)"));
        QRegularExpressionMatch match = rx.match(stdOut);
        QString modelCost;
        if (match.hasMatch()) {
            modelCost = match.captured(1);
        }
        if (!modelCost.isEmpty() && elapsedMs >= 0) {
            statusBar()->showMessage(QStringLiteral("Done. model_cost_ms=%1, e2e_ms=%2")
                                         .arg(modelCost)
                                         .arg(elapsedMs));
        } else if (elapsedMs >= 0) {
            statusBar()->showMessage(QStringLiteral("Done. e2e_ms=%1").arg(elapsedMs));
        } else {
            statusBar()->showMessage(QStringLiteral("Done."));
        }
        if (!stdOut.trimmed().isEmpty()) appendLog(QStringLiteral("[OK] %1").arg(stdOut.trimmed()));
        setOperationHint(QStringLiteral("Status: Inference completed successfully."));
        taskMode_ = TaskMode::None;
        return;
    }

    if (taskMode_ == TaskMode::VideoAnalysis) {
        statusBar()->showMessage(QStringLiteral("Video analysis failed"));
        if (batchProgressBar_) {
            batchProgressBar_->setRange(0, 100);
            batchProgressBar_->setValue(0);
        }
        if (batchStatusLabel_) {
            batchStatusLabel_->setText(QStringLiteral("Video analysis failed"));
        }
        if (!stdOut.trimmed().isEmpty()) appendLog(QStringLiteral("[VIDEO][FAIL][STDOUT] %1").arg(stdOut.trimmed()));
        if (!stdErr.trimmed().isEmpty()) appendLog(QStringLiteral("[VIDEO][FAIL][STDERR] %1").arg(stdErr.trimmed()));
        setOperationHint(QStringLiteral("Status: Video analysis failed. Check stderr and precheck logs."), true);
        QMessageBox::critical(
            this,
            QStringLiteral("Video analysis failed"),
            QStringLiteral("exitCode=%1\nstdout:\n%2\nstderr:\n%3")
                .arg(exitCode)
                .arg(stdOut)
                .arg(stdErr));
        taskMode_ = TaskMode::None;
        return;
    }

    statusBar()->showMessage(QStringLiteral("Inference failed"));
    if (!stdOut.trimmed().isEmpty()) appendLog(QStringLiteral("[FAIL][STDOUT] %1").arg(stdOut.trimmed()));
    if (!stdErr.trimmed().isEmpty()) appendLog(QStringLiteral("[FAIL][STDERR] %1").arg(stdErr.trimmed()));
    setOperationHint(QStringLiteral("Status: Inference failed. Check output log for details."), true);
    QMessageBox::critical(
        this,
        QStringLiteral("Inference failed"),
        QStringLiteral("exitCode=%1\nstdout:\n%2\nstderr:\n%3")
            .arg(exitCode)
            .arg(stdOut)
            .arg(stdErr));
    taskMode_ = TaskMode::None;
}

void MainWindow::closeEvent(QCloseEvent* event) {
    saveUiSettings();
    QMainWindow::closeEvent(event);
}
