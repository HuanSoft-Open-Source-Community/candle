#include "mainwindow.h"

#include <DSuggestButton>
#include <DWarningButton>
#include <DWidgetUtil>
#include <DTitlebar>

#include <QDateTime>
#include <QDir>
#include <QIcon>
#include <QLocalSocket>
#include <QProcess>
#include <QThread>
#include <unistd.h>

DWIDGET_USE_NAMESPACE

/**
 * @brief 计算用户专属 socket 路径（与 numlockd 侧保持一致）
 *
 * 优先 $XDG_RUNTIME_DIR（用户专属、权限 0700），未设置时回退
 * /tmp/numlockd-<uid>.sock。
 */
static QString numlockSocketPath()
{
    const QByteArray runtime = qgetenv("XDG_RUNTIME_DIR");
    if (!runtime.isEmpty()) {
        return QString::fromUtf8(runtime) + QStringLiteral("/numlockd.sock");
    }
    return QStringLiteral("/tmp/numlockd-%1.sock")
        .arg(static_cast<qint64>(getuid()));
}

/**
 * @brief 构造函数
 */
MainWindow::MainWindow(QWidget *parent)
    : DMainWindow(parent)
    , m_isServerRunning(false)
    , m_ipcSocket(nullptr)
    , m_socketPath(numlockSocketPath())
    , m_daemonProcess(nullptr)
    , m_autoStarted(false)
{
    // 设置窗口属性
    setMinimumSize(QSize(900, 500));
    resize(900, 500);

    // 设置标题栏
    titlebar()->setIcon(QIcon(":/images/logo.svg"));
    titlebar()->setTitle(tr("秉烛"));

    // 初始化 UI
    initUI();

    // 连接信号与槽
    setupConnections();

    // 初始化状态
    m_statusValueLabel->setText(tr("已停止"));
    m_statusValueLabel->setStyleSheet("color: #999;");

    // 初始化 IPC Socket
    m_ipcSocket = new QLocalSocket(this);
    connect(m_ipcSocket, &QLocalSocket::connected, this, &MainWindow::onSocketConnected);
    connect(m_ipcSocket, &QLocalSocket::disconnected, this, &MainWindow::onSocketDisconnected);
    connect(m_ipcSocket, &QLocalSocket::readyRead, this, &MainWindow::onSocketReadyRead);
    connect(m_ipcSocket, QOverload<QLocalSocket::LocalSocketError>::of(&QLocalSocket::errorOccurred),
            this, &MainWindow::onSocketError);

    // 尝试连接到守护进程，如果失败则自动启动
    if (!isDaemonRunning()) {
        appendLog(tr("守护进程未运行，正在自动启动..."));
        if (autoStartDaemon()) {
            appendLog(tr("守护进程启动成功"));
        } else {
            appendLog(tr("守护进程启动失败"));
        }
    }

    // 连接到守护进程
    connectToDaemon();

    // 添加启动日志
    appendLog(tr("应用程序已启动"));
}

/**
 * @brief 析构函数
 */
MainWindow::~MainWindow()
{
    // GUI 退出时终止本程序自动启动的守护进程（生命周期绑定），
    // 确保无后台残留进程（最小权限原则：守护进程只在面板运行时存在）
    stopAutoStartedDaemon();
}

/**
 * @brief 初始化用户界面
 */
