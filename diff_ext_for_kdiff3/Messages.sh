#! /usr/bin/env bash
# Translator::translate/translateContext aren't part of scripty's default KDE i18n keyword set.
$XGETTEXT *.cpp -kTranslator::translate:1 -kTranslator::translateContext:1c,2 -o $podir/diff_ext.pot

