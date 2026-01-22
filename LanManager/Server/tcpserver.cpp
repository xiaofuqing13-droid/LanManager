#include "tcpserver.h"
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QJsonArray>
#include <QDebug>

#define FILE_CHUNK_SIZE (64 * 1024)  // 64KB每块

TcpServer::TcpServer(QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
    , m_broadcastSocket(new QUdpSocket(this))
    , m_broadcastTimer(new QTimer(this))
    , m_heartbeatChecker(new QTimer(this))
    , m_tcpPort(DEFAULT_PORT)
{
    connect(m_server, &QTcpServer::newConnection, this, &TcpServer::onNewConnection);
    connect(m_heartbeatChecker, &QTimer::timeout, this, &TcpServer::checkHeartbeats);
    connect(m_broadcastTimer, &QTimer::timeout, this, &TcpServer::sendBroadcast);
}

TcpServer::~TcpServer()
{
    stop();
}

bool TcpServer::start(quint16 port)
{
    if (m_server->isListening()) {
        return true;
    }
    
    if (!m_server->listen(QHostAddress::Any, port)) {
        emit logMessage("服务器启动失败: " + m_server->errorString());
        return false;
    }
    
    m_tcpPort = port;
    emit logMessage(QString("服务器已启动,监听端口: %1").arg(port));
    m_heartbeatChecker->start(HEARTBEAT_INTERVAL);
    
    // 启动UDP广播，让客户端自动发现
    m_broadcastTimer->start(BROADCAST_INTERVAL);
    sendBroadcast();
    emit logMessage("UDP广播已启动,客户端将自动上线");
    
    return true;
}

void TcpServer::stop()
{
    m_heartbeatChecker->stop();
    m_broadcastTimer->stop();
    
    // 断开所有客户端
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (it.value()->socket) {
            it.value()->socket->disconnectFromHost();
        }
        delete it.value();
    }
    m_clients.clear();
    
    if (m_server->isListening()) {
        m_server->close();
        emit logMessage("服务器已停止");
    }
}

bool TcpServer::isRunning() const
{
    return m_server->isListening();
}

QList<qintptr> TcpServer::getClientIds() const
{
    return m_clients.keys();
}

ClientConnection* TcpServer::getClient(qintptr clientId)
{
    return m_clients.value(clientId, nullptr);
}

void TcpServer::sendToClient(qintptr clientId, CommandType cmd, const QByteArray& data)
{
    ClientConnection* client = m_clients.value(clientId, nullptr);
    if (client && client->socket && client->socket->state() == QAbstractSocket::ConnectedState) {
        QByteArray packet = Protocol::pack(cmd, data);
        client->socket->write(packet);
    }
}

void TcpServer::sendJsonToClient(qintptr clientId, CommandType cmd, const QJsonObject& json)
{
    QJsonDocument doc(json);
    sendToClient(clientId, cmd, doc.toJson(QJsonDocument::Compact));
}

void TcpServer::sendToAll(CommandType cmd, const QByteArray& data)
{
    for (qintptr clientId : m_clients.keys()) {
        sendToClient(clientId, cmd, data);
    }
}

void TcpServer::requestSysInfo(qintptr clientId)
{
    sendToClient(clientId, CMD_GET_SYSINFO, QByteArray());
    emit logMessage(QString("向客户端 %1 请求系统信息").arg(clientId));
}

void TcpServer::requestSoftwareList(qintptr clientId)
{
    sendToClient(clientId, CMD_GET_SOFTWARE, QByteArray());
    emit logMessage(QString("向客户端 %1 请求软件列表").arg(clientId));
}

void TcpServer::installSoftware(qintptr clientId, const QString& filePath, const QString& args)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit installResult(clientId, false, "无法打开文件: " + filePath);
        return;
    }
    
    QFileInfo fileInfo(filePath);
    
    // 存储文件传输信息
    FileTransferInfo& transfer = m_pendingTransfers[clientId];
    transfer.filePath = filePath;
    transfer.installArgs = args;
    transfer.fileData = file.readAll();
    transfer.sentSize = 0;
    file.close();
    
    // 发送文件传输开始命令
    QJsonObject json;
    json["fileName"] = fileInfo.fileName();
    json["fileSize"] = transfer.fileData.size();
    json["installArgs"] = args;
    
    sendJsonToClient(clientId, CMD_FILE_TRANSFER_START, json);
    emit logMessage(QString("开始向客户端 %1 传输文件: %2 (%3 字节)")
        .arg(clientId).arg(fileInfo.fileName()).arg(transfer.fileData.size()));
}

