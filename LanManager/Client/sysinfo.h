#ifndef SYSINFO_H
#define SYSINFO_H

#include <QString>
#include <QStringList>
#include "../Common/protocol.h"

class SysInfo {
public:
    // 获取完整系统信息
    static SystemInfo getSystemInfo();
    
    // 获取计算机名
    static QString getComputerName();
    
    // 获取操作系统版本
    static QString getOsVersion();
    
    // 获取CPU信息
    static QString getCpuInfo();
    
    // 获取内存信息(返回总内存和可用内存,单位MB)
    static void getMemoryInfo(quint64& totalMB, quint64& freeMB);
    
    // 获取磁盘信息
    static QString getDiskInfo();
    
    // 获取MAC地址
    static QString getMacAddress();
    
    // 获取IP地址
    static QString getIpAddress();
};

#endif // SYSINFO_H
