#ifndef SMART_BLOCKER_H
#define SMART_BLOCKER_H

#include <QObject>
#include <QStringList>
#include "window_monitor.h"

/**
 * @brief 智能阻止判定引擎
 *
 * 根据以下条件决定是否应发送 NumLock 按键：
 * 1. 智能模式是否启用
 * 2. 目标进程列表是否非空
 * 3. 当前活动窗口的进程名是否匹配目标列表
 * 4. 活动窗口是否未最小化
 *
 * 同时管理时间间隔计算：
 * - 自动模式：interval = 熄屏延时 − 1 秒（下限 5 秒）
 * - 手动模式：使用用户配置分钟数
 */
class SmartBlocker : public QObject
{
    Q_OBJECT

public:
    explicit SmartBlocker(QObject *parent = nullptr);

    // ---- 配置 ----
    void setSmartEnabled(bool enabled);
    bool isSmartEnabled() const { return m_smartEnabled; }

    void setAutoInterval(bool enabled);
    bool isAutoInterval() const { return m_autoInterval; }

    void setTargetProcesses(const QStringList &names);
    QStringList targetProcesses() const { return m_targetProcesses; }

    void setManualIntervalMinutes(int minutes);
    int manualIntervalMinutes() const { return m_manualIntervalMinutes; }

    // ---- 运行时状态 ----
    void updateActiveWindow(const ActiveWindowInfo &info);
    ActiveWindowInfo activeWindow() const { return m_activeWindow; }

    void setScreenBlackDelay(int seconds);
    int screenBlackDelay() const { return m_screenBlackDelay; }

    /**
     * @brief 当前是否应阻止（每轮定时器触发时调用）
     */
    bool shouldBlock() const;

    /**
     * @brief 当前有效的定时器间隔（毫秒）
     *
     * 自动模式：m_screenBlackDelay − 1 秒（下限 5 秒）
     * 手动模式：m_manualIntervalMinutes 分钟
     */
    int effectiveIntervalMs() const;

    /**
     * @brief 当前阻止状态描述（供 GUI 展示）
     * @return "blocking" | "idle" | "disabled"
     */
    QString blockingStatus() const;

signals:
    /**
     * @brief 阻止状态发生变化（阻止/空闲/禁用）
     */
    void blockingStateChanged(const QString &status);

private:
    bool matchesTarget(const QString &procName) const;

    // 配置
    bool        m_smartEnabled = false;
    bool        m_autoInterval = false;
    QStringList m_targetProcesses;
    int         m_manualIntervalMinutes = 15;

    // 运行时
    ActiveWindowInfo m_activeWindow;
    int              m_screenBlackDelay = -1; // 秒，-1 = 不可读
    mutable bool     m_lastBlocking = false; // 去重变化检测
};

#endif // SMART_BLOCKER_H
