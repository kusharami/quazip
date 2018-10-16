TEMPLATE = app
QT -= gui
QT += network testlib
CONFIG += console
CONFIG -= app_bundle
DEPENDPATH += .
INCLUDEPATH += .
!win32: LIBS += -lz
win32 {
    INCLUDEPATH += $$[QT_INSTALL_HEADERS]/QtZlib
    # workaround for qdatetime.h macro bug
    DEFINES += NOMINMAX
}
DEFINES += ZLIB_CONST

CONFIG(staticlib): DEFINES += QUAZIP_STATIC

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

win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../quazip/release/ -lquazip
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../quazip/debug/ -lquazipd
else:mac:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../quazip/ -lquazip_debug
else:unix: LIBS += -L$$OUT_PWD/../quazip/ -lquazip

INCLUDEPATH += $$PWD/..
DEPENDPATH += $$PWD/../quazip
