#include "ipc_server.h"

#include <QLocalServer>
#include <QLocalSocket>
#include <QFile>
#include <QDebug>
#include <unistd.h>

/**
 * @brief 计算用户专属 socket 路径
 *
 * 最小权限 + 多用户隔离：优先 $XDG_RUNTIME_DIR（用户专属、权限 0700），
 * 未设置时回退 /tmp/numlockd-<uid>.sock。不再使用全局 /tmp/numlockd，
 * 避免不同用户/会话之间互相冲突或越权连接。
 */
static QString numlockSocketPath()
{
    const QByteArray runtime = qgetenv("XDG_RUNTIME_DIR");
    if (!runtime.isEmpty()) {
        return QString::fromUtf8(runtime) + QStringLiteral("/numlockd.sock");
    }
    return QStringLiteral("/tmp/numlockd-%1.sock").arg(static_cast<qint64>(getuid()));
}

IpcServer::IpcServer(const QString& socketPath, QObject* parent)
    : QObject(parent)
    , m_server(nullptr)
    , m_socketPath(socketPath.isEmpty() ? numlockSocketPath() : socketPath)
{
}

IpcServer::~IpcServer()
{
    stop();
}

bool IpcServer::start()
{
    if (m_server) {
        qWarning() << "IPC server is already running";
        return false;
    }

    // 删除旧的 socket 文件（如果存在）
    if (QFile::exists(m_socketPath)) {
        if (!QFile::remove(m_socketPath)) {
            qWarning() << "Failed to remove old socket file:" << m_socketPath;
        } else {
            qDebug() << "Removed old socket file:" << m_socketPath;
        }
    }

    m_server = new QLocalServer(this);
    connect(m_server, &QLocalServer::newConnection, this, &IpcServer::onNewConnection);

    if (!m_server->listen(m_socketPath)) {
        qCritical() << "Failed to start IPC server:" << m_server->errorString();
        delete m_server;
        m_server = nullptr;
        return false;
    }

    qDebug() << "IPC server started on:" << m_socketPath;
    return true;
}

void IpcServer::stop()
{
    // 关闭所有客户端连接
    for (QLocalSocket* client : m_clients) {
        if (client) {
            disconnect(client, nullptr, this, nullptr);
            client->close();
            client->deleteLater();
        }
    }
    m_clients.clear();

    // 关闭服务器
    if (m_server) {
        m_server->close();
        delete m_server;
        m_server = nullptr;
        qDebug() << "IPC server stopped";
    }

    // 删除 socket 文件
    if (QFile::exists(m_socketPath)) {
        QFile::remove(m_socketPath);
    }
}

void IpcServer::sendLog(const QString& message)
{
    if (m_clients.isEmpty()) {
        return;
    }

    QString logMessage = QStringLiteral("LOG:%1").arg(message);
    QByteArray data = logMessage.toUtf8();
    for (QLocalSocket* client : m_clients) {
        if (client && client->state() == QLocalSocket::ConnectedState) {
            client->write(data);
            client->flush();
        }
    }
}

void IpcServer::sendStatus(const QString &status, int pid,
                            const QString &procName, bool minimized, int intervalMs)
{
    if (m_clients.isEmpty()) {
        return;
    }

    QString msg = QStringLiteral("STATUS:%1,%2,%3,%4,%5")
                      .arg(status)
                      .arg(pid)
                      .arg(procName)
                      .arg(minimized ? 1 : 0)
                      .arg(intervalMs);
    QByteArray data = msg.toUtf8();
    for (QLocalSocket *client : m_clients) {
        if (client && client->state() == QLocalSocket::ConnectedState) {
            client->write(data);
            client->flush();
        }
    }
}

void IpcServer::onNewConnection()
{
    QLocalSocket* client = m_server->nextPendingConnection();
    if (!client) {
        return;
    }

    m_clients.append(client);
    qDebug() << "New client connected. Total clients:" << m_clients.size();

    connect(client, &QLocalSocket::readyRead, this, &IpcServer::onReadyRead);
    connect(client, &QLocalSocket::disconnected, this, &IpcServer::onClientDisconnected);
    connect(client, &QLocalSocket::errorOccurred, this, [this, client](QLocalSocket::LocalSocketError error) {
        qWarning() << "Socket error for client:" << error;
        if (!m_clients.contains(client)) {
            return;
        }
        m_clients.removeOne(client);
        client->deleteLater();
    });
}

void IpcServer::onReadyRead()
{
    QLocalSocket* client = qobject_cast<QLocalSocket*>(sender());
    if (!client || !m_clients.contains(client)) {
        return;
    }

    QByteArray data = client->readAll();
    QString message = QString::fromUtf8(data).trimmed();

    qDebug() << "Received message from client:" << message;

    // 解析消息前缀
    if (message.startsWith(QStringLiteral("CONFIG:"))) {
        QString valueStr = message.mid(7);
        bool ok;
        int intervalMinutes = valueStr.toInt(&ok);
        if (ok && intervalMinutes > 0) {
            qDebug() << "Config received: interval =" << intervalMinutes << "minutes";
            emit configReceived(intervalMinutes);
        } else {
            qWarning() << "Invalid config value:" << valueStr;
        }
    } else if (message.startsWith(QStringLiteral("SMART:"))) {
        bool enabled = (message.mid(6) == QStringLiteral("1"));
        qDebug() << "Smart mode:" << (enabled ? "on" : "off");
        emit smartReceived(enabled);
    } else if (message.startsWith(QStringLiteral("TARGET:"))) {
        QString list = message.mid(7);
        QStringList procs = list.split(QLatin1Char(','), Qt::SkipEmptyParts);
        // trim each entry
        for (QString &p : procs) p = p.trimmed();
        qDebug() << "Target processes:" << procs;
        emit targetReceived(procs);
    } else if (message.startsWith(QStringLiteral("AUTO:"))) {
        bool enabled = (message.mid(5) == QStringLiteral("1"));
        qDebug() << "Auto interval:" << (enabled ? "on" : "off");
        emit autoIntervalReceived(enabled);
    }
}

void IpcServer::onClientDisconnected()
{
    QLocalSocket* client = qobject_cast<QLocalSocket*>(sender());
    if (!client) {
        return;
    }

    if (m_clients.contains(client)) {
        m_clients.removeOne(client);
        qDebug() << "Client disconnected. Total clients:" << m_clients.size();
    }

    client->deleteLater();
}
