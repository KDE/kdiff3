#! /usr/bin/env bash
$EXTRACTRC `find . \( -name \*.rc -o -name \*.ui \) \! -name kdiff3win.rc` >> rc.cpp
$XGETTEXT `find . -name \*.cpp` -o $podir/kdiff3.pot
