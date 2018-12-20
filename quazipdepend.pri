QUAZIP_LIBNAME = QuaZipAC

CONFIG(debug, debug|release) {
    CONFIG_DIR = Debug
    win32:CONFIG_WINDIR = debug
} else {
    CONFIG_DIR = Release
    win32:CONFIG_WINDIR = release
}

isEmpty(QUAZIP_LIBPATH) {
    QUAZIP_LIBPATH = $$OUT_PWD
}

QUAZIP_LIBPATH = $$QUAZIP_LIBPATH/$$CONFIG_WINDIR

win32 {
    INCLUDEPATH += $$[QT_INSTALL_HEADERS]/QtZlib
    PRE_TARGETDEPS += $$QUAZIP_LIBPATH/$$QUAZIP_LIBNAME.lib
}

unix:LIBS += -lz
LIBS += -L$$QUAZIP_LIBPATH
LIBS += -l$$QUAZIP_LIBNAME

CONFIG(staticlib) {
    DEFINES += QUAZIP_STATIC
} else {
    macx {
        DYNAMIC_LIBS.files += \
            $$QUAZIP_LIBPATH/$$join(QUAZIP_LIBNAME, , lib, .1.dylib)
    }
}

INCLUDEPATH += $$PWD
