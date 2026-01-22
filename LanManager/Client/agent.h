#ifndef AGENT_H
#define AGENT_H

#include <QObject>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QTimer>
#include <QFile>
#include "../Common/protocol.h"

class Agent : public QObject
{
    Q_OBJECT
public:
    explicit Agent(QObject *parent = nullptr);
    ~Agent();
    
    // 连接到服务器
    void connectToServer(const QString& host, quint16 port = DEFAULT_PORT);
    
    // 启动自动发现模式
    void startAutoDiscovery();
    
    // 断开连接
    void disconnect();
    
    // 是否已连接
    bool isConnected() const;
    
signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString& error);
    void logMessage(const QString& message);
    void serverDiscovered(const QString& host, quint16 port);
    
private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onError(QAbstractSocket::SocketError error);
    void sendHeartbeat();
    void onBroadcastReceived();
    void tryReconnect();
    
private:
    // 发送数据
    void sendPacket(CommandType cmd, const QByteArray& data);
    void sendJson(CommandType cmd, const QJsonObject& json);
    
    // 处理接收到的命令
    void processCommand(CommandType cmd, const QByteArray& data);
    
    // 命令处理函数
    void handleGetSysInfo();
    void handleGetSoftware();
    void handleInstallSoftware(const QJsonObject& json);
    void handleUninstallSoftware(const QJsonObject& json);
    void handleFileTransferStart(const QJsonObject& json);
    void handleFileTransferData(const QByteArray& data);
    void handleFileTransferEnd();
    
    // 发送客户端基本信息
    void sendClientInfo();
    
private:
    QTcpSocket* m_socket;
    QUdpSocket* m_discoverySocket;
    QTimer* m_heartbeatTimer;
    QTimer* m_reconnectTimer;
    QByteArray m_buffer;  // 接收缓冲区
    QString m_serverHost;
    quint16 m_serverPort;
    bool m_autoDiscovery;
    
    // 文件传输相关
    QFile* m_receiveFile;
    QString m_receiveFilePath;
    QString m_pendingInstallArgs;
    qint64 m_expectedFileSize;
    qint64 m_receivedSize;
};

#endif // AGENT_H
