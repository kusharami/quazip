win32:INCLUDEPATH += $$[QT_INSTALL_HEADERS]/QtZlib
unix:LIBS += -lz

DEFINES += ZLIB_CONST

DEPENDPATH += $$PWD
HEADERS += \
        $$PWD/minizip_crypt.h \
        $$PWD/ioapi.h \
        $$PWD/JlCompress.h \
        $$PWD/quaadler32.h \
        $$PWD/quachecksum32.h \
        $$PWD/quacrc32.h \
        $$PWD/quaziodevice.h \
        $$PWD/quazipdir.h \
        $$PWD/quazipfile.h \
        $$PWD/quazipfileinfo.h \
        $$PWD/quazip_global.h \
        $$PWD/quazip.h \
        $$PWD/unzip.h \
        $$PWD/zip.h \
    $$PWD/quagzipdevice.h \
    $$PWD/private/quaziodeviceprivate.h \
    $$PWD/private/quazipextrafields_p.h \
    $$PWD/quaziptextcodec.h \
    $$PWD/quazextrafield.h \
    $$PWD/minizip_crypt.h \
    $$PWD/quazutils.h \
    $$PWD/quazipkeysgenerator.h \
    $$PWD/quazipextrafields_p.h

SOURCES += $$PWD/qioapi.cpp \
           $$PWD/JlCompress.cpp \
           $$PWD/quaadler32.cpp \
           $$PWD/quacrc32.cpp \
           $$PWD/quaziodevice.cpp \
           $$PWD/quazip.cpp \
           $$PWD/quazipdir.cpp \
           $$PWD/quazipfile.cpp \
           $$PWD/quazipfileinfo.cpp \
           $$PWD/unzip.c \
           $$PWD/zip.c \
    $$PWD/quagzipdevice.cpp \
    $$PWD/private/quaziodeviceprivate.cpp \
    $$PWD/quaziptextcodec.cpp \
    $$PWD/quazextrafield.cpp \
    $$PWD/quazutils.cpp \
    $$PWD/quazipkeysgenerator.cpp \
    $$PWD/quachecksum32.cpp
