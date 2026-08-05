#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <DMainWindow>
#include <DPushButton>
#include <DSpinBox>
#include <DLineEdit>
#include <DLabel>
#include <DTextEdit>
#include <DFrame>
#include <DSwitch>

#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>

class QLocalSocket;
class QProcess;

DWIDGET_USE_NAMESPACE

class MainWindow : public DMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onStartServer();
    void onStopServer();
    void onRestartServer();
    void onApplyConfig();

    // 智能模式
    void onSmartToggled(bool checked);
    void onAutoIntervalToggled(bool checked);
    void onTargetProcessesEdited();

    void connectToDaemon();
    bool autoStartDaemon();
    bool isDaemonRunning();

    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketReadyRead();
    void onSocketError();

private:
    void initUI();
    void setupConnections();
    void appendLog(const QString &message);
    void sendConfigToDaemon(int minutes);
    void sendSmartConfig();
    void sendTargetConfig();
    void sendAutoIntervalConfig();
    void updateServerStatus();

    // 中央控件
    QWidget *m_centralWidget;
    QVBoxLayout *m_mainLayout;

    // 服务控制区域
    DFrame *m_controlFrame;
    QHBoxLayout *m_controlLayout;
    DPushButton *m_startServerBtn;
    DPushButton *m_stopServerBtn;
    DPushButton *m_restartServerBtn;

    // 手动配置区域
    DFrame *m_configFrame;
    QHBoxLayout *m_configLayout;
    DLabel *m_configLabel;
    DSpinBox *m_valueSpinBox;
    DPushButton *m_applyBtn;

    // 智能阻止区域
    DFrame *m_smartFrame;
    QVBoxLayout *m_smartLayout;
    QHBoxLayout *m_smartTopLayout;
    DSwitch *m_smartSwitch;
    DLabel *m_smartSwitchLabel;
    DSwitch *m_autoIntervalSwitch;
    DLabel *m_autoIntervalLabel;
    QHBoxLayout *m_targetLayout;
    DLabel *m_targetLabel;
    DLineEdit *m_targetEdit;
    QHBoxLayout *m_delayInfoLayout;
    DLabel *m_delayInfoLabel;
    DLabel *m_delayInfoValue;

    // 状态区域
    DFrame *m_statusFrame;
    QHBoxLayout *m_statusLayout;
    DLabel *m_statusTitleLabel;
    DLabel *m_statusValueLabel;

    // 日志区域
    DFrame *m_logFrame;
    QVBoxLayout *m_logLayout;
    DLabel *m_logTitleLabel;
    DTextEdit *m_logTextEdit;

    // 服务状态
    bool m_isServerRunning;

    // IPC
    QLocalSocket *m_ipcSocket;
    QString m_socketPath;

    // 守护进程
    QProcess *m_daemonProcess;
    bool m_autoStarted;

    // 智能模式运行时数据（从守护进程 STATUS 消息更新）
    int m_lastIntervalMs = 0;
};

#endif // MAINWINDOW_H
