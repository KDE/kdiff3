TEMPLATE = app
# When unresolved items remain during linking: Try adding "shared" in the CONFIG.
CONFIG  += qt warn_on thread precompile_header
!os2:PRECOMPILED_HEADER = stable.h
HEADERS  = version.h                     \
           diff.h                        \
           difftextwindow.h              \
           mergeresultwindow.h           \
           kdiff3.h                      \
           merger.h                      \
           optiondialog.h                \
           options.h                     \
           progress.h                    \
           kreplacements/kreplacements.h \
           directorymergewindow.h        \
           fileaccess.h                  \
           kdiff3_shell.h                \
           kdiff3_part.h                 \
           smalldialogs.h
SOURCES  = main.cpp                      \
           diff.cpp                      \
           difftextwindow.cpp            \
           kdiff3.cpp                    \
           merger.cpp                    \
           mergeresultwindow.cpp         \
           optiondialog.cpp              \
           pdiff.cpp                     \
           directorymergewindow.cpp      \
           fileaccess.cpp                \
           progress.cpp                  \
           smalldialogs.cpp              \
           kdiff3_shell.cpp              \
           kdiff3_part.cpp               \
           gnudiff_analyze.cpp           \
           gnudiff_io.cpp                \
           gnudiff_xmalloc.cpp           \
           common.cpp                    \
           stable.cpp                    \
           kreplacements/kreplacements.cpp \
           kreplacements/ShellContextMenu.cpp
TARGET   = kdiff3
INCLUDEPATH += . ./kreplacements

win32 {
#   QMAKE_CXXFLAGS_DEBUG  -= -Zi
#   QMAKE_CXXFLAGS_DEBUG  += -GX -GR -Z7 /FR -DQT_NO_ASCII_CAST
#   QMAKE_LFLAGS_DEBUG  += /PDB:NONE
#   QMAKE_CXXFLAGS_RELEASE  += -GX -GR -DNDEBUG -DQT_NO_ASCII_CAST

   QMAKE_CXXFLAGS_DEBUG  += -DQT_NO_ASCII_CAST
   QMAKE_CXXFLAGS_RELEASE  += -DNDEBUG -DQT_NO_ASCII_CAST
   RC_FILE = kdiff3win.rc
   win32-g++ {
           QMAKE_LFLAGS += -luser32 -lshell32
           QMAKE_LFLAGS_RELEASE += -static-libgcc  # Remove dependency to libgcc_s_dw2-1.dll
           QMAKE_LFLAGS_EXCEPTIONS_ON -= -mthreads # Remove dependency to mingwm10.dll (hack, yet no problem if not multithreaded)
                                                   # Better: configure -static -release -no-exceptions -fast
   } else {
           QMAKE_LFLAGS += user32.lib shell32.lib
   }
}
os2 {
  RC_FILE = kdiff3os2.rc
}

unix {
  documentation.path = /usr/local/share/doc/kdiff3
  documentation.files = ../doc/*

  INSTALLS += documentation

  target.path = /usr/local/bin
  INSTALLS += target
}
