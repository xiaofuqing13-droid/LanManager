#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("LanManager Server");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("LanManager");
    
    // 设置中文字体
    QFont font("Microsoft YaHei", 9);
    app.setFont(font);
    
    MainWindow window;
    window.show();
    
    return app.exec();
}
