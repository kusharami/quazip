TEMPLATE = lib
CONFIG += qt warn_on
QT -= gui

!win32 {
    # Creating pkgconfig .pc file
    CONFIG += create_prl no_install_prl create_pc
}

DEFINES += QT_NO_DEPRECATED_WARNINGS

clang:QMAKE_CXXFLAGS_WARN_ON += \
    -Wno-deprecated-copy \
    -Wno-unknown-warning-option

QMAKE_PKGCONFIG_PREFIX = $$PREFIX
QMAKE_PKGCONFIG_INCDIR = $$headers.path
QMAKE_PKGCONFIG_REQUIRES = Qt5Core

# The ABI version.

TARGET = QuaZipAC
VERSION = 1.2.1

# This one handles dllimport/dllexport directives.
DEFINES += QUAZIP_BUILD

# You'll need to define this one manually if using a build system other
# than qmake or using QuaZIP sources directly in your project.
CONFIG(static, static) {
    DEFINES += QUAZIP_STATIC
} else {
    win32 {
        TARGET_EXT = .dll
    }
    
    macx {
        QMAKE_SONAME_PREFIX = @executable_path/../lib
    }
}

INCLUDEPATH += $$PWD

# Input
include(quazip.pri)

