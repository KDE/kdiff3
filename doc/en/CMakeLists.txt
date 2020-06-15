# SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
# SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
# SPDX-License-Identifier: GPL-2.0-or-later

kdoctools_create_handbook(index.docbook INSTALL_DESTINATION ${KDE_INSTALL_DOCBUNDLEDIR}/en SUBDIR kdiff3)
kdoctools_create_manpage (man-kdiff3.1.docbook 1 INSTALL_DESTINATION ${KDE_INSTALL_MANDIR})