void MainWindow::initUI()
{
    // 创建中央窗体
    m_centralWidget = new QWidget(this);
    m_mainLayout = new QVBoxLayout(m_centralWidget);
    m_mainLayout->setContentsMargins(20, 20, 20, 20);
    m_mainLayout->setSpacing(15);

    // ==================== 服务控制区域 ====================
    m_controlFrame = new DFrame(this);
    m_controlLayout = new QHBoxLayout(m_controlFrame);
    m_controlLayout->setContentsMargins(15, 15, 15, 15);
    m_controlLayout->setSpacing(15);

    // 启动服务按钮 - 使用 DSuggestButton 样式（蓝色强调）
    m_startServerBtn = new DPushButton(tr("启动服务"), this);
    m_startServerBtn->setIcon(QIcon::fromTheme("media-playback-start"));
    m_startServerBtn->setFixedSize(140, 40);
    m_startServerBtn->setProperty("type", "suggest");  // 设置建议按钮样式

    // 停止服务按钮 - 使用 DWarningButton 样式（红色警示）
    m_stopServerBtn = new DPushButton(tr("停止服务"), this);
    m_stopServerBtn->setIcon(QIcon::fromTheme("media-playback-stop"));
    m_stopServerBtn->setFixedSize(140, 40);
    m_stopServerBtn->setProperty("type", "warning");  // 设置警告按钮样式
    m_stopServerBtn->setEnabled(false);  // 初始状态下禁用停止按钮

    // 重启服务按钮
    m_restartServerBtn = new DPushButton(tr("重启服务"), this);
    m_restartServerBtn->setIcon(QIcon::fromTheme("view-refresh"));
    m_restartServerBtn->setFixedSize(140, 40);
    m_restartServerBtn->setProperty("type", "suggest");

    m_controlLayout->addWidget(m_startServerBtn);
    m_controlLayout->addWidget(m_stopServerBtn);
    m_controlLayout->addWidget(m_restartServerBtn);
    m_controlLayout->addStretch();

    // ==================== 配置区域 ====================
    m_configFrame = new DFrame(this);
    m_configLayout = new QHBoxLayout(m_configFrame);
    m_configLayout->setContentsMargins(15, 15, 15, 15);
    m_configLayout->setSpacing(15);

    // 配置标签
    m_configLabel = new DLabel(tr("时间:"), this);

    // 数值输入框
    m_valueSpinBox = new DSpinBox(this);
    m_valueSpinBox->setRange(2, 9999);
    m_valueSpinBox->setValue(15);
    m_valueSpinBox->setSuffix(tr(" 分钟"));
    m_valueSpinBox->setFixedWidth(180);

    // 应用配置按钮
    m_applyBtn = new DPushButton(tr("应用配置"), this);
    m_applyBtn->setIcon(QIcon::fromTheme("document-save"));
    m_applyBtn->setFixedSize(120, 36);

    m_configLayout->addWidget(m_configLabel);
    m_configLayout->addWidget(m_valueSpinBox);
    m_configLayout->addWidget(m_applyBtn);
    m_configLayout->addStretch();

    // ==================== 智能阻止区域 ====================
    m_smartFrame = new DFrame(this);
    m_smartLayout = new QVBoxLayout(m_smartFrame);
    m_smartLayout->setContentsMargins(15, 15, 15, 15);
    m_smartLayout->setSpacing(8);

    // 第一行：智能开关 + 自动同步开关
    m_smartTopLayout = new QHBoxLayout();
    m_smartTopLayout->setSpacing(10);

    m_smartSwitch = new DSwitchButton(this);
    m_smartSwitchLabel = new DLabel(tr("智能阻止"), this);
    m_smartSwitchLabel->setBuddy(m_smartSwitch);

    m_autoIntervalSwitch = new DSwitchButton(this);
    m_autoIntervalLabel = new DLabel(tr("自动同步熄屏时间"), this);
    m_autoIntervalLabel->setBuddy(m_autoIntervalSwitch);

    m_smartTopLayout->addWidget(m_smartSwitchLabel);
    m_smartTopLayout->addWidget(m_smartSwitch);
    m_smartTopLayout->addSpacing(20);
    m_smartTopLayout->addWidget(m_autoIntervalLabel);
    m_smartTopLayout->addWidget(m_autoIntervalSwitch);
    m_smartTopLayout->addStretch();

    // 第二行：目标进程输入
    m_targetLayout = new QHBoxLayout();
    m_targetLabel = new DLabel(tr("目标进程:"), this);
    m_targetEdit = new DLineEdit(this);
    m_targetEdit->setPlaceholderText(tr("进程名，逗号分隔（如 firefox,chromium）"));
    m_targetEdit->setClearButtonEnabled(true);

    m_targetLayout->addWidget(m_targetLabel);
    m_targetLayout->addWidget(m_targetEdit, 1);

    // 第三行：当前熄屏时间 + 计算间隔
    m_delayInfoLayout = new QHBoxLayout();
    m_delayInfoLabel = new DLabel(tr("守护进程上报的间隔与状态将显示在下方状态栏"), this);
    m_delayInfoLabel->setStyleSheet("color: #888;");

    m_delayInfoValue = new DLabel(this);
    m_delayInfoValue->setStyleSheet("color: #555;");

    m_delayInfoLayout->addWidget(m_delayInfoLabel);
    m_delayInfoLayout->addWidget(m_delayInfoValue);
    m_delayInfoLayout->addStretch();

    m_smartLayout->addLayout(m_smartTopLayout);
    m_smartLayout->addLayout(m_targetLayout);
    m_smartLayout->addLayout(m_delayInfoLayout);

    // 初始状态：智能模式关闭时禁用子控件
    m_targetEdit->setEnabled(false);
    m_autoIntervalSwitch->setEnabled(false);

    // ==================== 底部区域（状态栏 + 日志）====================
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    bottomLayout->setSpacing(15);

    // 状态区域
    m_statusFrame = new DFrame(this);
    m_statusLayout = new QHBoxLayout(m_statusFrame);
    m_statusLayout->setContentsMargins(15, 15, 15, 15);
    m_statusLayout->setSpacing(10);

    m_statusTitleLabel = new DLabel(tr("服务状态:"), this);
    m_statusValueLabel = new DLabel(this);
    m_statusValueLabel->setWordWrap(true);

    m_statusLayout->addWidget(m_statusTitleLabel);
    m_statusLayout->addWidget(m_statusValueLabel);
    m_statusLayout->addStretch();

    // 日志区域
    m_logFrame = new DFrame(this);
    m_logLayout = new QVBoxLayout(m_logFrame);
    m_logLayout->setContentsMargins(15, 15, 15, 15);
    m_logLayout->setSpacing(10);

    m_logTitleLabel = new DLabel(tr("运行日志"), this);
    m_logTitleLabel->setStyleSheet("font-weight: bold;");

    m_logTextEdit = new DTextEdit(this);
    m_logTextEdit->setReadOnly(true);
    m_logTextEdit->setPlaceholderText(tr("日志将显示在这里..."));

    m_logLayout->addWidget(m_logTitleLabel);
    m_logLayout->addWidget(m_logTextEdit);

    // 将状态栏和日志窗格添加到底部布局
    bottomLayout->addWidget(m_statusFrame, 1);  // 状态栏占 1 份
    bottomLayout->addWidget(m_logFrame, 2);     // 日志窗格占 2 份

    // ==================== 组装主布局 ====================
    m_mainLayout->addWidget(m_controlFrame);
    m_mainLayout->addWidget(m_configFrame);
    m_mainLayout->addWidget(m_smartFrame);
    m_mainLayout->addLayout(bottomLayout, 1);  // 底部区域可伸展

    // 设置中央窗体
    setCentralWidget(m_centralWidget);
}

