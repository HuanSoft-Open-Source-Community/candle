#ifndef WINDOW_MONITOR_H
#define WINDOW_MONITOR_H

#include <QObject>
#include <QString>

/**
 * @brief 活动窗口信息结构
 */
struct ActiveWindowInfo {
    int pid = -1;               ///< 窗口所属进程 PID，-1 表示无活动窗口
    QString processName;        ///< 进程名称（从 /proc/<pid>/comm 读取）
    bool minimized = false;     ///< 窗口是否处于最小化状态
    bool valid = false;         ///< 信息是否有效

    bool operator==(const ActiveWindowInfo &other) const {
        return pid == other.pid && minimized == other.minimized && valid == other.valid;
    }
    bool operator!=(const ActiveWindowInfo &other) const { return !(*this == other); }
};

/**
 * @brief 活动窗口检测抽象基类
 *
 * 通过运行时探测会话类型（X11 / Wayland + Treeland）自动选择实现。
 * X11 会话用 EWMH 标准（xcb）；deepin 25 Treeland Wayland 用私有协议
 * treeland-foreign-toplevel-manager-v1。
 */
class WindowMonitor : public QObject
{
    Q_OBJECT

public:
    explicit WindowMonitor(QObject *parent = nullptr) : QObject(parent) {}
    ~WindowMonitor() override = default;

    /**
     * @brief 初始化监视器（打开连接、注册事件监听）
     * @return 是否初始化成功
     */
    virtual bool init() = 0;

    /**
     * @brief 获取当前活动窗口信息（同步读取）
     */
    virtual ActiveWindowInfo currentInfo() const = 0;

    /**
     * @brief 监视器是否可用
     */
    bool isAvailable() const { return m_available; }

signals:
    /**
     * @brief 活动窗口发生变化时发出
     * @param info 新的活动窗口信息
     */
    void activeWindowChanged(const ActiveWindowInfo &info);

protected:
    bool m_available = false;
};

/**
 * @brief 自动检测当前会话类型并创建对应的 WindowMonitor 实例
 *
 * 探测顺序：
 * 1. XDG_SESSION_TYPE == "x11" 或未设置 → X11WindowMonitor
 * 2. XDG_SESSION_TYPE == "wayland" → TreelandWindowMonitor
 *
 * 调用方应在构造后调用 init() 检查是否可用。
 * 返回的指针由调用方管理（需 delete 或设置 parent）。
 */
WindowMonitor *createWindowMonitor(QObject *parent = nullptr);

#endif // WINDOW_MONITOR_H
