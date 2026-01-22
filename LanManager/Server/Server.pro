QT += core gui widgets network

CONFIG += c++17

TARGET = LanServer
TEMPLATE = app

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    tcpserver.cpp

HEADERS += \
    mainwindow.h \
    tcpserver.h \
    ../Common/protocol.h

INCLUDEPATH += ../Common

# 输出目录
DESTDIR = ../bin

# Windows特定配置
win32 {
    RC_ICONS = 
}
