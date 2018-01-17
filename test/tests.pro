TEMPLATE = app
CONFIG  += qt warn_on thread precompile_header debug

HEADERS  = ../src-QT4/kreplacements/kreplacements.h \
           ../src-QT4/fileaccess.h \
           ../src-QT4/progress.h
SOURCES = alignmenttest.cpp \
          ../src-QT4/common.cpp \
          ../src-QT4/diff.cpp \
          ../src-QT4/fileaccess.cpp \
          ../src-QT4/gnudiff_analyze.cpp \
          ../src-QT4/gnudiff_io.cpp \
          ../src-QT4/gnudiff_xmalloc.cpp \
          ../src-QT4/kreplacements/kreplacements.cpp \
          fakekdiff3_part.cpp \
          fakeprogressproxy.cpp


TARGET = alignmenttest
INCLUDEPATH += ../src-QT4 ../src-QT4/kreplacements



QMAKE_EXTRA_TARGETS = check

check.depends = alignmenttest
check.commands = ./alignmenttest


