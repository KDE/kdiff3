rem set QTDIR=f:\qt\3.2.1\
echo QTDIR=%QTDIR%
del kdiff3setup.exe
del kdiff3.exe
qmake kdiff3.pro "CONFIG+=release" "DEFINES+=NO_DEBUG" 
nmake distclean
nmake clean
nmake
lupdate kdiff3.pro
lrelease kdiff3.pro
cl version.c -o version.exe
version nsis > version.nsi
md tmp
cd tmp
wget --mirror http://kdiff3.sourceforge.net/doc/index.html
cd ..
makensis.exe  /DQTDIR=%QTDIR% /DWINDOWS_DIR=%WINDIR%  kdiff3
pscp  kdiff3setup*.exe friseb123@kdiff3.sourceforge.net:/home/groups/k/kd/kdiff3/htdocs
