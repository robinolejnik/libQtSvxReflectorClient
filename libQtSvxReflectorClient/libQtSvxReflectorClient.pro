QT -= gui
QT += core network multimedia

TARGET = libQtSvxReflectorClient
TEMPLATE = lib
CONFIG += static shared
DESTDIR = $$OUT_PWD/../bin

DEFINES += LIBQTSVXREFLECTORCLIENT_LIBRARY
DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

INCLUDEPATH += libs/include
LIBS += -L$$PWD/libs/lib
LIBS += -lgcrypt -lopus

SOURCES += qtsvxreflectorclient_p.cpp qtsvxreflectorclient.cpp
HEADERS += qtsvxreflectorclient.h libqtsvxreflectorclient_global.h ReflectorMsg.h qtsvxreflectorclient_p.h AsyncMsg.h

unix {
    target.path = /usr/lib
    INSTALLS += target
}

DISTFILES += libQtSvxReflectorClient.pri

copydata.commands = $(COPY_DIR) \"$$shell_path($$PWD/libs/bin/*.dll)\" \"$$shell_path($$DESTDIR)\"
first.depends = $(first) copydata
export(first.depends)
export(copydata.commands)
QMAKE_EXTRA_TARGETS += first copydata