void TcpServer::uninstallSoftware(qintptr clientId, const QString& softwareName, const QString& uninstallCmd)
{
    QJsonObject json;
    json["name"] = softwareName;
    json["uninstallCmd"] = uninstallCmd;
    
    sendJsonToClient(clientId, CMD_UNINSTALL_SOFTWARE, json);
    emit logMessage(QString("向客户端 %1 发送卸载命令: %2").arg(clientId).arg(softwareName));
}

void TcpServer::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket* socket = m_server->nextPendingConnection();
        qintptr clientId = socket->socketDescriptor();
        
        ClientConnection* client = new ClientConnection();
        client->socket = socket;
        client->ipAddress = socket->peerAddress().toString();
        client->lastHeartbeat = QDateTime::currentDateTime();
        client->online = true;
        client->isTransferring = false;
        client->fileSize = 0;
        client->sentSize = 0;
        
        m_clients[clientId] = client;
        
        connect(socket, &QTcpSocket::disconnected, this, &TcpServer::onClientDisconnected);
        connect(socket, &QTcpSocket::readyRead, this, &TcpServer::onClientReadyRead);
        connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
                this, &TcpServer::onClientError);
        
        emit logMessage(QString("新客户端连接: %1 (%2)").arg(clientId).arg(client->ipAddress));
        emit clientConnected(clientId);
    }
}

void TcpServer::onClientDisconnected()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    
    qintptr clientId = socket->socketDescriptor();
    
    // 查找客户端ID
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (it.value()->socket == socket) {
            clientId = it.key();
            break;
        }
    }
    
    ClientConnection* client = m_clients.value(clientId, nullptr);
    if (client) {
        emit logMessage(QString("客户端断开连接: %1 (%2)").arg(clientId).arg(client->computerName));
        client->online = false;
        m_clients.remove(clientId);
        delete client;
        m_pendingTransfers.remove(clientId);
        emit clientDisconnected(clientId);
    }
}

void TcpServer::onClientReadyRead()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    
    qintptr clientId = -1;
    ClientConnection* client = nullptr;
    
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (it.value()->socket == socket) {
            clientId = it.key();
            client = it.value();
            break;
        }
    }
    
    if (client) {
        client->buffer.append(socket->readAll());
        processClientData(clientId, client);
    }
}

void TcpServer::onClientError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket) {
        emit logMessage("客户端连接错误: " + socket->errorString());
    }
}

void TcpServer::checkHeartbeats()
{
    QDateTime now = QDateTime::currentDateTime();
    QList<qintptr> timeoutClients;
    
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (it.value()->lastHeartbeat.msecsTo(now) > HEARTBEAT_TIMEOUT) {
            timeoutClients.append(it.key());
        }
    }
    
    for (qintptr clientId : timeoutClients) {
        ClientConnection* client = m_clients.value(clientId);
        if (client && client->socket) {
            emit logMessage(QString("客户端 %1 心跳超时,断开连接").arg(clientId));
            client->socket->disconnectFromHost();
        }
    }
}

void TcpServer::processClientData(qintptr clientId, ClientConnection* client)
{
    while (client->buffer.size() >= Protocol::headerSize()) {
        ProtocolHeader header;
        if (!Protocol::parseHeader(client->buffer, header)) {
            break;
        }
        
        int packetSize = Protocol::headerSize() + header.dataLength;
        if (client->buffer.size() < packetSize) {
            break;
        }
        
        QByteArray data = client->buffer.mid(Protocol::headerSize(), header.dataLength);
        client->buffer.remove(0, packetSize);
        
        processCommand(clientId, static_cast<CommandType>(header.cmdType), data);
    }
}

void TcpServer::processCommand(qintptr clientId, CommandType cmd, const QByteArray& data)
{
    switch (cmd) {
    case CMD_CLIENT_INFO:
        handleClientInfo(clientId, Protocol::parseJson(data));
        break;
        
    case CMD_HEARTBEAT:
        handleHeartbeat(clientId);
        break;
        
    case CMD_SYSINFO_RESPONSE:
        handleSysInfoResponse(clientId, Protocol::parseJson(data));
        break;
        
    case CMD_SOFTWARE_RESPONSE:
        handleSoftwareResponse(clientId, Protocol::parseJson(data));
        break;
        
    case CMD_INSTALL_RESPONSE:
        handleInstallResponse(clientId, Protocol::parseJson(data));
        break;
        
    case CMD_UNINSTALL_RESPONSE:
        handleUninstallResponse(clientId, Protocol::parseJson(data));
        break;
        
    case CMD_FILE_TRANSFER_ACK:
        handleFileTransferAck(clientId, Protocol::parseJson(data));
        break;
        
    default:
        break;
    }
}

