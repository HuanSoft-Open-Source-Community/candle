#include "smart_blocker.h"

#include <QDebug>

SmartBlocker::SmartBlocker(QObject *parent)
    : QObject(parent)
{
}

// ---- 配置 ----

void SmartBlocker::setSmartEnabled(bool enabled)
{
    if (m_smartEnabled != enabled) {
        m_smartEnabled = enabled;
        emit blockingStateChanged(blockingStatus());
    }
}

void SmartBlocker::setAutoInterval(bool enabled)
{
    m_autoInterval = enabled;
}

void SmartBlocker::setTargetProcesses(const QStringList &names)
{
    m_targetProcesses = names;
    // 目标的进程列表变化不触发状态变化（等一下个定时器周期）
}

void SmartBlocker::setManualIntervalMinutes(int minutes)
{
    if (minutes < 2) minutes = 2;
    m_manualIntervalMinutes = minutes;
}

// ---- 运行时 ----

void SmartBlocker::updateActiveWindow(const ActiveWindowInfo &info)
{
    bool wasBlocking = shouldBlock();
    m_activeWindow = info;
    if (shouldBlock() != wasBlocking) {
        emit blockingStateChanged(blockingStatus());
    }
}

void SmartBlocker::setScreenBlackDelay(int seconds)
{
    m_screenBlackDelay = seconds;
}

bool SmartBlocker::shouldBlock() const
{
    if (!m_smartEnabled) {
        // 智能模式关闭 → 总是阻止（向后兼容）
        return true;
    }

    if (m_targetProcesses.isEmpty()) {
        // 智能模式开启但未配置进程 → 总是阻止（避免意外不阻止）
        return true;
    }

    if (!m_activeWindow.valid) {
        // 无有效的活动窗口信息 → 不阻止（不知道前端是什么）
        return false;
    }

    if (m_activeWindow.minimized) {
        // 活动窗口已最小化 → 不阻止
        return false;
    }

    // 检查进程名是否匹配目标列表
    return matchesTarget(m_activeWindow.processName);
}

int SmartBlocker::effectiveIntervalMs() const
{
    if (m_autoInterval && m_screenBlackDelay > 0) {
        // 自动模式：熄屏时间 − 1 秒，下限 5 秒
        int ms = (m_screenBlackDelay - 1) * 1000;
        if (ms < 5000) ms = 5000;
        return ms;
    }

    // 手动模式：分钟 → 毫秒（保持原有 "分钟" 语义）
    return m_manualIntervalMinutes * 60 * 1000;
}

QString SmartBlocker::blockingStatus() const
{
    if (!m_smartEnabled) {
        return QStringLiteral("disabled");
    }
    if (shouldBlock()) {
        return QStringLiteral("blocking");
    }
    return QStringLiteral("idle");
}

bool SmartBlocker::matchesTarget(const QString &procName) const
{
    if (procName.isEmpty()) return false;

    for (const QString &target : m_targetProcesses) {
        if (target.isEmpty()) continue;
        // 大小写不敏感、前缀匹配
        if (procName.startsWith(target, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}
