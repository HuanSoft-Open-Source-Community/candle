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
 * Wayland 协议分发器与注册表监听实现为 static 成员函数
 * （与 C 回调函数指针兼容，且可访问私有成员）。
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

    // ---- Wayland 协议回调（static 成员，与 C 函数指针兼容） ----
    static void onRegistryGlobal(void *data, struct wl_registry *registry,
                                 uint32_t name, const char *interface,
                                 uint32_t version);
    static void onRegistryGlobalRemove(void *data, struct wl_registry *registry,
                                       uint32_t name);
    static int managerDispatcher(const void *implementation, void *target,
                                 uint32_t opcode, const struct wl_message *msg,
                                 union wl_argument *args);
    static int handleDispatcher(const void *implementation, void *target,
                                uint32_t opcode, const struct wl_message *msg,
                                union wl_argument *args);
    /** wl_output 空绑定占位分发器 */
    static int noopDispatcher(const void *implementation, void *target,
                              uint32_t opcode, const struct wl_message *msg,
                              union wl_argument *args);

    struct wl_display   *m_display   = nullptr;
    struct wl_registry  *m_registry  = nullptr;
    struct wl_proxy     *m_manager   = nullptr;
    struct wl_proxy     *m_output    = nullptr;   // wl_output 空绑定（防泄漏）
    QSocketNotifier     *m_notifier  = nullptr;
    QTimer              *m_pollTimer = nullptr;

    // key = wl_proxy*, value = HandleContext*（在 .cpp 中定义）
    QHash<struct wl_proxy *, struct HandleContext *> m_toplevels;
    struct wl_proxy *m_activeHandle = nullptr;

    ActiveWindowInfo m_lastInfo;
};

#endif // TREELAND_WINDOW_MONITOR_H
