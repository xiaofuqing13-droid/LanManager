#ifndef SOFTMGR_H
#define SOFTMGR_H

#include <QList>
#include <QString>
#include "../Common/protocol.h"

class SoftwareManager {
public:
    // 获取已安装软件列表
    static QList<SoftwareInfo> getInstalledSoftware();
    
    // 安装软件(静默安装)
    // filePath: 安装包路径
    // args: 额外的安装参数(可选)
    // 返回: 是否安装成功
    static bool installSoftware(const QString& filePath, const QString& args = "");
    
    // 卸载软件
    // uninstallCmd: 卸载命令(从软件列表获取)
    // 返回: 是否卸载成功
    static bool uninstallSoftware(const QString& uninstallCmd);
    
private:
    // 从注册表路径读取软件列表
    static void readSoftwareFromRegistry(const QString& regPath, QList<SoftwareInfo>& list);
    
    // 获取静默安装参数
    static QString getSilentArgs(const QString& filePath);
};

#endif // SOFTMGR_H
