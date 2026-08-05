#ifndef X11_WINDOW_MONITOR_H
#define X11_WINDOW_MONITOR_H

#include "window_monitor.h"

#include <QTimer>

class QSocketNotifier;

typedef struct xcb_connection_t xcb_connection_t;
typedef uint32_t xcb_window_t;
typedef uint32_t xcb_atom_t;

/**
 * @brief 基于 EWMH 标准的 X11 活动窗口监视器
 *
 * 使用纯 xcb 库（无需 Qt GUI 模块），适配 QCoreApplication 守护进程。
 * 覆盖 deepin 23、UOS 20、UOS 25 等全部 X11 会话。
 *
 * 工作原理：
 * 1. 监听 root 窗口 _NET_ACTIVE_WINDOW 属性的 PropertyNotify 事件
 * 2. 事件触发后同步读取活动窗口的 _NET_WM_PID（PID）和 _NET_WM_STATE（最小化状态）
 * 3. QTimer 每 500ms 兜底轮询，防止事件丢失
 * 4. 进程名从 /proc/<pid>/comm 读取
 */
class X11WindowMonitor : public WindowMonitor
{
    Q_OBJECT

public:
    explicit X11WindowMonitor(QObject *parent = nullptr);
    ~X11WindowMonitor() override;

    bool init() override;
    ActiveWindowInfo currentInfo() const override;

private slots:
    void onX11Event();
    void onPollTimer();

private:
    ActiveWindowInfo readActiveWindowInfo() const;
    bool getActiveWindow(xcb_window_t &window) const;
    int getWindowPid(xcb_window_t window) const;
    bool isWindowMinimized(xcb_window_t window) const;
    static QString readProcComm(int pid);

    xcb_connection_t *m_conn = nullptr;
    QSocketNotifier *m_notifier = nullptr;
    QTimer *m_pollTimer = nullptr;

    xcb_window_t m_rootWindow = 0;
    xcb_atom_t m_atomNetActiveWindow = 0;
    xcb_atom_t m_atomNetWmPid = 0;
    xcb_atom_t m_atomNetWmState = 0;
    xcb_atom_t m_atomNetWmStateHidden = 0;

    ActiveWindowInfo m_lastInfo;
};

#endif // X11_WINDOW_MONITOR_H
