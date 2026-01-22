#include "sysinfo.h"
#include <QSettings>
#include <QStorageInfo>
#include <QNetworkInterface>
#include <QSysInfo>
#include <QHostAddress>

#ifdef Q_OS_WIN
#include <windows.h>
#include <intrin.h>
#endif

SystemInfo SysInfo::getSystemInfo()
{
    SystemInfo info;
    info.computerName = getComputerName();
    info.osVersion = getOsVersion();
    info.cpuInfo = getCpuInfo();
    getMemoryInfo(info.totalMemory, info.freeMemory);
    info.diskInfo = getDiskInfo();
    info.macAddress = getMacAddress();
    info.ipAddress = getIpAddress();
    return info;
}

QString SysInfo::getComputerName()
{
#ifdef Q_OS_WIN
    WCHAR buffer[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
    if (GetComputerNameW(buffer, &size)) {
        return QString::fromWCharArray(buffer);
    }
#endif
    return QSysInfo::machineHostName();
}

QString SysInfo::getOsVersion()
{
#ifdef Q_OS_WIN
    // 从注册表读取Windows版本信息
    QSettings reg("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 
                  QSettings::NativeFormat);
    QString productName = reg.value("ProductName").toString();
    QString displayVersion = reg.value("DisplayVersion").toString();
    QString buildNumber = reg.value("CurrentBuildNumber").toString();
    
    QString version = productName;
    if (!displayVersion.isEmpty()) {
        version += " " + displayVersion;
    }
    if (!buildNumber.isEmpty()) {
        version += " (Build " + buildNumber + ")";
    }
    return version;
#else
    return QSysInfo::prettyProductName();
#endif
}

QString SysInfo::getCpuInfo()
{
#ifdef Q_OS_WIN
    // 使用CPUID指令获取CPU信息
    int cpuInfo[4] = {0};
    char cpuBrand[49] = {0};
    
    // 获取CPU品牌字符串
    __cpuid(cpuInfo, 0x80000002);
    memcpy(cpuBrand, cpuInfo, sizeof(cpuInfo));
    __cpuid(cpuInfo, 0x80000003);
    memcpy(cpuBrand + 16, cpuInfo, sizeof(cpuInfo));
    __cpuid(cpuInfo, 0x80000004);
    memcpy(cpuBrand + 32, cpuInfo, sizeof(cpuInfo));
    
    QString brand = QString::fromLatin1(cpuBrand).trimmed();
    
    // 获取CPU核心数
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    int coreCount = sysInfo.dwNumberOfProcessors;
    
    return QString("%1 (%2 核)").arg(brand).arg(coreCount);
#else
    return "Unknown CPU";
#endif
}

void SysInfo::getMemoryInfo(quint64& totalMB, quint64& freeMB)
{
#ifdef Q_OS_WIN
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        totalMB = memStatus.ullTotalPhys / (1024 * 1024);
        freeMB = memStatus.ullAvailPhys / (1024 * 1024);
        return;
    }
#endif
    totalMB = 0;
    freeMB = 0;
}

QString SysInfo::getDiskInfo()
{
    QStringList diskInfoList;
    
    foreach (const QStorageInfo& storage, QStorageInfo::mountedVolumes()) {
        if (storage.isValid() && storage.isReady()) {
            QString driveName = storage.rootPath();
            // 只处理本地磁盘
            if (driveName.length() >= 2 && driveName[1] == ':') {
                qint64 totalGB = storage.bytesTotal() / (1024 * 1024 * 1024);
                qint64 freeGB = storage.bytesFree() / (1024 * 1024 * 1024);
                diskInfoList.append(QString("%1 %2GB/%3GB")
                    .arg(driveName.left(2))
                    .arg(freeGB)
                    .arg(totalGB));
            }
        }
    }
    
    return diskInfoList.join(", ");
}

QString SysInfo::getMacAddress()
{
    foreach (const QNetworkInterface& netInterface, QNetworkInterface::allInterfaces()) {
        // 过滤掉回环和非活动接口
        if (netInterface.flags().testFlag(QNetworkInterface::IsLoopBack)) continue;
        if (!netInterface.flags().testFlag(QNetworkInterface::IsUp)) continue;
        if (!netInterface.flags().testFlag(QNetworkInterface::IsRunning)) continue;
        
        QString mac = netInterface.hardwareAddress();
        if (!mac.isEmpty() && mac != "00:00:00:00:00:00") {
            return mac;
        }
    }
    return "";
}

QString SysInfo::getIpAddress()
{
    foreach (const QNetworkInterface& netInterface, QNetworkInterface::allInterfaces()) {
        if (netInterface.flags().testFlag(QNetworkInterface::IsLoopBack)) continue;
        if (!netInterface.flags().testFlag(QNetworkInterface::IsUp)) continue;
        if (!netInterface.flags().testFlag(QNetworkInterface::IsRunning)) continue;
        
        foreach (const QNetworkAddressEntry& entry, netInterface.addressEntries()) {
            QHostAddress ip = entry.ip();
            if (ip.protocol() == QAbstractSocket::IPv4Protocol) {
                QString ipStr = ip.toString();
                // 过滤掉回环地址
                if (!ipStr.startsWith("127.")) {
                    return ipStr;
                }
            }
        }
    }
    return "";
}
