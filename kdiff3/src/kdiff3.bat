set DEBUG=1
echo QTDIR=%QTDIR%
del Makefile
del kdiff3setup*.exe
del kdiff3.pdb
del kdiff3.exe
if %DEBUG%==1 (
  echo debug mode
  qmake kdiff3.pro "CONFIG-=release" "CONFIG+=debug" 
) else (
  qmake kdiff3.pro "CONFIG+=release" "DEFINES+=NO_DEBUG" 
)

 nmake distclean
 nmake clean
nmake
del kdiff3.zip
nmake dist
lupdate kdiff3.pro
lrelease kdiff3.pro
cl version.c -o version.exe
version nsis > version.nsi
 del /S /Q tmp
md tmp
cd tmp
 wget --mirror http://kdiff3.sourceforge.net/doc/index.html
md source
cd source
unzip -x ..\..\kdiff3.zip
cd ..
cd ..
if %DEBUG%==1 (
  makensis.exe  /DQTDIR=%QTDIR% /DWINDOWS_DIR=%WINDIR%  /DDEBUG=%DEBUG% kdiff3
) else (
  makensis.exe  /DQTDIR=%QTDIR% /DWINDOWS_DIR=%WINDIR%  kdiff3
)
pscp  kdiff3setup*.exe friseb123@kdiff3.sourceforge.net:/home/groups/k/kd/kdiff3/htdocs
