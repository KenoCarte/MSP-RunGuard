#pragma once

#include <QMainWindow>
#include <QProcess>
#include <QElapsedTimer>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QCheckBox;
class QSpinBox;
class QComboBox;
class QCloseEvent;
class QFileSystemWatcher;
class QTimer;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void browseVideoPath();
    void browseAnalysisOutDir();
    void browseRiskConfigPath();
    void browseInputImage();
    void browseOutputImage();
    void browsePythonPath();
    void browseScriptPath();
    void browseCfgPath();
    void browseWeightPath();
    void browseSummaryPath();
    void runInference();
    void runVideoAnalysis();
    void onSourceModeChanged(int index);
    void detectCameraIndex();
    void refreshLivePreview();
    void validateConfiguration();
    void saveLogToFile();
    void runBatchInference();
    void cancelBatchInference();
    void loadRiskSummary();
    void copyRiskAdvice();
    void onProcessStdoutReady();
    void onProcessStderrReady();
    void onSummaryPathChanged(const QString& path);
    void onSummaryFileChanged(const QString& path);
    void onInferenceFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    enum class TaskMode {
        None,
        ImageInference,
        VideoAnalysis,
    };

    void buildUi();
    void loadDefaults();
    void loadUiSettings();
    void saveUiSettings() const;
    void setUiRunning(bool running);
    bool validatePaths(QString* errorMessage = nullptr) const;
    QStringList buildInferenceArgs(const QString& inputPath, const QString& outputPath) const;
    void appendLog(const QString& line);
    void setRiskPanelDefaults();
    void refreshSummaryWatcher();
    bool loadRiskSummaryFromPath(const QString& summaryPath, bool interactive);
    void showImageToLabel(const QString& imagePath, QLabel* targetLabel);
    static QString findProjectRoot();
    static QString formatEtaSeconds(qint64 seconds);

protected:
    void closeEvent(QCloseEvent* event) override;

    QLineEdit* inputEdit_ = nullptr;
    QLineEdit* outputEdit_ = nullptr;
    QLineEdit* pythonEdit_ = nullptr;
    QLineEdit* scriptEdit_ = nullptr;
    QLineEdit* cfgEdit_ = nullptr;
    QLineEdit* weightEdit_ = nullptr;
    QLineEdit* videoEdit_ = nullptr;
    QLineEdit* analysisOutDirEdit_ = nullptr;
    QLineEdit* riskConfigEdit_ = nullptr;
    QLineEdit* summaryEdit_ = nullptr;

    QPushButton* runButton_ = nullptr;
    QPushButton* runVideoButton_ = nullptr;
    QPushButton* validateButton_ = nullptr;
    QPushButton* batchButton_ = nullptr;
    QPushButton* cancelBatchButton_ = nullptr;
    QPushButton* detectCameraButton_ = nullptr;
    QPushButton* loadSummaryButton_ = nullptr;
    QPushButton* copyAdviceButton_ = nullptr;
    QPushButton* saveLogButton_ = nullptr;
    QLabel* inputPreviewLabel_ = nullptr;
    QLabel* outputPreviewLabel_ = nullptr;
    QLabel* batchStatusLabel_ = nullptr;
    QLabel* riskLevelValueLabel_ = nullptr;
    QLabel* riskScoreValueLabel_ = nullptr;
    QLabel* riskFlagsValueLabel_ = nullptr;
    QLabel* livePreviewLabel_ = nullptr;
    QPlainTextEdit* riskAdviceView_ = nullptr;
    QPlainTextEdit* logView_ = nullptr;
    QProgressBar* batchProgressBar_ = nullptr;
    QFileSystemWatcher* summaryWatcher_ = nullptr;

    QProcess* process_ = nullptr;
    QProcess* activeBatchProcess_ = nullptr;
    QString processStdoutBuffer_;
    QString processStderrBuffer_;
    QString livePreviewPath_;
    QTimer* livePreviewTimer_ = nullptr;
    QElapsedTimer inferTimer_;
    TaskMode taskMode_ = TaskMode::None;
    QSpinBox* frameStrideSpin_ = nullptr;
    QComboBox* sourceModeCombo_ = nullptr;
    QSpinBox* cameraIndexSpin_ = nullptr;
    QSpinBox* maxSecondsSpin_ = nullptr;
    QCheckBox* saveOverlayCheck_ = nullptr;
        QCheckBox* showLiveCheck_ = nullptr;
    bool batchCancelRequested_ = false;
};
