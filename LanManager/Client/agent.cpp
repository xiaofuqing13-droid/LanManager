#include "agent.h"
#include "sysinfo.h"
#include "softmgr.h"
#include <QDir>
#include <QStandardPaths>
#include <QJsonArray>
#include <QDebug>

Agent::Agent(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_discoverySocket(new QUdpSocket(this))
    , m_heartbeatTimer(new QTimer(this))
    , m_reconnectTimer(new QTimer(this))
    , m_serverPort(DEFAULT_PORT)
    , m_autoDiscovery(false)
    , m_receiveFile(nullptr)
    , m_expectedFileSize(0)
    , m_receivedSize(0)
{
    connect(m_socket, &QTcpSocket::connected, this, &Agent::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &Agent::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &Agent::onReadyRead);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, &Agent::onError);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &Agent::sendHeartbeat);
    connect(m_discoverySocket, &QUdpSocket::readyRead, this, &Agent::onBroadcastReceived);
    connect(m_reconnectTimer, &QTimer::timeout, this, &Agent::tryReconnect);
}

Agent::~Agent()
{
    disconnect();
    if (m_receiveFile) {
        m_receiveFile->close();
        delete m_receiveFile;
    }
}

void Agent::connectToServer(const QString& host, quint16 port)
{
    m_serverHost = host;
    m_serverPort = port;
    emit logMessage(QString("正在连接服务器 %1:%2...").arg(host).arg(port));
    m_socket->connectToHost(host, port);
}

void Agent::startAutoDiscovery()
{
    m_autoDiscovery = true;
    
    // 绑定UDP端口监听广播
    if (!m_discoverySocket->bind(BROADCAST_PORT, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        emit logMessage("无法绑定广播端口: " + m_discoverySocket->errorString());
        return;
    }
    
    emit logMessage("自动发现模式已启动,等待服务器...");
}

void Agent::onBroadcastReceived()
{
    while (m_discoverySocket->hasPendingDatagrams()) {
        QByteArray data;
        QHostAddress sender;
        quint16 senderPort;
        
        data.resize(m_discoverySocket->pendingDatagramSize());
        m_discoverySocket->readDatagram(data.data(), data.size(), &sender, &senderPort);
        
        // 解析广播数据: LANMGR_SERVER:TCP端口
        QString msg = QString::fromUtf8(data);
        if (msg.startsWith(BROADCAST_MAGIC)) {
            QStringList parts = msg.split(':');
            if (parts.size() >= 2) {
                QString serverIp = sender.toString();
                // 移除IPv6前缀
                if (serverIp.startsWith("::ffff:")) {
                    serverIp = serverIp.mid(7);
                }
                quint16 tcpPort = parts[1].toUInt();
                
                emit logMessage(QString("发现服务器: %1:%2").arg(serverIp).arg(tcpPort));
                emit serverDiscovered(serverIp, tcpPort);
                
                // 如果未连接,自动连接
                if (!isConnected()) {
                    connectToServer(serverIp, tcpPort);
                }
            }
        }
    }
}

void Agent::tryReconnect()
{
    if (!isConnected() && !m_serverHost.isEmpty()) {
        emit logMessage("尝试重新连接...");
        connectToServer(m_serverHost, m_serverPort);
    }
}

void Agent::disconnect()
{
    m_heartbeatTimer->stop();
    m_reconnectTimer->stop();
    m_autoDiscovery = false;
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->disconnectFromHost();
    }
}

bool Agent::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void Agent::onConnected()
{
    emit logMessage("已连接到服务器");
    emit connected();
    
    // 发送客户端基本信息
    sendClientInfo();
    
    // 启动心跳
    m_heartbeatTimer->start(HEARTBEAT_INTERVAL);
}

void Agent::onDisconnected()
{
    emit logMessage("与服务器断开连接");
    m_heartbeatTimer->stop();
    emit disconnected();
    
    // 自动重连
    if (m_autoDiscovery || !m_serverHost.isEmpty()) {
        emit logMessage("5秒后自动重连...");
        m_reconnectTimer->start(5000);
    }
}