/**
 * @brief 连接信号与槽
 */
void MainWindow::setupConnections()
{
    // 启动服务按钮
    connect(m_startServerBtn, &DPushButton::clicked, this, &MainWindow::onStartServer);

    // 停止服务按钮
    connect(m_stopServerBtn, &DPushButton::clicked, this, &MainWindow::onStopServer);

    // 重启服务按钮
    connect(m_restartServerBtn, &DPushButton::clicked, this, &MainWindow::onRestartServer);

    // 应用配置按钮
    connect(m_applyBtn, &DPushButton::clicked, this, &MainWindow::onApplyConfig);

    // 智能模式
    connect(m_smartSwitch, &DSwitchButton::toggled, this, &MainWindow::onSmartToggled);
    connect(m_autoIntervalSwitch, &DSwitchButton::toggled, this, &MainWindow::onAutoIntervalToggled);
    connect(m_targetEdit, &DLineEdit::editingFinished, this, &MainWindow::onTargetProcessesEdited);
}

/**
 * @brief 启动服务按钮槽函数
 */
void MainWindow::onStartServer()
{
    if (m_isServerRunning) {
        appendLog(tr("服务已经在运行中"));
        return;
    }

    m_isServerRunning = true;

    // 更新状态显示
    m_statusValueLabel->setText(tr("运行中 (时间: %1 分钟)").arg(m_valueSpinBox->value()));
    m_statusValueLabel->setStyleSheet("color: #2ecc71; font-weight: bold;");

    // 更新按钮状态
    m_startServerBtn->setEnabled(false);
    m_stopServerBtn->setEnabled(true);
    m_restartServerBtn->setEnabled(true);

    // 记录日志
    appendLog(tr("服务已启动，设定时间: %1 分钟").arg(m_valueSpinBox->value()));

    // 如果已连接，发送配置到守护进程
    if (m_ipcSocket && m_ipcSocket->state() == QLocalSocket::ConnectedState) {
        sendConfigToDaemon(m_valueSpinBox->value());
    }
}