void TcpServer::handleClientInfo(qintptr clientId, const QJsonObject& json)
{
    ClientConnection* client = m_clients.value(clientId, nullptr);
    if (!client) return;
    
    client->computerName = json["computerName"].toString();
    client->ipAddress = json["ipAddress"].toString();
    client->macAddress = json["macAddress"].toString();
    client->osVersion = json["osVersion"].toString();
    
    emit logMessage(QString("客户端 %1 信息: %2 (%3)")
        .arg(clientId).arg(client->computerName).arg(client->ipAddress));
    emit clientInfoUpdated(clientId);
}

void TcpServer::handleHeartbeat(qintptr clientId)
{
    ClientConnection* client = m_clients.value(clientId, nullptr);
    if (client) {
        client->lastHeartbeat = QDateTime::currentDateTime();
        sendToClient(clientId, CMD_HEARTBEAT_ACK, QByteArray());
    }
}

void TcpServer::handleSysInfoResponse(qintptr clientId, const QJsonObject& json)
{
    SystemInfo info = SystemInfo::fromJson(json);
    emit logMessage(QString("收到客户端 %1 系统信息").arg(clientId));
    emit sysInfoReceived(clientId, info);
}

void TcpServer::handleSoftwareResponse(qintptr clientId, const QJsonObject& json)
{
    QList<SoftwareInfo> list;
    QJsonArray arr = json["software"].toArray();
    
    for (const QJsonValue& val : arr) {
        list.append(SoftwareInfo::fromJson(val.toObject()));
    }
    
    emit logMessage(QString("收到客户端 %1 软件列表 (%2 个)").arg(clientId).arg(list.size()));
    emit softwareListReceived(clientId, list);
}

void TcpServer::handleInstallResponse(qintptr clientId, const QJsonObject& json)
{
    bool success = json["success"].toBool();
    QString message = json["message"].toString();
    QString filePath = json["filePath"].toString();
    
    emit logMessage(QString("客户端 %1 安装结果: %2 - %3")
        .arg(clientId).arg(success ? "成功" : "失败").arg(message));
    emit installResult(clientId, success, message);
    
    // 清理传输信息
    m_pendingTransfers.remove(clientId);
}

void TcpServer::handleUninstallResponse(qintptr clientId, const QJsonObject& json)
{
    bool success = json["success"].toBool();
    QString message = json["message"].toString();
    QString name = json["name"].toString();
    
    emit logMessage(QString("客户端 %1 卸载 %2: %3 - %4")
        .arg(clientId).arg(name).arg(success ? "成功" : "失败").arg(message));
    emit uninstallResult(clientId, success, message);
}

void TcpServer::handleFileTransferAck(qintptr clientId, const QJsonObject& json)
{
    bool success = json["success"].toBool();
    
    if (!success) {
        QString message = json["message"].toString();
        emit logMessage(QString("文件传输失败: %1").arg(message));
        emit installResult(clientId, false, message);
        m_pendingTransfers.remove(clientId);
        return;
    }
    
    // 继续传输文件
    continueFileTransfer(clientId);
}

void TcpServer::sendBroadcast()
{
    // 广播格式: LANMGR_SERVER:TCP端口
    QByteArray data = QString("%1:%2").arg(BROADCAST_MAGIC).arg(m_tcpPort).toUtf8();
    m_broadcastSocket->writeDatagram(data, QHostAddress::Broadcast, BROADCAST_PORT);
}

void TcpServer::continueFileTransfer(qintptr clientId)
{
    if (!m_pendingTransfers.contains(clientId)) {
        return;
    }
    
    FileTransferInfo& transfer = m_pendingTransfers[clientId];
    
    if (transfer.sentSize >= transfer.fileData.size()) {
        // 传输完成
        sendToClient(clientId, CMD_FILE_TRANSFER_END, QByteArray());
        emit logMessage(QString("文件传输完成,等待客户端安装"));
        return;
    }
    
    // 发送下一块数据
    qint64 remaining = transfer.fileData.size() - transfer.sentSize;
    qint64 chunkSize = qMin((qint64)FILE_CHUNK_SIZE, remaining);
    
    QByteArray chunk = transfer.fileData.mid(transfer.sentSize, chunkSize);
    sendToClient(clientId, CMD_FILE_TRANSFER_DATA, chunk);
    
    transfer.sentSize += chunkSize;
    
    // 计算进度
    int percent = (int)(transfer.sentSize * 100 / transfer.fileData.size());
    emit fileTransferProgress(clientId, percent);
    
    // 继续发送
    QMetaObject::invokeMethod(this, [this, clientId]() {
        continueFileTransfer(clientId);
    }, Qt::QueuedConnection);
}
