#include "window_monitor.h"

#include <QProcessEnvironment>

#ifdef HAVE_XCB
#include "x11_window_monitor.h"
#endif
#ifdef HAVE_WAYLAND
#include "treeland_window_monitor.h"
#endif

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
