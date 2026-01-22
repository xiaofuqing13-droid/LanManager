QT += core network
QT -= gui

CONFIG += c++17 console
CONFIG -= app_bundle

TARGET = LanClient
TEMPLATE = app

# Windows特定配置
win32 {
    LIBS += -liphlpapi
    RC_FILE = client.rc
}

SOURCES += \
    main.cpp \
    agent.cpp \
    sysinfo.cpp \
    softmgr.cpp

HEADERS += \
    agent.h \
    sysinfo.h \
    softmgr.h \
    ../Common/protocol.h

INCLUDEPATH += ../Common

# 输出目录
DESTDIR = ../bin
