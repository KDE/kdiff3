TEMPLATE	= app
CONFIG		+= qt warn-on release
HEADERS  = diff.h kdiff3.h merger.h optiondialog.h kreplacements/kreplacements.h \
                  directorymergewindow.h fileaccess.h kdiff3_shell.h kdiff3_part.h
SOURCES  = diff.cpp difftextwindow.cpp kdiff3.cpp main.cpp merger.cpp mergeresultwindow.cpp \
                  optiondialog.cpp pdiff.cpp directorymergewindow.cpp fileaccess.cpp \
                  kdiff3_shell.cpp kdiff3_part.cpp kreplacements/kreplacements.cpp
TARGET		= kdiff3
INCLUDEPATH	+= . ./kreplacements
REQUIRES             = full-config
win32:RC_FILE        = kdiff3.rc
win32:TMAKE_CFLAGS   = -GX -GR
win32:TMAKE_CXXFLAGS = -GX -GR