Clearcase install helper plugin DLL-Readme
==========================================
Copyright (C) 2007 Joachim Eibl, All rights reserved.
License: GPL Version 2 or any later version.

This subdirectory contains files to build a NSIS-plugin which is
called during installation and uninstallation.

The plugin tries to locate clearcase by looking for cleartool.exe in the
PATH environment variable.

Then it searches for the "map" file relative to the cleartool.exe in
../lib/mgrs/map.

This file contains a map which tells clearcase which tool to use for a certain
operation. It is readable and modifiable with any text editor.

During installation this plugin replaces several entries with the 
KDiff3-executable.
The original file is renamed into "map.preKDiff3Install".

During uninstallation the plugin restores the original contents from the 
"map.preKDiff3Install" but only for those entries where KDiff3 is used.

NSIS integrates the plugin as part of the installer executable and in the 
"Uninstall.exe" file.



