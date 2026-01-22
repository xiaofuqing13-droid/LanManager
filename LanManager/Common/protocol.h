#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <QString>
#include <QByteArray>
#include <QDataStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

// 默认端口
#define DEFAULT_PORT 8899

// UDP广播端口
#define BROADCAST_PORT 8898

// UDP广播间隔(毫秒)
#define BROADCAST_INTERVAL 3000

// UDP广播标识
#define BROADCAST_MAGIC "LANMGR_SERVER"

// 心跳间隔(毫秒)
#define HEARTBEAT_INTERVAL 5000

// 心跳超时(毫秒)
#define HEARTBEAT_TIMEOUT 15000

// 命令类型枚举
enum CommandType {
    CMD_HEARTBEAT = 0x0001,          // 心跳包
    CMD_HEARTBEAT_ACK = 0x0002,      // 心跳响应
    CMD_GET_SYSINFO = 0x0010,        // 获取系统信息
    CMD_SYSINFO_RESPONSE = 0x0011,   // 系统信息响应
    CMD_GET_SOFTWARE = 0x0020,       // 获取已安装软件列表
    CMD_SOFTWARE_RESPONSE = 0x0021,  // 软件列表响应
    CMD_INSTALL_SOFTWARE = 0x0030,   // 安装软件
    CMD_INSTALL_RESPONSE = 0x0031,   // 安装结果响应
    CMD_UNINSTALL_SOFTWARE = 0x0040, // 卸载软件
    CMD_UNINSTALL_RESPONSE = 0x0041, // 卸载结果响应
    CMD_FILE_TRANSFER_START = 0x0050,// 文件传输开始
    CMD_FILE_TRANSFER_DATA = 0x0051, // 文件传输数据
    CMD_FILE_TRANSFER_END = 0x0052,  // 文件传输结束
    CMD_FILE_TRANSFER_ACK = 0x0053,  // 文件传输确认
    CMD_CLIENT_INFO = 0x0060,        // 客户端基本信息(连接时发送)
    CMD_ERROR = 0x00FF               // 错误响应
};

// 协议头结构
// [4字节数据长度][4字节命令类型][数据]
struct ProtocolHeader {
    quint32 dataLength;  // 数据长度(不包含头部)
    quint32 cmdType;     // 命令类型
};

// 系统信息结构
struct SystemInfo {
    QString computerName;    // 计算机名
    QString osVersion;       // 操作系统版本
    QString cpuInfo;         // CPU信息
    quint64 totalMemory;     // 总内存(MB)
    quint64 freeMemory;      // 可用内存(MB)
    QString diskInfo;        // 磁盘信息
    QString macAddress;      // MAC地址
    QString ipAddress;       // IP地址
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["computerName"] = computerName;
        obj["osVersion"] = osVersion;
        obj["cpuInfo"] = cpuInfo;
        obj["totalMemory"] = (qint64)totalMemory;
        obj["freeMemory"] = (qint64)freeMemory;
        obj["diskInfo"] = diskInfo;
        obj["macAddress"] = macAddress;
        obj["ipAddress"] = ipAddress;
        return obj;
    }
    
    static SystemInfo fromJson(const QJsonObject& obj) {
        SystemInfo info;
        info.computerName = obj["computerName"].toString();
        info.osVersion = obj["osVersion"].toString();
        info.cpuInfo = obj["cpuInfo"].toString();
        info.totalMemory = obj["totalMemory"].toVariant().toULongLong();
        info.freeMemory = obj["freeMemory"].toVariant().toULongLong();
        info.diskInfo = obj["diskInfo"].toString();
        info.macAddress = obj["macAddress"].toString();
        info.ipAddress = obj["ipAddress"].toString();
        return info;
    }
};

// 软件信息结构
struct SoftwareInfo {
    QString name;           // 软件名称
    QString version;        // 版本号
    QString publisher;      // 发布者
    QString installDate;    // 安装日期
    QString installPath;    // 安装路径
    QString uninstallCmd;   // 卸载命令
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["name"] = name;
        obj["version"] = version;
        obj["publisher"] = publisher;
        obj["installDate"] = installDate;
        obj["installPath"] = installPath;
        obj["uninstallCmd"] = uninstallCmd;
        return obj;
    }
    
    static SoftwareInfo fromJson(const QJsonObject& obj) {
        SoftwareInfo info;
        info.name = obj["name"].toString();
        info.version = obj["version"].toString();
        info.publisher = obj["publisher"].toString();
        info.installDate = obj["installDate"].toString();
        info.installPath = obj["installPath"].toString();
        info.uninstallCmd = obj["uninstallCmd"].toString();
        return info;
    }
};

// 协议工具类
class Protocol {
public:
    // 打包数据
    static QByteArray pack(CommandType cmd, const QByteArray& data) {
        QByteArray packet;
        QDataStream stream(&packet, QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::BigEndian);
        stream << (quint32)data.size();
        stream << (quint32)cmd;
        packet.append(data);
        return packet;
    }
    
    // 打包JSON数据
    static QByteArray packJson(CommandType cmd, const QJsonObject& json) {
        QJsonDocument doc(json);
        return pack(cmd, doc.toJson(QJsonDocument::Compact));
    }
    
    // 解析协议头
    static bool parseHeader(const QByteArray& data, ProtocolHeader& header) {
        if (data.size() < 8) return false;
        QDataStream stream(data);
        stream.setByteOrder(QDataStream::BigEndian);
        stream >> header.dataLength;
        stream >> header.cmdType;
        return true;
    }
    
    // 解析JSON数据
    static QJsonObject parseJson(const QByteArray& data) {
        QJsonDocument doc = QJsonDocument::fromJson(data);
        return doc.object();
    }
    
    // 协议头大小
    static int headerSize() {
        return 8; // 4字节长度 + 4字节命令
    }
};

#endif // PROTOCOL_H