/**
 * @brief 停止服务按钮槽函数
 */
void MainWindow::onStopServer()
{
    if (!m_isServerRunning) {
        appendLog(tr("服务已经处于停止状态"));
        return;
    }

    m_isServerRunning = false;

    // 更新状态显示
    m_statusValueLabel->setText(tr("已停止"));
    m_statusValueLabel->setStyleSheet("color: #999;");

    // 更新按钮状态
    m_startServerBtn->setEnabled(true);
    m_stopServerBtn->setEnabled(false);

    // 记录日志
    appendLog(tr("服务已停止"));

    // 断开与守护进程的连接
    if (m_ipcSocket && m_ipcSocket->state() == QLocalSocket::ConnectedState) {
        m_ipcSocket->disconnectFromServer();
    }

    // 如果是本程序自动启动的守护进程，则终止它
    if (m_autoStarted && m_daemonProcess) {
        appendLog(tr("正在终止守护进程..."));
        stopAutoStartedDaemon();
        appendLog(tr("守护进程已终止"));
    }
}

/**
 * @brief 终止本程序自动启动的守护进程
 */
void MainWindow::stopAutoStartedDaemon()
{
    if (!m_autoStarted || !m_daemonProcess) {
        return;
    }

    m_daemonProcess->terminate();
    if (!m_daemonProcess->waitForFinished(3000)) {
        m_daemonProcess->kill();
        m_daemonProcess->waitForFinished(1000);
    }
    delete m_daemonProcess;
    m_daemonProcess = nullptr;
    m_autoStarted = false;
}

/**
 * @brief 重启服务按钮槽函数
 */
void MainWindow::onRestartServer()
{
    if (!m_isServerRunning) {
        appendLog(tr("服务已停止，现在启动服务..."));
        onStartServer();
        return;
    }

    appendLog(tr("正在重启服务..."));

    m_isServerRunning = false;

    // 断开与守护进程的连接
    if (m_ipcSocket && m_ipcSocket->state() == QLocalSocket::ConnectedState) {
        m_ipcSocket->disconnectFromServer();
    }

    // 如果是本程序自动启动的守护进程，则终止它
    stopAutoStartedDaemon();

    QThread::msleep(500);

    onStartServer();
}

/**
 * @brief 应用配置按钮槽函数
 */
void MainWindow::onApplyConfig()
{
    int timeValue = m_valueSpinBox->value();

    // 如果服务正在运行，提示需要重启
    if (m_isServerRunning) {
        appendLog(tr("配置已更新为时间 %1 分钟，将在服务重启后生效").arg(timeValue));
        m_statusValueLabel->setText(tr("运行中 (时间: %1 分钟，配置待重启)").arg(timeValue));
    } else {
        appendLog(tr("配置已更新为时间 %1 分钟").arg(timeValue));
    }

    // 如果已连接，发送新配置到守护进程
    if (m_ipcSocket && m_ipcSocket->state() == QLocalSocket::ConnectedState) {
        sendConfigToDaemon(timeValue);
        appendLog(tr("配置已发送到守护进程: %1 分钟").arg(timeValue));
    }
}

/**
 * @brief 添加日志信息
 */
void MainWindow::appendLog(const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    QString logEntry = QString("[%1] %2").arg(timestamp, message);

    m_logTextEdit->append(logEntry);
}

/**
 * @brief 更新服务器状态显示
 */
void MainWindow::updateServerStatus()
{
    if (m_isServerRunning) {
        m_statusValueLabel->setText(tr("运行中 (时间: %1 分钟)").arg(m_valueSpinBox->value()));
        m_statusValueLabel->setStyleSheet("color: #2ecc71; font-weight: bold;");
        m_startServerBtn->setEnabled(false);
        m_stopServerBtn->setEnabled(true);
    } else {
        m_statusValueLabel->setText(tr("已停止"));
        m_statusValueLabel->setStyleSheet("color: #999;");
        m_startServerBtn->setEnabled(true);
        m_stopServerBtn->setEnabled(false);
    }
}

