#include <QCoreApplication>
#include <QTimer>
#include <QCommandLineParser>
#include <QSettings>
#include <csignal>

#include <DLog>

#include "numlock_simulator.h"
#include "ipc_server.h"
#include "window_monitor.h"
#include "power_settings_reader.h"
#include "smart_blocker.h"

DCORE_USE_NAMESPACE

// ---- 配置持久化 key ----
static const char kSettingsGroup[]  = "SmartBlocker";
static const char kKeySmart[]       = "smartEnabled";
static const char kKeyAuto[]        = "autoInterval";
static const char kKeyTarget[]      = "targetProcesses";
static const char kKeyManualMin[]   = "manualIntervalMinutes";

static QCoreApplication *g_app = nullptr;

static void signalHandler(int sig)
{
    if (g_app) {
        dInfo() << "Received signal" << sig << ", shutting down gracefully...";
        g_app->quit();
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    g_app = &app;

    app.setApplicationName("numlockd");
    app.setOrganizationName("org.yxzl.candle");
    app.setApplicationVersion("1.1.0");

    DLogManager::registerConsoleAppender();
    DLogManager::registerFileAppender();

    // ---- 命令行参数 ----
    QCommandLineParser parser;
    parser.setApplicationDescription(
        "Numlock daemon — intelligent screen-block prevention");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption intervalOption(
        QStringList() << "i" << "interval",
        "Manual simulation interval in minutes (default: 15, ignored in auto mode)",
        "minutes", "15");
    parser.addOption(intervalOption);
    parser.process(app);

    int manualIntervalMin = parser.value(intervalOption).toInt();
    if (manualIntervalMin < 2) manualIntervalMin = 2;

    // ---- 持久化配置 ----
    QSettings settings("org.yxzl.candle", "numlockd");
    settings.beginGroup(kSettingsGroup);

    // ---- 初始化模块 ----
    NumlockSimulator simulator;
    if (!simulator.init()) {
        dError() << "Failed to initialize NumlockSimulator";
    }

    IpcServer ipcServer;
    if (!ipcServer.start()) {
        dWarning() << "Failed to start IPC server, continuing without IPC";
    }

    auto *monitor = createWindowMonitor(&app);
    if (monitor && !monitor->init()) {
        dWarning() << "WindowMonitor init failed — smart blocking unavailable";
    }

    PowerSettingsReader powerReader;
    powerReader.init();

    SmartBlocker blocker;

    // ---- 从 QSettings 恢复配置 ----
    blocker.setSmartEnabled(
        settings.value(kKeySmart, false).toBool());
    blocker.setAutoInterval(
        settings.value(kKeyAuto, false).toBool());
    blocker.setManualIntervalMinutes(
        settings.value(kKeyManualMin, manualIntervalMin).toInt());
    QString savedTargets = settings.value(kKeyTarget).toString();
    if (!savedTargets.isEmpty()) {
        blocker.setTargetProcesses(savedTargets.split(QLatin1Char(','),
                                          Qt::SkipEmptyParts));
    }
    settings.endGroup();

    // ---- 信号连接：IPC → 配置 ----
    QObject::connect(&ipcServer, &IpcServer::configReceived,
                     &blocker, &SmartBlocker::setManualIntervalMinutes);
    QObject::connect(&ipcServer, &IpcServer::smartReceived,
                     &blocker, &SmartBlocker::setSmartEnabled);
    QObject::connect(&ipcServer, &IpcServer::targetReceived,
                     &blocker, &SmartBlocker::setTargetProcesses);
    QObject::connect(&ipcServer, &IpcServer::autoIntervalReceived,
                     &blocker, &SmartBlocker::setAutoInterval);

    // ---- 信号连接：WindowMonitor → SmartBlocker ----
    if (monitor) {
        QObject::connect(monitor, &WindowMonitor::activeWindowChanged,
                         &blocker, &SmartBlocker::updateActiveWindow);
        // 初始同步
        blocker.updateActiveWindow(monitor->currentInfo());
    }

    // ---- 信号连接：PowerSettings → SmartBlocker ----
    QObject::connect(&powerReader, &PowerSettingsReader::screenBlackDelayChanged,
                     &blocker, &SmartBlocker::setScreenBlackDelay);
    if (powerReader.isAvailable()) {
        blocker.setScreenBlackDelay(powerReader.screenBlackDelay());
    }

    // ---- 状态推送到 GUI ----
    QObject::connect(&blocker, &SmartBlocker::blockingStateChanged,
                     [&](const QString &status) {
        ActiveWindowInfo info = blocker.activeWindow();
        ipcServer.sendStatus(status, info.pid, info.processName,
                             info.minimized, blocker.effectiveIntervalMs());
    });

    // ---- 定时器：智能阻止 ----
    QTimer timer;
    timer.setInterval(blocker.effectiveIntervalMs());
    dInfo() << "Initial interval:" << timer.interval() << "ms";

    QObject::connect(&timer, &QTimer::timeout, [&]() {
        if (blocker.shouldBlock()) {
            dInfo() << "Timer tick — blocking (active window:"
                    << blocker.activeWindow().processName << ")";
            simulator.simulateNumLock();
        } else {
            dInfo() << "Timer tick — skipped (no matching active window)";
        }
        // 动态更新间隔（自动模式可能因熄屏延时变化而调整）
        int ms = blocker.effectiveIntervalMs();
        if (ms != timer.interval()) {
            timer.setInterval(ms);
            dInfo() << "Interval updated to" << ms << "ms";
        }
    });

    // ---- 保存配置（退出时持久化） ----
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&]() {
        QSettings s("org.yxzl.candle", "numlockd");
        s.beginGroup(kSettingsGroup);
        s.setValue(kKeySmart, blocker.isSmartEnabled());
        s.setValue(kKeyAuto, blocker.isAutoInterval());
        s.setValue(kKeyManualMin, blocker.manualIntervalMinutes());
        s.setValue(kKeyTarget, blocker.targetProcesses().join(QLatin1Char(',')));
        s.endGroup();
        s.sync();
    });

    // ---- 信号处理 ----
    std::signal(SIGTERM, signalHandler);
    std::signal(SIGINT, signalHandler);

    timer.start();
    // 推送初始状态
    ActiveWindowInfo initInfo = blocker.activeWindow();
    ipcServer.sendStatus(blocker.blockingStatus(), initInfo.pid,
                         initInfo.processName, initInfo.minimized,
                         blocker.effectiveIntervalMs());

    dInfo() << "numlockd started successfully"
            << "smart:" << blocker.isSmartEnabled()
            << "auto:" << blocker.isAutoInterval()
            << "targets:" << blocker.targetProcesses();

    return app.exec();
}
