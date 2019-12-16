QUAZIP_LIBNAME = QuaZipAC

CONFIG(debug, debug|release) {
    win32:CONFIG_WINDIR = debug
} else {
    win32:CONFIG_WINDIR = release
}

isEmpty(QUAZIP_LIBPATH) {
    QUAZIP_LIBPATH = $$OUT_PWD
}

QUAZIP_LIBPATH = $$QUAZIP_LIBPATH/$$CONFIG_WINDIR

win32|emscripten:INCLUDEPATH += $$[QT_INSTALL_HEADERS]/QtZlib
else:unix:LIBS += -lz

win32 {
    PRE_TARGETDEPS += $$QUAZIP_LIBPATH/$$QUAZIP_LIBNAME.lib
}

LIBS += -L$$QUAZIP_LIBPATH
LIBS += -l$$QUAZIP_LIBNAME

CONFIG(static, static) {
    DEFINES += QUAZIP_STATIC
} else {
    macx {
        DYNAMIC_LIBS.files += \
            $$QUAZIP_LIBPATH/$$join(QUAZIP_LIBNAME, , lib, .1.dylib)
    }
}

INCLUDEPATH += $$PWD
