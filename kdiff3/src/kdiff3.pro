TEMPLATE = app
CONFIG  += qt warn_on thread release
HEADERS  = diff.h kdiff3.h merger.h optiondialog.h kreplacements/kreplacements.h \
                  directorymergewindow.h fileaccess.h kdiff3_shell.h kdiff3_part.h
SOURCES  = diff.cpp difftextwindow.cpp kdiff3.cpp main.cpp merger.cpp mergeresultwindow.cpp \
                  optiondialog.cpp pdiff.cpp directorymergewindow.cpp fileaccess.cpp \
                  kdiff3_shell.cpp kdiff3_part.cpp kreplacements/kreplacements.cpp
TARGET   = kdiff3
INCLUDEPATH += . ./kreplacements

win32 {
   QMAKE_CXXFLAGS_DEBUG  -= -Zi
   QMAKE_CXXFLAGS_DEBUG  += -GX -GR -Z7
   QMAKE_LFLAGS_DEBUG  += /PDB:NONE
   QMAKE_CXXFLAGS_RELEASE  += -GX -GR -DNDEBUG
   RC_FILE = kdiff3.rc
}
unix {
  documentation.path = /usr/local/share/doc/kdiff3
  documentation.files = ../doc/*

  INSTALLS += documentation

  target.path = /usr/local/bin
  INSTALLS += target
}
