#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QMap>
#include <QTimer>
#include <QDateTime>
#include "../Common/protocol.h"

// 客户端连接信息
struct ClientConnection {
    QTcpSocket* socket;
    QString computerName;
    QString ipAddress;
    QString macAddress;
    QString osVersion;
    QDateTime lastHeartbeat;
    QByteArray buffer;
    bool online;
    
    // 文件传输相关
    bool isTransferring;
    qint64 fileSize;
    qint64 sentSize;
};

class TcpServer : public QObject
{
    Q_OBJECT
public:
    explicit TcpServer(QObject *parent = nullptr);
    ~TcpServer();
    
    // 启动服务器
    bool start(quint16 port = DEFAULT_PORT);
    
    // 停止服务器
    void stop();
    
    // 是否正在运行
    bool isRunning() const;
    
    // 获取所有客户端ID
    QList<qintptr> getClientIds() const;
    
    // 获取客户端信息
    ClientConnection* getClient(qintptr clientId);
    
    // 发送命令到指定客户端
    void sendToClient(qintptr clientId, CommandType cmd, const QByteArray& data);
    void sendJsonToClient(qintptr clientId, CommandType cmd, const QJsonObject& json);
    
    // 发送命令到所有客户端
    void sendToAll(CommandType cmd, const QByteArray& data);
    
    // 请求系统信息
    void requestSysInfo(qintptr clientId);
    
    // 请求软件列表
    void requestSoftwareList(qintptr clientId);
    
    // 安装软件(传输文件并安装)
    void installSoftware(qintptr clientId, const QString& filePath, const QString& args = "");
    
    // 卸载软件
    void uninstallSoftware(qintptr clientId, const QString& softwareName, const QString& uninstallCmd);
    
signals:
    void clientConnected(qintptr clientId);
    void clientDisconnected(qintptr clientId);
    void clientInfoUpdated(qintptr clientId);
    void sysInfoReceived(qintptr clientId, const SystemInfo& info);
    void softwareListReceived(qintptr clientId, const QList<SoftwareInfo>& list);
    void installResult(qintptr clientId, bool success, const QString& message);
    void uninstallResult(qintptr clientId, bool success, const QString& message);
    void fileTransferProgress(qintptr clientId, int percent);
    void logMessage(const QString& message);
    
private slots:
    void onNewConnection();
    void onClientDisconnected();
    void onClientReadyRead();
    void onClientError(QAbstractSocket::SocketError error);
    void checkHeartbeats();
    void sendBroadcast();
    
private:
    void processClientData(qintptr clientId, ClientConnection* client);
    void processCommand(qintptr clientId, CommandType cmd, const QByteArray& data);
    
    // 命令处理
    void handleClientInfo(qintptr clientId, const QJsonObject& json);
    void handleHeartbeat(qintptr clientId);
    void handleSysInfoResponse(qintptr clientId, const QJsonObject& json);
    void handleSoftwareResponse(qintptr clientId, const QJsonObject& json);
    void handleInstallResponse(qintptr clientId, const QJsonObject& json);
    void handleUninstallResponse(qintptr clientId, const QJsonObject& json);
    void handleFileTransferAck(qintptr clientId, const QJsonObject& json);
    
    // 继续文件传输
    void continueFileTransfer(qintptr clientId);
    
private:
    QTcpServer* m_server;
    QUdpSocket* m_broadcastSocket;
    QTimer* m_broadcastTimer;
    QMap<qintptr, ClientConnection*> m_clients;
    QTimer* m_heartbeatChecker;
    quint16 m_tcpPort;
    
    // 文件传输缓存
    struct FileTransferInfo {
        QString filePath;
        QString installArgs;
        QByteArray fileData;
        qint64 sentSize;
    };
    QMap<qintptr, FileTransferInfo> m_pendingTransfers;
};

#endif // TCPSERVER_H
