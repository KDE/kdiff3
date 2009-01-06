#! /usr/bin/env bash
$XGETTEXT -kaliasLocal `find -name \*.cpp -o -name \*.h` -o $podir/kdiff3plugin.pot