void Agent::onReadyRead()
{
    m_buffer.append(m_socket->readAll());
    
    // 循环处理完整的数据包
    while (m_buffer.size() >= Protocol::headerSize()) {
        ProtocolHeader header;
        if (!Protocol::parseHeader(m_buffer, header)) {
            break;
        }
        
        int packetSize = Protocol::headerSize() + header.dataLength;
        if (m_buffer.size() < packetSize) {
            break; // 数据包不完整,等待更多数据
        }
        
        // 提取数据
        QByteArray data = m_buffer.mid(Protocol::headerSize(), header.dataLength);
        m_buffer.remove(0, packetSize);
        
        // 处理命令
        processCommand(static_cast<CommandType>(header.cmdType), data);
    }
}

void Agent::onError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
    emit errorOccurred(m_socket->errorString());
    emit logMessage("连接错误: " + m_socket->errorString());
}

void Agent::sendHeartbeat()
{
    sendPacket(CMD_HEARTBEAT, QByteArray());
}

void Agent::sendPacket(CommandType cmd, const QByteArray& data)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }
    QByteArray packet = Protocol::pack(cmd, data);
    m_socket->write(packet);
}

void Agent::sendJson(CommandType cmd, const QJsonObject& json)
{
    QJsonDocument doc(json);
    sendPacket(cmd, doc.toJson(QJsonDocument::Compact));
}

void Agent::processCommand(CommandType cmd, const QByteArray& data)
{
    switch (cmd) {
    case CMD_HEARTBEAT_ACK:
        // 心跳响应,不需要处理
        break;
        
    case CMD_GET_SYSINFO:
        emit logMessage("收到系统信息请求");
        handleGetSysInfo();
        break;
        
    case CMD_GET_SOFTWARE:
        emit logMessage("收到软件列表请求");
        handleGetSoftware();
        break;
        
    case CMD_INSTALL_SOFTWARE:
        emit logMessage("收到安装软件请求");
        handleInstallSoftware(Protocol::parseJson(data));
        break;
        
    case CMD_UNINSTALL_SOFTWARE:
        emit logMessage("收到卸载软件请求");
        handleUninstallSoftware(Protocol::parseJson(data));
        break;
        
    case CMD_FILE_TRANSFER_START:
        emit logMessage("收到文件传输开始");
        handleFileTransferStart(Protocol::parseJson(data));
        break;
        
    case CMD_FILE_TRANSFER_DATA:
        handleFileTransferData(data);
        break;
        
    case CMD_FILE_TRANSFER_END:
        emit logMessage("文件传输完成");
        handleFileTransferEnd();
        break;
        
    default:
        emit logMessage(QString("收到未知命令: 0x%1").arg(cmd, 4, 16, QChar('0')));
        break;
    }
}

void Agent::sendClientInfo()
{
    SystemInfo sysInfo = SysInfo::getSystemInfo();
    QJsonObject json;
    json["computerName"] = sysInfo.computerName;
    json["ipAddress"] = sysInfo.ipAddress;
    json["macAddress"] = sysInfo.macAddress;
    json["osVersion"] = sysInfo.osVersion;
    sendJson(CMD_CLIENT_INFO, json);
}

void Agent::handleGetSysInfo()
{
    SystemInfo sysInfo = SysInfo::getSystemInfo();
    sendJson(CMD_SYSINFO_RESPONSE, sysInfo.toJson());
    emit logMessage("已发送系统信息");
}

void Agent::handleGetSoftware()
{
    QList<SoftwareInfo> softList = SoftwareManager::getInstalledSoftware();
    
    QJsonArray arr;
    for (const SoftwareInfo& info : softList) {
        arr.append(info.toJson());
    }
    
    QJsonObject json;
    json["software"] = arr;
    json["count"] = softList.size();
    
    sendJson(CMD_SOFTWARE_RESPONSE, json);
    emit logMessage(QString("已发送软件列表 (%1 个)").arg(softList.size()));
}

void Agent::handleInstallSoftware(const QJsonObject& json)
{
    QString filePath = json["filePath"].toString();
    QString args = json["args"].toString();
    
    emit logMessage(QString("正在安装: %1").arg(filePath));
    
    bool success = SoftwareManager::installSoftware(filePath, args);
    
    QJsonObject response;
    response["success"] = success;
    response["filePath"] = filePath;
    response["message"] = success ? "安装成功" : "安装失败";
    
    sendJson(CMD_INSTALL_RESPONSE, response);
    emit logMessage(success ? "安装完成" : "安装失败");
}