/**
 * @brief 连接到守护进程
 */
void MainWindow::connectToDaemon()
{
    if (!m_ipcSocket) {
        return;
    }

    if (m_ipcSocket->state() == QLocalSocket::UnconnectedState) {
        m_ipcSocket->connectToServer(m_socketPath);
    }
}

/**
 * @brief 检查守护进程是否正在运行
 */
bool MainWindow::isDaemonRunning()
{
    // 尝试连接来检查守护进程是否运行
    QLocalSocket testSocket;
    testSocket.connectToServer(m_socketPath);
    bool connected = testSocket.waitForConnected(500);
    if (connected) {
        testSocket.disconnectFromServer();
    }
    return connected;
}

/**
 * @brief 自动启动守护进程
 */
bool MainWindow::autoStartDaemon()
{
    if (m_daemonProcess) {
        return false;
    }

    // 获取 numlockd 可执行文件路径
    QString exePath = QCoreApplication::applicationDirPath();
    // 尝试在相同目录查找 numlockd（安装后的位置）
    QString daemonPath = exePath + "/numlockd";

    // 检查文件是否存在
    if (!QFile::exists(daemonPath)) {
        appendLog(tr("找不到守护进程可执行文件: %1").arg(daemonPath));
        return false;
    }

    // 创建进程
    m_daemonProcess = new QProcess(this);

    // 设置工作目录为可执行文件所在目录
    QString workDir = QFileInfo(daemonPath).absolutePath();
    m_daemonProcess->setWorkingDirectory(workDir);

    // 连接信号
    connect(m_daemonProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus status) {
        appendLog(tr("守护进程已退出 (代码: %1, 状态: %2)").arg(exitCode).arg(status));
        m_autoStarted = false;
        m_isServerRunning = false;
        updateServerStatus();
    });

    connect(m_daemonProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError /*error*/) {
        appendLog(tr("守护进程错误: %1").arg(m_daemonProcess->errorString()));
    });

    // 启动进程
    appendLog(tr("正在启动守护进程..."));
    m_daemonProcess->start(daemonPath, QStringList());
    if (!m_daemonProcess->waitForStarted(3000)) {
        appendLog(tr("守护进程启动超时"));
        delete m_daemonProcess;
        m_daemonProcess = nullptr;
        return false;
    }

    appendLog(tr("守护进程进程已启动，PID: %1").arg(m_daemonProcess->processId()));

    // 等待服务器准备好接受连接
    QThread::msleep(1000);

    // 检查进程是否还在运行
    if (m_daemonProcess->state() != QProcess::Running) {
        appendLog(tr("守护进程启动后退出，退出码: %1").arg(m_daemonProcess->exitCode()));
        delete m_daemonProcess;
        m_daemonProcess = nullptr;
        return false;
    }

    m_autoStarted = true;
    m_isServerRunning = true;
    updateServerStatus();

    return true;
}

/**
 * @brief 连接成功槽函数
 */
void MainWindow::onSocketConnected()
{
    appendLog(tr("已连接到守护进程"));
    // 连接成功时同步智能模式配置到守护进程
    sendSmartConfig();
    sendAutoIntervalConfig();
    sendTargetConfig();
}

/**
 * @brief 连接断开槽函数
 */
void MainWindow::onSocketDisconnected()
{
    appendLog(tr("与守护进程断开连接"));
}

/**
 * @brief 接收数据槽函数
 */
