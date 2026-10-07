#! /usr/bin/env bash
# Translator::translate/translateContext aren't part of scripty's default KDE i18n keyword set.
$XGETTEXT *.cpp -ktr:1 -ktrContext:1c,2 -o $podir/diff_ext.pot