void Agent::handleUninstallSoftware(const QJsonObject& json)
{
    QString softwareName = json["name"].toString();
    QString uninstallCmd = json["uninstallCmd"].toString();
    
    emit logMessage(QString("正在卸载: %1").arg(softwareName));
    
    bool success = SoftwareManager::uninstallSoftware(uninstallCmd);
    
    QJsonObject response;
    response["success"] = success;
    response["name"] = softwareName;
    response["message"] = success ? "卸载成功" : "卸载失败";
    
    sendJson(CMD_UNINSTALL_RESPONSE, response);
    emit logMessage(success ? "卸载完成" : "卸载失败");
}

void Agent::handleFileTransferStart(const QJsonObject& json)
{
    QString fileName = json["fileName"].toString();
    m_expectedFileSize = json["fileSize"].toVariant().toLongLong();
    m_pendingInstallArgs = json["installArgs"].toString();
    
    // 创建临时目录
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir().mkpath(tempDir);
    
    m_receiveFilePath = tempDir + "/" + fileName;
    
    // 关闭之前的文件(如果有)
    if (m_receiveFile) {
        m_receiveFile->close();
        delete m_receiveFile;
    }
    
    m_receiveFile = new QFile(m_receiveFilePath);
    if (!m_receiveFile->open(QIODevice::WriteOnly)) {
        emit logMessage("无法创建文件: " + m_receiveFilePath);
        
        QJsonObject response;
        response["success"] = false;
        response["message"] = "无法创建文件";
        sendJson(CMD_FILE_TRANSFER_ACK, response);
        return;
    }
    
    m_receivedSize = 0;
    
    QJsonObject response;
    response["success"] = true;
    response["message"] = "准备接收文件";
    sendJson(CMD_FILE_TRANSFER_ACK, response);
    
    emit logMessage(QString("开始接收文件: %1 (%2 字节)").arg(fileName).arg(m_expectedFileSize));
}

void Agent::handleFileTransferData(const QByteArray& data)
{
    if (!m_receiveFile || !m_receiveFile->isOpen()) {
        return;
    }
    
    m_receiveFile->write(data);
    m_receivedSize += data.size();
    
    // 计算进度
    int progress = (m_expectedFileSize > 0) ? 
        (int)(m_receivedSize * 100 / m_expectedFileSize) : 0;
    
    if (m_receivedSize % (1024 * 1024) < data.size()) { // 每MB报告一次
        emit logMessage(QString("接收进度: %1%").arg(progress));
    }
}

void Agent::handleFileTransferEnd()
{
    if (!m_receiveFile) {
        return;
    }
    
    m_receiveFile->close();
    
    bool success = (m_receivedSize == m_expectedFileSize);
    
    QJsonObject response;
    response["success"] = success;
    response["filePath"] = m_receiveFilePath;
    response["receivedSize"] = m_receivedSize;
    
    if (success) {
        response["message"] = "文件接收完成";
        emit logMessage("文件接收完成: " + m_receiveFilePath);
        
        // 自动安装
        emit logMessage("开始安装...");
        bool installSuccess = SoftwareManager::installSoftware(m_receiveFilePath, m_pendingInstallArgs);
        
        QJsonObject installResponse;
        installResponse["success"] = installSuccess;
        installResponse["filePath"] = m_receiveFilePath;
        installResponse["message"] = installSuccess ? "安装成功" : "安装失败";
        sendJson(CMD_INSTALL_RESPONSE, installResponse);
        
        // 删除临时文件
        QFile::remove(m_receiveFilePath);
    } else {
        response["message"] = QString("文件不完整: 期望 %1 字节, 收到 %2 字节")
            .arg(m_expectedFileSize).arg(m_receivedSize);
        emit logMessage(response["message"].toString());
    }
    
    sendJson(CMD_FILE_TRANSFER_ACK, response);
    
    delete m_receiveFile;
    m_receiveFile = nullptr;
    m_receiveFilePath.clear();
    m_pendingInstallArgs.clear();
}
