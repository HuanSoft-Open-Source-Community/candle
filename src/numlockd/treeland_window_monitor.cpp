#ifdef HAVE_WAYLAND

#include "treeland_window_monitor.h"

#include <wayland-client.h>
#include <wayland-client-protocol.h>

#include <QDebug>
#include <QFile>
#include <QProcessEnvironment>
#include <QSocketNotifier>
#include <QTimer>
#include <unistd.h>
#include <cstdlib>
#include <cstring>

// ============================================================================
// Wayland 协议手动绑定（无 wayland-scanner 代码生成依赖）
//
// struct wl_interface 标准定义（wayland-util.h）：
//   { name, version, method_count, methods, event_count, events }
// 定义顺序避免 C++ 循环引用：events → types → interface。
// ============================================================================

// ---- treeland_foreign_toplevel_handle_v1 ----
// types 数组（输出对象接口；wl_output_interface 来自 wayland-client-protocol.h）
static const struct wl_interface *handle_types[] = {
    &wl_output_interface,   // output_enter / output_leave
    &wl_seat_interface,     // activate 请求
};

static const struct wl_message handle_events[] = {
    { "pid",          "u",  nullptr },
    { "title",        "s",  nullptr },
    { "app_id",       "s",  nullptr },
    { "identifier",   "u",  nullptr },
    { "output_enter", "o",  handle_types + 0 },
    { "output_leave", "o",  handle_types + 0 },
    { "state",        "a",  nullptr },
    { "done",         "",   nullptr },
    { "closed",       "",   nullptr },
    { "parent",       "?o", nullptr },
};

static const struct wl_message handle_requests[] = {
    { "set_maximized",    "",   nullptr },
    { "unset_maximized",  "",   nullptr },
    { "set_minimized",    "",   nullptr },
    { "unset_minimized",  "",   nullptr },
    { "activate",         "o",  handle_types + 1 },
    { "close",            "",   nullptr },
    { "set_rectangle",    "oiiii", nullptr },
    { "destroy",          "",   nullptr },   // destructor
    { "set_fullscreen",   "?o", nullptr },
    { "unset_fullscreen", "",   nullptr },
};

static const struct wl_interface treeland_foreign_toplevel_handle_v1_interface = {
    "treeland_foreign_toplevel_handle_v1", 2,
    10, handle_requests,
    10, handle_events,
};

// ---- treeland_foreign_toplevel_manager_v1 ----
static const struct wl_interface *manager_types[] = {
    &treeland_foreign_toplevel_handle_v1_interface,   // toplevel 事件 new_id
};

static const struct wl_message manager_requests[] = {
    { "stop", "", nullptr },
};

static const struct wl_message manager_events[] = {
    { "toplevel", "n", manager_types + 0 },
    { "finished", "",  nullptr },
};

static const struct wl_interface treeland_foreign_toplevel_manager_v1_interface = {
    "treeland_foreign_toplevel_manager_v1", 2,
    1, manager_requests,
    2, manager_events,
};

// 状态枚举值（与 treeland_foreign_toplevel_handle_v1::state 对齐）
enum : uint32_t {
    STATE_MAXIMIZED  = 0,
    STATE_MINIMIZED  = 1,
    STATE_ACTIVATED  = 2,
    STATE_FULLSCREEN = 3,
    STATE_ATTENTION  = 4,
};

// ============================================================================
// 每个 handle 的独立上下文（传递给分发器）
// ============================================================================
struct HandleContext {
    TreelandWindowMonitor *monitor = nullptr;
    struct wl_proxy         *proxy   = nullptr;
    uint32_t                 pid     = 0;
    QString                  appId;
    bool                     activated = false;
    bool                     minimized = false;
};

// ============================================================================
// TreelandWindowMonitor 实现
// ============================================================================

TreelandWindowMonitor::TreelandWindowMonitor(QObject *parent)
    : WindowMonitor(parent)
{
}

TreelandWindowMonitor::~TreelandWindowMonitor()
{
    cleanup();
}

