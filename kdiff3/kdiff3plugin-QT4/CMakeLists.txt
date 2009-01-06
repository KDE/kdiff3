########### kdiff3 plugin ###############
find_package(LibKonq REQUIRED)
set(kdiff3plugin_SRCS 
   kdiff3plugin.cpp ) 

kde4_add_plugin(kdiff3plugin WITH_PREFIX ${kdiff3plugin_SRCS})

target_link_libraries(kdiff3plugin ${KDE4_KDECORE_LIBS} ${KDE4_KPARTS_LIBS} konq )
#${KDE4_KDECORE_LIBRARY} ${KDE4_KDEUI_LIBRARY} ${KDE4_KIO_LIBRARY} QtGui QtCore konq )

install(TARGETS kdiff3plugin DESTINATION ${PLUGIN_INSTALL_DIR} )

########## install files ###############

install( FILES kdiff3_plugin.desktop DESTINATION ${SERVICES_INSTALL_DIR} )
