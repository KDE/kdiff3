KDiff3-Readme for Windows
=========================

Author: Joachim Eibl  (joachim.eibl@gmx.de)
Copyright: (C) 2002-2003 by Joachim Eibl
KDiff3-Version: 0.9.80
Homepage: http://kdiff3.sourceforge.net

KDiff3 is a program that
- compares and merges two or three input files or directories,
- shows the differences line by line and character by character (!),
- provides an automatic merge-facility and
- an integrated editor for comfortable solving of merge-conflicts
- and has an intuitive graphical user interface.

See the Changelog.txt for a list of fixed bugs and new features.

Windows-specific information for the precompiled KDiff3 version:
================================================================

This executable is provided for the convenience of users who don't have a
VC6-compiler at hand.

You may redistribute it under the terms of the GNU GENERAL PUBLIC LICENCE.

Note that there is NO WARRANTY for this program.

Installation:
- The installer was created by Sebastien Fricker (sebastien.fricker@web.de).
  It is based on the Nullsoft Scriptable Install System (http://nsis.sourceforge.net)

- You can place the directory where you want it. But don't separate the file
  kdiff3.exe from the others, since they are needed for correct execution.

- Integration with WinCVS: When selected the installer sets KDiff3 to be the
  default diff-tool for WinCVS if available.

- Integration with Explorer: When selected KDiff3 will be added to the "Send To"
  menu in the context menu. If you then select two files or two directories and
  choose "Send To"->"KDiff3" then KDiff3 will start and compare the specified files.

Since this program was actually developed for GNU/Linux, there might be Windows
specific problems I don't know of yet. Please write me about problems you encounter.

Known bugs:
- Links are not handled correctly. (This is because links in Windows are not
  the same as under Un*x-filesystems.)

Licence:
    GNU GENERAL PUBLIC LICENSE, Version 2, June 1991
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    For details see file "COPYING".

Exception from the GPL:
    As a special exception, the copyright holder Joachim Eibl gives permission
    to link this program with the Qt-library (commercial or non-commercial edition)
    from Trolltech (www.trolltech.com), and he permits to distribute the resulting
    executable, without including the source code for the Qt-library in the
    source distribution.



Start from commandline:
- Comparing 2 files:       kdiff3 file1 file2
- Merging 2 files:         kdiff3 file1 file2 -o outputfile
- Comparing 3 files:       kdiff3 file1 file2 file3
- Merging 3 files:         kdiff3 file1 file2 file3 -o outputfile
     Note that file1 will be treated as base of file2 and file3.

If all files have the same name but are in different directories, you can
reduce typework by specifying the filename only for the first file. E.g.:
- Comparing 3 files:     kdiff3 dir1/filename dir2 dir3
(This also works in the open-dialog.)

- Comparing 2 directories: kdiff3 dir1 dir2
- Merging 2 directories:   kdiff3 dir1 dir2-o destinationdir
- Comparing 3 directories: kdiff3 dir1 dir2 dir3
- Merging 3 directories:   kdiff3 dir1 dir2 dir3 -o destinationdir
(Please read the documentation about comparing/merging directories,
especially before you start merging.)

If you start without arguments, then a dialog will appear where you can
select your files and directories via a filebrowser.

For more documentation, see the help-menu or the subdirectory doc.

Have fun!
