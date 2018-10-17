TEMPLATE=subdirs
SUBDIRS=quazip qztest
qztest.depends = quazip

OTHER_FILES += \
    .clang-format \
    README.md \
    NEWS.txt \
    COPYING
