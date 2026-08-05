#ifdef HAVE_XCB

#include "x11_window_monitor.h"

#include <xcb/xcb.h>
#include <xcb/xproto.h>

#include <QDebug>
#include <QFile>
#include <QSocketNotifier>
#include <QTimer>
#include <unistd.h>

// ---------------------------------------------------------------------------
// X11WindowMonitor 实现
// ---------------------------------------------------------------------------

X11WindowMonitor::X11WindowMonitor(QObject *parent)
    : WindowMonitor(parent)
{
}

X11WindowMonitor::~X11WindowMonitor()
{
    m_available = false;
    delete m_pollTimer;
    delete m_notifier;
    if (m_conn) {
        xcb_disconnect(m_conn);
        m_conn = nullptr;
    }
}

bool X11WindowMonitor::init()
{
    if (m_conn) {
        qWarning() << "X11WindowMonitor: already initialized";
        return false;
    }

    // --- 打开 X11 连接 ---
    int screenNum = 0;
    m_conn = xcb_connect(nullptr, &screenNum);
    if (xcb_connection_has_error(m_conn)) {
        qWarning() << "X11WindowMonitor: failed to connect to X server";
        xcb_disconnect(m_conn);
        m_conn = nullptr;
        return false;
    }

    // --- 获取 root 窗口 ---
    const xcb_setup_t *setup = xcb_get_setup(m_conn);
    if (!setup) {
        qWarning() << "X11WindowMonitor: failed to get X11 setup";
        xcb_disconnect(m_conn);
        m_conn = nullptr;
        return false;
    }
    xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
    for (int i = 0; i < screenNum && iter.rem; ++i) {
        xcb_screen_next(&iter);
    }
    if (!iter.data) {
        qWarning() << "X11WindowMonitor: no screen found";
        xcb_disconnect(m_conn);
        m_conn = nullptr;
        return false;
    }
    m_rootWindow = iter.data->root;

    // --- 注册 atoms ---
    auto internAtom = [this](const char *name) -> xcb_atom_t {
        xcb_intern_atom_cookie_t cookie = xcb_intern_atom(
            m_conn, 0, static_cast<uint16_t>(qstrlen(name)), name);
        xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(m_conn, cookie, nullptr);
        if (!reply) return XCB_ATOM_NONE;
        xcb_atom_t atom = reply->atom;
        free(reply);
        return atom;
    };

    m_atomNetActiveWindow  = internAtom("_NET_ACTIVE_WINDOW");
    m_atomNetWmPid         = internAtom("_NET_WM_PID");
    m_atomNetWmState       = internAtom("_NET_WM_STATE");
    m_atomNetWmStateHidden = internAtom("_NET_WM_STATE_HIDDEN");

    if (!m_atomNetActiveWindow) {
        qWarning() << "X11WindowMonitor: _NET_ACTIVE_WINDOW atom not available";
        xcb_disconnect(m_conn);
        m_conn = nullptr;
        return false;
    }

    // --- 监听 root 窗口的 PropertyNotify 事件 ---
    uint32_t mask = XCB_CW_EVENT_MASK;
    uint32_t values[] = { XCB_EVENT_MASK_PROPERTY_CHANGE };
    xcb_change_window_attributes(m_conn, m_rootWindow, mask, values);
    xcb_flush(m_conn);

    // --- 设置 QSocketNotifier 监听 xcb fd ---
    int fd = xcb_get_file_descriptor(m_conn);
    if (fd < 0) {
        qWarning() << "X11WindowMonitor: failed to get xcb file descriptor";
        xcb_disconnect(m_conn);
        m_conn = nullptr;
        return false;
    }
    m_notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &X11WindowMonitor::onX11Event);

    // --- 兜底轮询定时器 ---
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(500);
    connect(m_pollTimer, &QTimer::timeout, this, &X11WindowMonitor::onPollTimer);
    m_pollTimer->start();

    // --- 读取初始状态 ---
    m_lastInfo = readActiveWindowInfo();

    m_available = true;
    qDebug() << "X11WindowMonitor: initialized successfully, fd:" << fd;
    return true;
}

ActiveWindowInfo X11WindowMonitor::currentInfo() const
{
    // 返回缓存的最新值（由事件驱动或定时器更新）
    return m_lastInfo;
}

