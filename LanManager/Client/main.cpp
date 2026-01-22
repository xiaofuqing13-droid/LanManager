#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>
#include "agent.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("LanManager Client");
    app.setApplicationVersion("1.0.0");
    
    // 命令行参数解析
    QCommandLineParser parser;
    parser.setApplicationDescription("局域网远程管理客户端Agent");
    parser.addHelpOption();
    parser.addVersionOption();
    
    QCommandLineOption serverOption(
        QStringList() << "s" << "server",
        "服务器地址 (留空则自动发现)",
        "address",
        ""
    );
    parser.addOption(serverOption);
    
    QCommandLineOption portOption(
        QStringList() << "p" << "port",
        "服务器端口",
        "port",
        QString::number(DEFAULT_PORT)
    );
    parser.addOption(portOption);
    
    parser.process(app);
    
    QString serverAddress = parser.value(serverOption);
    quint16 serverPort = parser.value(portOption).toUShort();
    
    qInfo() << "===================================";
    qInfo() << "局域网远程管理客户端 v1.0.0";
    qInfo() << "===================================";
    
    Agent agent;
    
    // 日志输出
    QObject::connect(&agent, &Agent::logMessage, [](const QString& msg) {
        qInfo() << "[Agent]" << msg;
    });
    
    if (serverAddress.isEmpty()) {
        // 自动发现模式
        qInfo() << "启动自动发现模式,等待服务器广播...";
        agent.startAutoDiscovery();
    } else {
        // 指定服务器模式
        qInfo() << "连接服务器:" << serverAddress << ":" << serverPort;
        agent.connectToServer(serverAddress, serverPort);
    }
    
    return app.exec();
}
