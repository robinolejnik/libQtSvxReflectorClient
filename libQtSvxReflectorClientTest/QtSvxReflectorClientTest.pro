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

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