bool TreelandWindowMonitor::init()
{
    if (m_display) {
        qWarning() << "TreelandWindowMonitor: already initialized";
        return false;
    }

    // 连接 Wayland display
    m_display = wl_display_connect(nullptr);
    if (!m_display) {
        qWarning() << "TreelandWindowMonitor: failed to connect to Wayland display";
        return false;
    }

    // 获取 registry 并设置监听
    m_registry = wl_display_get_registry(m_display);
    if (!m_registry) {
        qWarning() << "TreelandWindowMonitor: failed to get registry";
        cleanup();
        return false;
    }
    static const struct wl_registry_listener registryListener = {
        &TreelandWindowMonitor::onRegistryGlobal,
        &TreelandWindowMonitor::onRegistryGlobalRemove,
    };
    wl_registry_add_listener(m_registry, &registryListener, this);

    // 第一轮同步：获取全局列表
    if (wl_display_roundtrip(m_display) < 0) {
        qWarning() << "TreelandWindowMonitor: roundtrip failed";
        cleanup();
        return false;
    }

    if (!m_manager) {
        qWarning() << "TreelandWindowMonitor: treeland_foreign_toplevel_manager_v1 not available";
        cleanup();
        return false;
    }

    // 第二轮同步：等待 toplevel 初始事件
    if (wl_display_roundtrip(m_display) < 0) {
        qWarning() << "TreelandWindowMonitor: second roundtrip failed";
        cleanup();
        return false;
    }

    // QSocketNotifier 事件驱动
    int fd = wl_display_get_fd(m_display);
    if (fd < 0) {
        qWarning() << "TreelandWindowMonitor: failed to get display fd";
        cleanup();
        return false;
    }
    m_notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated,
            this, &TreelandWindowMonitor::onWaylandEvent);

    // 兜底定时器
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(500);
    connect(m_pollTimer, &QTimer::timeout,
            this, &TreelandWindowMonitor::onPollTimer);
    m_pollTimer->start();

    m_available = true;
    m_lastInfo = currentInfo();
    qDebug() << "TreelandWindowMonitor: initialized, fd:" << fd
             << "toplevels:" << m_toplevels.size();
    return true;
}

ActiveWindowInfo TreelandWindowMonitor::currentInfo() const
{
    return m_lastInfo;
}

void TreelandWindowMonitor::onWaylandEvent()
{
    // 1. 处理已有待处理事件（获取读锁前）
    if (wl_display_dispatch_pending(m_display) < 0) {
        qWarning() << "TreelandWindowMonitor: dispatch_pending failed (pre-read)";
        m_available = false;
        return;
    }

    // 2. 获取读锁
    int prepareRet = wl_display_prepare_read(m_display);
    if (prepareRet < 0) {
        qWarning() << "TreelandWindowMonitor: prepare_read failed, display disconnected";
        m_available = false;
        return;
    }
    // prepareRet > 0: 有新事件入队，先处理掉再重试
    if (prepareRet > 0) {
        if (wl_display_dispatch_pending(m_display) < 0) {
            m_available = false;
            return;
        }
        // 重试获取读锁
        prepareRet = wl_display_prepare_read(m_display);
        if (prepareRet < 0) {
            m_available = false;
            return;
        }
        if (prepareRet > 0) {
            // 仍有事件，放弃本轮（下次 socket 激活再试）
            wl_display_cancel_read(m_display);
            return;
        }
    }

    // 3. flush → read events → dispatch
    if (wl_display_flush(m_display) < 0) {
        wl_display_cancel_read(m_display);
        qWarning() << "TreelandWindowMonitor: flush failed";
        m_available = false;
        return;
    }
    if (wl_display_read_events(m_display) < 0) {
        // read_events 失败已隐式释放锁
        qWarning() << "TreelandWindowMonitor: read_events failed";
        m_available = false;
        return;
    }
    if (wl_display_dispatch_pending(m_display) < 0) {
        qWarning() << "TreelandWindowMonitor: dispatch_pending failed (post-read)";
        m_available = false;
        return;
    }
}

void TreelandWindowMonitor::onPollTimer()
{
    // 兜底：事件驱动为主，此定时器仅作极低概率兜底
}

