QT += core gui network widgets multimedia serialport

TARGET = QtSvxReflectorClientTest
TEMPLATE = app
DESTDIR = $$OUT_PWD/../bin

include("../libQtSvxReflectorClient/libQtSvxReflectorClient.pri")

DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += main.cpp mainwindow.cpp

HEADERS += mainwindow.h

FORMS += MainWindow.ui

LIBS += -lUser32

QMAKE_POST_LINK += $$quote(windeployqt \"$$shell_path($$DESTDIR)\")
