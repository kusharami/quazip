TEMPLATE = app
QT -= gui
QT += network testlib
CONFIG += console
CONFIG -= app_bundle
DEFINES += ZLIB_CONST

QUAZIP_LIBPATH = $$OUT_PWD/../quazip

include($$PWD/../quazipdepend.pri)

# Input
HEADERS += qztest.h \
testjlcompress.h \
testquachecksum32.h \
testquaziodevice.h \
testquazipdir.h \
testquazipfile.h \
testquazip.h \
    testquazipnewinfo.h \
    testquazipfileinfo.h \
    testquagzipdevice.h

SOURCES += qztest.cpp \
testjlcompress.cpp \
testquachecksum32.cpp \
testquaziodevice.cpp \
testquazip.cpp \
testquazipdir.cpp \
testquazipfile.cpp \
    testquazipnewinfo.cpp \
    testquazipfileinfo.cpp \
    testquagzipdevice.cpp

OBJECTS_DIR = .obj
MOC_DIR = .moc

INCLUDEPATH += $$PWD/..
DEPENDPATH += $$PWD/../quazip