void TreelandWindowMonitor::cleanup()
{
    m_available = false;
    delete m_pollTimer;
    m_pollTimer = nullptr;
    delete m_notifier;
    m_notifier = nullptr;

    // 清理 handle 上下文（先销毁 proxy 再释放上下文）
    for (auto *ctx : m_toplevels) {
        if (ctx->proxy) {
            wl_proxy_destroy(ctx->proxy);
            ctx->proxy = nullptr;
        }
        delete ctx;
    }
    m_toplevels.clear();
    m_activeHandle = nullptr;

    if (m_manager) {
        wl_proxy_destroy(m_manager);
        m_manager = nullptr;
    }
    if (m_output) {
        wl_proxy_destroy(m_output);
        m_output = nullptr;
    }
    if (m_registry) {
        wl_registry_destroy(m_registry);
        m_registry = nullptr;
    }
    if (m_display) {
        wl_display_disconnect(m_display);
        m_display = nullptr;
    }
}

// ============================================================================
// Wayland 协议回调（static 成员函数，与 C 函数指针兼容）
// ============================================================================

void TreelandWindowMonitor::onRegistryGlobal(void *data, struct wl_registry *registry,
                                              uint32_t name, const char *interface,
                                              uint32_t version)
{
    auto *self = static_cast<TreelandWindowMonitor *>(data);

    // 事件中的 'o' 参数对象必须已 bind 且有非空 implementation，
    // 否则 wl_closure_lookup_objects 在 dispatch 前返回 EINVAL。
    // 这里对 wl_output 做空绑定，只为让对象存在于 display map 中。
    if (std::strcmp(interface, "wl_output") == 0) {
        // 多输出场景：先释放旧的空绑定 proxy，避免覆盖泄漏
        if (self->m_output) {
            wl_proxy_destroy(self->m_output);
            self->m_output = nullptr;
        }
        struct wl_proxy *out = static_cast<struct wl_proxy *>(
            wl_registry_bind(registry, name, &wl_output_interface,
                             (version < 4) ? version : 4));
        if (!out) return;
        // implementation 传非空占位指针（noopDispatcher 忽略全部参数）
        static char dummyImpl;
        wl_proxy_add_dispatcher(out, &noopDispatcher, &dummyImpl, nullptr);
        self->m_output = out;
        return;
    }

    if (std::strcmp(interface, "treeland_foreign_toplevel_manager_v1") == 0) {
        uint32_t v = (version < 2) ? version : 2;
        struct wl_proxy *mgr = static_cast<struct wl_proxy *>(
            wl_registry_bind(registry, name,
                             &treeland_foreign_toplevel_manager_v1_interface, v));
        if (!mgr) return;
        // implementation 传 self（非空），data 传 self（经 user_data 获取）
        wl_proxy_add_dispatcher(mgr, &TreelandWindowMonitor::managerDispatcher,
                                self, self);
        self->m_manager = mgr;
        qDebug() << "TreelandMonitor: bound manager v" << v;
    }
}

void TreelandWindowMonitor::onRegistryGlobalRemove(void *, struct wl_registry *,
                                                    uint32_t)
{
    // Treeland 管理器全局被移除 —— 罕见情况，忽略
}

// wl_output 空绑定占位分发器：不处理任何事件
int TreelandWindowMonitor::noopDispatcher(const void *, void *, uint32_t,
                                           const struct wl_message *,
                                           union wl_argument *)
{
    return 0;
}

int TreelandWindowMonitor::managerDispatcher(const void *, void *target,
                                              uint32_t opcode,
                                              const struct wl_message *,
                                              union wl_argument *args)
{
    // 契约：target 是收到事件的 wl_proxy*，用户数据经 user_data 获取
    auto *proxy = static_cast<struct wl_proxy *>(target);
    auto *self = static_cast<TreelandWindowMonitor *>(
        wl_proxy_get_user_data(proxy));
    if (!self) return 0;

    if (opcode == 0) {  // toplevel → 新 handle
        struct wl_proxy *hp = reinterpret_cast<struct wl_proxy *>(args[0].o);
        if (!hp) return 0;

        auto *ctx = new HandleContext;
        ctx->monitor = self;
        ctx->proxy   = hp;
        // implementation 传 self（非空），data 传 ctx（经 user_data 获取）
        wl_proxy_add_dispatcher(hp, &TreelandWindowMonitor::handleDispatcher,
                                self, ctx);
        self->m_toplevels.insert(hp, ctx);
        qDebug() << "TreelandMonitor: toplevel handle created";
    }
    // opcode 1 = finished, 忽略
    return 0;
}