void MainWindow::onSocketReadyRead()
{
    if (!m_ipcSocket) {
        return;
    }

    QByteArray data = m_ipcSocket->readAll();
    QString message = QString::fromUtf8(data);

    // 解析日志消息（格式 "LOG:message"）
    if (message.startsWith("LOG:")) {
        QString logMessage = message.mid(4);
        appendLog(logMessage);
    }
    // 解析状态消息（格式 "STATUS:status,pid,procName,minimized,intervalMs"）
    else if (message.startsWith("STATUS:")) {
        QString body = message.mid(7);
        QStringList parts = body.split(QLatin1Char(','));
        if (parts.size() >= 5) {
            QString status = parts[0];
            QString procName = parts[2];
            int intervalMs = parts[4].toInt();
            m_lastIntervalMs = intervalMs;

            // 更新状态显示
            QString display;
            if (m_isServerRunning) {
                if (status == QStringLiteral("blocking")) {
                    display = tr("阻止中");
                    if (!procName.isEmpty())
                        display += tr("（活动窗口: %1）").arg(procName);
                    m_statusValueLabel->setStyleSheet("color: #2ecc71; font-weight: bold;");
                } else if (status == QStringLiteral("idle")) {
                    display = tr("等待中（无匹配窗口）");
                    m_statusValueLabel->setStyleSheet("color: #f39c12; font-weight: bold;");
                } else {
                    display = tr("已禁用智能模式");
                    m_statusValueLabel->setStyleSheet("color: #999;");
                }
            } else {
                display = tr("已停止");
                m_statusValueLabel->setStyleSheet("color: #999;");
            }
            m_statusValueLabel->setText(display);

            // 更新间隔显示
            int intervalSec = intervalMs / 1000;
            if (intervalSec >= 60)
                m_delayInfoValue->setText(tr("当前间隔: %1 分 %2 秒")
                    .arg(intervalSec / 60).arg(intervalSec % 60));
            else
                m_delayInfoValue->setText(tr("当前间隔: %1 秒").arg(intervalSec));
        }
    }
}

/**
 * @brief 连接错误槽函数
 */
void MainWindow::onSocketError()
{
    if (!m_ipcSocket) {
        return;
    }

    QString errorString = m_ipcSocket->errorString();
    appendLog(tr("IPC 连接错误: %1").arg(errorString));
}

/**
 * @brief 发送配置到守护进程
 */
void MainWindow::sendConfigToDaemon(int minutes)
{
    if (!m_ipcSocket || m_ipcSocket->state() != QLocalSocket::ConnectedState) {
        return;
    }

    QString message = QString("CONFIG:%1").arg(minutes);
    m_ipcSocket->write(message.toUtf8());
    m_ipcSocket->flush();
}

// ---- 智能模式发送 ----

void MainWindow::sendSmartConfig()
{
    if (!m_ipcSocket || m_ipcSocket->state() != QLocalSocket::ConnectedState) return;
    QString msg = QStringLiteral("SMART:%1").arg(m_smartSwitch->isChecked() ? 1 : 0);
    m_ipcSocket->write(msg.toUtf8());
    m_ipcSocket->flush();
}

void MainWindow::sendTargetConfig()
{
    if (!m_ipcSocket || m_ipcSocket->state() != QLocalSocket::ConnectedState) return;
    QString msg = QStringLiteral("TARGET:%1").arg(m_targetEdit->text().trimmed());
    m_ipcSocket->write(msg.toUtf8());
    m_ipcSocket->flush();
}

void MainWindow::sendAutoIntervalConfig()
{
    if (!m_ipcSocket || m_ipcSocket->state() != QLocalSocket::ConnectedState) return;
    QString msg = QStringLiteral("AUTO:%1").arg(m_autoIntervalSwitch->isChecked() ? 1 : 0);
    m_ipcSocket->write(msg.toUtf8());
    m_ipcSocket->flush();
}

// ---- 智能模式槽 ----

void MainWindow::onSmartToggled(bool checked)
{
    m_targetEdit->setEnabled(checked);
    m_autoIntervalSwitch->setEnabled(checked);
    m_valueSpinBox->setEnabled(!checked || !m_autoIntervalSwitch->isChecked());
    sendSmartConfig();
    appendLog(checked ? tr("智能阻止已启用") : tr("智能阻止已禁用"));
}

void MainWindow::onAutoIntervalToggled(bool checked)
{
    m_valueSpinBox->setEnabled(!checked);
    sendAutoIntervalConfig();
    appendLog(checked ? tr("自动同步熄屏时间已启用") : tr("自动同步熄屏时间已禁用"));
}

void MainWindow::onTargetProcessesEdited()
{
    sendTargetConfig();
    appendLog(tr("目标进程列表已更新: %1").arg(m_targetEdit->text()));
}
