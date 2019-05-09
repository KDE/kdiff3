set(KDIFF3_ICONS
     16-apps-kdiff3.png
     22-apps-kdiff3.png
     32-apps-kdiff3.png
     48-apps-kdiff3.png
     64-apps-kdiff3.png
     128-apps-kdiff3.png
     256-apps-kdiff3.png
     sc-apps-kdiff3.svgz
)

# add icons to application sources, to have them bundled
ecm_add_app_icon(kdiff3_SRCS ICONS ${KDIFF3_ICONS})

ecm_install_icons(
  ICONS
   ${KDIFF3_ICONS}
  DESTINATION ${KDE_INSTALL_ICONDIR}
  THEME hicolor
)