int TreelandWindowMonitor::handleDispatcher(const void *, void *target,
                                             uint32_t opcode,
                                             const struct wl_message *,
                                             union wl_argument *args)
{
    // 契约：target 是收到事件的 wl_proxy*，HandleContext 经 user_data 获取
    auto *proxy = static_cast<struct wl_proxy *>(target);
    auto *ctx = static_cast<HandleContext *>(
        wl_proxy_get_user_data(proxy));
    if (!ctx || !ctx->monitor) return 0;
    auto *self = ctx->monitor;

    switch (opcode) {
    case 0: // pid
        ctx->pid = args[0].u;
        break;
    case 2: // app_id
        ctx->appId = QString::fromUtf8(args[0].s);
        break;
    case 6: { // state → array of uint32
        struct wl_array *arr = args[0].a;
        if (!arr || arr->size == 0) break;
        bool wasActivated = ctx->activated;
        bool wasMinimized = ctx->minimized;
        ctx->activated = false;
        ctx->minimized = false;
        uint32_t *states = static_cast<uint32_t *>(arr->data);
        size_t count = arr->size / sizeof(uint32_t);
        for (size_t i = 0; i < count; ++i) {
            if (states[i] == STATE_ACTIVATED) ctx->activated = true;
            if (states[i] == STATE_MINIMIZED) ctx->minimized = true;
        }
        if (ctx->activated != wasActivated || ctx->minimized != wasMinimized) {
            self->onHandleStateChanged(ctx->proxy);
        }
        break;
    }
    case 7: // done → 初始数据发送完毕
        self->onHandleStateChanged(ctx->proxy);
        break;
    case 8: { // closed → handle 销毁
        struct wl_proxy *deadProxy = ctx->proxy;
        self->m_toplevels.remove(deadProxy);
        if (self->m_activeHandle == deadProxy) {
            self->m_activeHandle = nullptr;
            self->emitActiveWindowChanged();
        }
        ctx->proxy = nullptr;
        wl_proxy_destroy(deadProxy);
        delete ctx;
        break;
    }
    }
    return 0;
}

// ============================================================================
// 供 dispatcher 回调的公共方法
// ============================================================================

void TreelandWindowMonitor::onHandleStateChanged(struct wl_proxy *proxy)
{
    auto *ctx = m_toplevels.value(proxy, nullptr);
    if (!ctx) return;

    if (ctx->activated) {
        if (m_activeHandle != proxy) {
            m_activeHandle = proxy;
            emitActiveWindowChanged();
        }
    } else if (m_activeHandle == proxy) {
        // 当前激活的 handle 失去了激活状态 → 扫描是否有其他激活的 handle
        m_activeHandle = nullptr;
        for (auto it = m_toplevels.begin(); it != m_toplevels.end(); ++it) {
            if (it.value()->activated) {
                m_activeHandle = it.key();
                break;
            }
        }
        emitActiveWindowChanged();
    }
}

void TreelandWindowMonitor::emitActiveWindowChanged()
{
    ActiveWindowInfo info;
    auto *ctx = m_toplevels.value(m_activeHandle, nullptr);
    if (ctx && ctx->pid > 0) {
        info.pid = static_cast<int>(ctx->pid);
        info.minimized = ctx->minimized;
        // 从 /proc/<pid>/comm 读取进程名（更精确）
        QFile f(QStringLiteral("/proc/%1/comm").arg(info.pid));
        if (f.open(QIODevice::ReadOnly)) {
            info.processName = QString::fromUtf8(f.read(256)).trimmed();
        }
        // 备选：用 appId
        if (info.processName.isEmpty() && !ctx->appId.isEmpty()) {
            info.processName = ctx->appId;
        }
        info.valid = true;
    }
    if (info != m_lastInfo) {
        m_lastInfo = info;
        emit activeWindowChanged(info);
    }
}

#endif // HAVE_WAYLAND
