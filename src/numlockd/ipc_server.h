#ifndef IPC_SERVER_H
#define IPC_SERVER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>

QT_BEGIN_NAMESPACE
class QLocalServer;
class QLocalSocket;
QT_END_NAMESPACE

/**
 * @brief numlockd IPC 服务器
 *
 * 通过 Unix domain socket 与 candle GUI 通信。
 *
 * GUI → daemon 消息：
 *   CONFIG:N             手动间隔（分钟），向后兼容
 *   SMART:1|0            智能模式开关
 *   TARGET:p1,p2,...     目标进程列表（逗号分隔，空=清空）
 *   AUTO:1|0             自动同步熄屏时间
 *
 * daemon → GUI 消息：
 *   LOG:message          日志（向后兼容）
 *   STATUS:status,pid,procName,minimized,intervalMs  状态推送
 */
class IpcServer : public QObject
{
    Q_OBJECT

public:
    explicit IpcServer(const QString &socketPath = QString(),
                       QObject *parent = nullptr);
    ~IpcServer() override;

    bool start();
    void stop();

    /** 向所有客户端发送日志 */
    void sendLog(const QString &message);

    /**
     * @brief 推送阻止状态到所有客户端
     * @param status     "blocking" | "idle" | "disabled"
     * @param pid        活动窗口 PID（-1 = 无）
     * @param procName   进程名
     * @param minimized  是否最小化
     * @param intervalMs 当前有效间隔（毫秒）
     */
    void sendStatus(const QString &status, int pid,
                    const QString &procName, bool minimized, int intervalMs);

signals:
    void configReceived(int intervalMinutes);
    void smartReceived(bool enabled);
    void targetReceived(const QStringList &processes);
    void autoIntervalReceived(bool enabled);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onClientDisconnected();

private:
    QLocalServer *m_server = nullptr;
    QList<QLocalSocket *> m_clients;
    QString m_socketPath;
};

#endif // IPC_SERVER_H
