#include "softmgr.h"
#include <QSettings>
#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

QList<SoftwareInfo> SoftwareManager::getInstalledSoftware()
{
    QList<SoftwareInfo> softwareList;
    
    // 读取64位程序注册表
    readSoftwareFromRegistry(
        "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
        softwareList);
    
    // 读取32位程序注册表(在64位系统上)
    readSoftwareFromRegistry(
        "HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
        softwareList);
    
    // 读取当前用户安装的程序
    readSoftwareFromRegistry(
        "HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
        softwareList);
    
    return softwareList;
}

void SoftwareManager::readSoftwareFromRegistry(const QString& regPath, QList<SoftwareInfo>& list)
{
    QSettings reg(regPath, QSettings::NativeFormat);
    
    foreach (const QString& subKey, reg.childGroups()) {
        reg.beginGroup(subKey);
        
        QString displayName = reg.value("DisplayName").toString();
        
        // 过滤掉没有名称的条目和更新补丁
        if (!displayName.isEmpty() && !displayName.contains("Update") &&
            !displayName.contains("KB") && !reg.value("SystemComponent", 0).toBool()) {
            
            SoftwareInfo info;
            info.name = displayName;
            info.version = reg.value("DisplayVersion").toString();
            info.publisher = reg.value("Publisher").toString();
            info.installDate = reg.value("InstallDate").toString();
            info.installPath = reg.value("InstallLocation").toString();
            info.uninstallCmd = reg.value("UninstallString").toString();
            
            // 避免重复
            bool exists = false;
            for (const SoftwareInfo& existing : list) {
                if (existing.name == info.name && existing.version == info.version) {
                    exists = true;
                    break;
                }
            }
            
            if (!exists && !info.uninstallCmd.isEmpty()) {
                list.append(info);
            }
        }
        
        reg.endGroup();
    }
}

bool SoftwareManager::installSoftware(const QString& filePath, const QString& args)
{
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        qWarning() << "Installation file not found:" << filePath;
        return false;
    }
    
    QString extension = fileInfo.suffix().toLower();
    QString command;
    QStringList arguments;
    
    if (extension == "msi") {
        // MSI安装包
        command = "msiexec";
        arguments << "/i" << filePath << "/quiet" << "/norestart";
        if (!args.isEmpty()) {
            arguments << args.split(' ', Qt::SkipEmptyParts);
        }
    } else if (extension == "exe") {
        // EXE安装包 - 尝试常见的静默安装参数
        command = filePath;
        QString silentArgs = getSilentArgs(filePath);
        if (!args.isEmpty()) {
            arguments << args.split(' ', Qt::SkipEmptyParts);
        } else if (!silentArgs.isEmpty()) {
            arguments << silentArgs.split(' ', Qt::SkipEmptyParts);
        }
    } else if (extension == "bat" || extension == "cmd") {
        // 批处理脚本
        command = "cmd.exe";
        arguments << "/c" << filePath;
        if (!args.isEmpty()) {
            arguments << args.split(' ', Qt::SkipEmptyParts);
        } else {
            arguments << "/S";  // 默认静默参数
        }
    } else {
        qWarning() << "Unsupported installation package format:" << extension;
        return false;
    }
    
    QProcess process;
    process.start(command, arguments);
    
    // 等待安装完成,最长等待10分钟
    if (!process.waitForFinished(600000)) {
        qWarning() << "Installation timeout";
        return false;
    }
    
    int exitCode = process.exitCode();
    if (exitCode != 0) {
        qWarning() << "Installation failed with exit code:" << exitCode;
        qWarning() << "Error output:" << process.readAllStandardError();
        return false;
    }
    
    return true;
}

bool SoftwareManager::uninstallSoftware(const QString& uninstallCmd)
{
    if (uninstallCmd.isEmpty()) {
        qWarning() << "Uninstall command is empty";
        return false;
    }
    
    QString cmd = uninstallCmd;
    QStringList arguments;
    
    // 解析卸载命令
    // 处理带引号的路径
    if (cmd.startsWith("\"")) {
        int endQuote = cmd.indexOf("\"", 1);
        if (endQuote > 0) {
            QString exe = cmd.mid(1, endQuote - 1);
            QString rest = cmd.mid(endQuote + 1).trimmed();
            cmd = exe;
            if (!rest.isEmpty()) {
                arguments = rest.split(' ', Qt::SkipEmptyParts);
            }
        }
    } else if (cmd.contains(" ")) {
        // 没有引号但有空格
        int space = cmd.indexOf(' ');
        QString rest = cmd.mid(space + 1).trimmed();
        cmd = cmd.left(space);
        if (!rest.isEmpty()) {
            arguments = rest.split(' ', Qt::SkipEmptyParts);
        }
    }
    
    // 检查是否是MSI卸载
    if (cmd.toLower().contains("msiexec")) {
        // 确保静默卸载
        if (!arguments.contains("/quiet") && !arguments.contains("/q")) {
            arguments << "/quiet" << "/norestart";
        }
    } else {
        // EXE卸载程序 - 尝试添加静默参数
        bool hasSilent = false;
        for (const QString& arg : arguments) {
            if (arg.toLower() == "/s" || arg.toLower() == "/silent" ||
                arg.toLower() == "/q" || arg.toLower() == "/quiet" ||
                arg.toLower() == "-s" || arg.toLower() == "-silent") {
                hasSilent = true;
                break;
            }
        }
        if (!hasSilent) {
            // 尝试常见的静默参数
            arguments << "/S";
        }
    }
    
    QProcess process;
    process.start(cmd, arguments);
    
    // 等待卸载完成,最长等待5分钟
    if (!process.waitForFinished(300000)) {
        qWarning() << "Uninstall timeout";
        return false;
    }
    
    int exitCode = process.exitCode();
    if (exitCode != 0 && exitCode != 1605) { // 1605 = 产品未安装
        qWarning() << "Uninstall failed with exit code:" << exitCode;
        return false;
    }
    
    return true;
}

QString SoftwareManager::getSilentArgs(const QString& filePath)
{
    QString fileName = QFileInfo(filePath).fileName().toLower();
    
    // 根据常见安装程序类型返回对应的静默参数
    if (fileName.contains("inno") || fileName.contains("setup")) {
        return "/VERYSILENT /SUPPRESSMSGBOXES /NORESTART";
    } else if (fileName.contains("nsis")) {
        return "/S";
    } else if (fileName.contains("installshield")) {
        return "/s /v\"/qn\"";
    }
    
    // 默认尝试通用静默参数
    return "/S /silent /quiet";
}