// -------- 私有槽 --------

void X11WindowMonitor::onX11Event()
{
    while (true) {
        xcb_generic_event_t *event = xcb_poll_for_event(m_conn);
        if (!event) break;

        uint8_t responseType = event->response_type & ~0x80;
        if (responseType == XCB_PROPERTY_NOTIFY) {
            auto *pn = reinterpret_cast<xcb_property_notify_event_t *>(event);
            if (pn->atom == m_atomNetActiveWindow && pn->window == m_rootWindow) {
                ActiveWindowInfo info = readActiveWindowInfo();
                if (info != m_lastInfo) {
                    m_lastInfo = info;
                    emit activeWindowChanged(info);
                }
            }
        }
        free(event);
    }
}

void X11WindowMonitor::onPollTimer()
{
    ActiveWindowInfo info = readActiveWindowInfo();
    if (info != m_lastInfo) {
        m_lastInfo = info;
        emit activeWindowChanged(info);
    }
}

// -------- 内部方法 --------

ActiveWindowInfo X11WindowMonitor::readActiveWindowInfo() const
{
    ActiveWindowInfo info;

    xcb_window_t activeWin = 0;
    if (!getActiveWindow(activeWin) || activeWin == 0) {
        return info; // valid == false
    }

    info.pid = getWindowPid(activeWin);
    info.minimized = isWindowMinimized(activeWin);
    info.processName = readProcComm(info.pid);
    info.valid = true;
    return info;
}

bool X11WindowMonitor::getActiveWindow(xcb_window_t &window) const
{
    xcb_get_property_cookie_t cookie = xcb_get_property(
        m_conn, 0, m_rootWindow,
        m_atomNetActiveWindow, XCB_ATOM_WINDOW, 0, 1);

    xcb_get_property_reply_t *reply = xcb_get_property_reply(m_conn, cookie, nullptr);
    if (!reply || reply->type != XCB_ATOM_WINDOW || reply->format != 32) {
        free(reply);
        return false;
    }

    uint32_t *data = static_cast<uint32_t *>(xcb_get_property_value(reply));
    if (data && reply->length > 0) {
        window = *data;
    }
    free(reply);
    return window != 0;
}

int X11WindowMonitor::getWindowPid(xcb_window_t window) const
{
    xcb_get_property_cookie_t cookie = xcb_get_property(
        m_conn, 0, window,
        m_atomNetWmPid, XCB_ATOM_CARDINAL, 0, 1);

    xcb_get_property_reply_t *reply = xcb_get_property_reply(m_conn, cookie, nullptr);
    if (!reply || reply->type != XCB_ATOM_CARDINAL || reply->format != 32) {
        free(reply);
        return -1;
    }

    uint32_t *data = static_cast<uint32_t *>(xcb_get_property_value(reply));
    int pid = (data && reply->length > 0) ? static_cast<int>(*data) : -1;
    free(reply);
    return pid;
}

bool X11WindowMonitor::isWindowMinimized(xcb_window_t window) const
{
    if (!m_atomNetWmState || !m_atomNetWmStateHidden)
        return false;

    xcb_get_property_cookie_t cookie = xcb_get_property(
        m_conn, 0, window,
        m_atomNetWmState, XCB_ATOM_ATOM, 0, 64);

    xcb_get_property_reply_t *reply = xcb_get_property_reply(m_conn, cookie, nullptr);
    if (!reply || reply->type != XCB_ATOM_ATOM || reply->format != 32) {
        free(reply);
        return false;
    }

    uint32_t *atoms = static_cast<uint32_t *>(xcb_get_property_value(reply));
    int count = static_cast<int>(reply->length);
    for (int i = 0; i < count; ++i) {
        if (atoms[i] == m_atomNetWmStateHidden) {
            free(reply);
            return true;
        }
    }
    free(reply);
    return false;
}

QString X11WindowMonitor::readProcComm(int pid)
{
    if (pid <= 0)
        return QString();

    QFile f(QStringLiteral("/proc/%1/comm").arg(pid));
    if (!f.open(QIODevice::ReadOnly))
        return QString();

    return QString::fromUtf8(f.read(256)).trimmed();
}

#endif // HAVE_XCB