CONFIG(debug, debug|release) {
    CONFIG_DIR = Debug
    macx {
        QUAZIP_LIBNAME = quazip_debug
    }
    win32 {
        QUAZIP_LIBNAME = quazipd
        CONFIG_WINDIR = debug
    }
} else {
    CONFIG_DIR = Release
    QUAZIP_LIBNAME = quazip
    win32 {
        CONFIG_WINDIR = release
    }
}

isEmpty(QUAZIP_LIBPATH) {
    QUAZIP_LIBPATH = $$OUT_PWD
}

win32 {
    INCLUDEPATH += $$[QT_INSTALL_HEADERS]/QtZlib
}

unix {
    LIBS += -lz
}

QUAZIP_LIBPATH = $$QUAZIP_LIBPATH/$$CONFIG_WINDIR
LIBS += -L$$QUAZIP_LIBPATH
LIBS += -l$$QUAZIP_LIBNAME

macx {
    DYNAMIC_LIBS.files += \
        $$QUAZIP_LIBPATH/$$join(QUAZIP_LIBNAME, , lib, .1.dylib)
}

INCLUDEPATH += $$PWD
