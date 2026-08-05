#ifndef TREELAND_WINDOW_MONITOR_H
#define TREELAND_WINDOW_MONITOR_H

#include "window_monitor.h"

#include <QHash>
#include <QTimer>

class QSocketNotifier;

struct wl_display;
struct wl_registry;
struct wl_proxy;
struct HandleContext;

/**
 * @brief deepin 25 Treeland 合成器活动窗口监视器
 *
 * 使用 treeland-foreign-toplevel-manager-v1 私有 Wayland 协议
 * 获取活动窗口的 PID、最小化状态。仅覆盖 deepin 25 Treeland 会话。
 *
 * 依赖 libwayland-client，无 wayland-scanner 代码生成依赖。
 */
class TreelandWindowMonitor : public WindowMonitor
{
    Q_OBJECT

public:
    explicit TreelandWindowMonitor(QObject *parent = nullptr);
    ~TreelandWindowMonitor() override;

    bool init() override;
    ActiveWindowInfo currentInfo() const override;

    // ---- 供 Wayland 协议分发器回调的公开方法 ----
    void onHandleStateChanged(struct wl_proxy *proxy);
    void emitActiveWindowChanged();

private slots:
    void onWaylandEvent();
    void onPollTimer();

private:
    void cleanup();

    struct wl_display   *m_display   = nullptr;
    struct wl_registry  *m_registry  = nullptr;
    struct wl_proxy     *m_manager   = nullptr;
    QSocketNotifier     *m_notifier  = nullptr;
    QTimer              *m_pollTimer = nullptr;

    // key = wl_proxy*, value = HandleContext*（在 .cpp 中定义）
    QHash<struct wl_proxy *, struct HandleContext *> m_toplevels;
    struct wl_proxy *m_activeHandle = nullptr;

    ActiveWindowInfo m_lastInfo;

    friend int managerDispatcherImpl(const void *, void *, uint32_t,
                                     const struct wl_message *, union wl_argument *);
    friend int handleDispatcherImpl(const void *, void *, uint32_t,
                                    const struct wl_message *, union wl_argument *);
};

#endif // TREELAND_WINDOW_MONITOR_H
