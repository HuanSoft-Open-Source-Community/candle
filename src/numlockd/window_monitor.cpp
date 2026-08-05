#include "window_monitor.h"

#include <QProcessEnvironment>

// 前置声明
class X11WindowMonitor;
class TreelandWindowMonitor;

WindowMonitor *createWindowMonitor(QObject *parent)
{
    auto env = QProcessEnvironment::systemEnvironment();
    QString sessionType = env.value(QStringLiteral("XDG_SESSION_TYPE"));

    if (sessionType == QStringLiteral("wayland")) {
#ifdef HAVE_WAYLAND
        auto *monitor = new TreelandWindowMonitor(parent);
        return monitor;
#else
        Q_UNUSED(parent);
        return nullptr;
#endif
    }

    // X11 或未检测到会话类型
#ifdef HAVE_XCB
    auto *monitor = new X11WindowMonitor(parent);
    return monitor;
#else
    Q_UNUSED(parent);
    return nullptr;
#endif
}
