TEMPLATE = subdirs
SUBDIRS = lib app
DESTDIR = build
DLLDESTDIR = dllbuild
lib.subdir = libQtSvxReflectorClient
app.subdir = libQtSvxReflectorClientTest
app.depends = lib
CONFIG += c++17
