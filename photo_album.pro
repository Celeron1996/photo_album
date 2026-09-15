QT += core gui widgets multimedia multimediawidgets

TARGET = PhotoAlbum
TEMPLATE = app
CONFIG += c++11

SOURCES += \
    main.cpp \
    albumwindow.cpp \
    playlistmodel.cpp \
    imagetransitionwidget.cpp

HEADERS += \
    albumwindow.h \
    playlistmodel.h \
    imagetransitionwidget.h

# ========== IMX6ULL Qt 头文件路径（与 qt_camera.pro 保持一致） ==========
INCLUDEPATH += /home/book/100ask_imx6ull-sdk/Buildroot_2020.02.x/output/host/arm-buildroot-linux-gnueabihf/sysroot/usr/include/qt5
INCLUDEPATH += /home/book/100ask_imx6ull-sdk/Buildroot_2020.02.x/output/host/arm-buildroot-linux-gnueabihf/sysroot/usr/include/qt5/QtWidgets
INCLUDEPATH += /home/book/100ask_imx6ull-sdk/Buildroot_2020.02.x/output/host/arm-buildroot-linux-gnueabihf/sysroot/usr/include/qt5/QtMultimedia
INCLUDEPATH += /home/book/100ask_imx6ull-sdk/Buildroot_2020.02.x/output/host/arm-buildroot-linux-gnueabihf/sysroot/usr/include/qt5/QtMultimediaWidgets
INCLUDEPATH += /home/book/100ask_imx6ull-sdk/Buildroot_2020.02.x/output/host/arm-buildroot-linux-gnueabihf/sysroot/usr/include/qt5/QtCore

# ARM 编译工具链
QMAKE_CC  = arm-buildroot-linux-gnueabihf-gcc
QMAKE_CXX = arm-buildroot-linux-gnueabihf-g++
QMAKE_LINK = arm-buildroot-linux-gnueabihf-g++

# 关闭无关警告
QMAKE_CXXFLAGS += -Wno-all -Wno-psabi
