rem set QTDIR=f:\qt\3.2.1\
echo QTDIR=%QTDIR%
del kdiff3setup.exe
del kdiff3.exe
qmake kdiff3.pro "CONFIG+=release" "DEFINES+=NO_DEBUG" 
rem nmake distclean
rem nmake clean
nmake
lupdate kdiff3.pro
lrelease kdiff3.pro
cl version.c -o version.exe
version nsis > version.nsi
makensis.exe  /DQTDIR=%QTDIR% /DWINDOWS_DIR=%WINDIR%  kdiff3

