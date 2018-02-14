#! /usr/bin/env bash
$EXTRACTRC *.rc >> rc.cpp
$XGETTEXT `find -name \*.cpp -o -name \*.h` -o $podir/kdiff3.pot
