# SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
# SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
# SPDX-License-Identifier: GPL-2.0-or-later

set(KDIFF3_PNG_ICONS
     icons/16-apps-kdiff3.png
     icons/22-apps-kdiff3.png
     icons/32-apps-kdiff3.png
     icons/48-apps-kdiff3.png
     icons/64-apps-kdiff3.png
     icons/128-apps-kdiff3.png
     icons/256-apps-kdiff3.png)

set(KDIFF3_ICONS
     ${KDIFF3_PNG_ICONS}
     icons/sc-apps-kdiff3.svgz
)

# add icons to application sources, to have them bundled -- must be in same scope as src/CMakeLists.txt
ecm_add_app_icon(kdiff3_SRCS ICONS ${KDIFF3_PNG_ICONS})

ecm_install_icons(
  ICONS
   ${KDIFF3_ICONS}
  DESTINATION ${KDE_INSTALL_ICONDIR}
  THEME hicolor
)
