kde4_add_plugin(kdiff3fileitemaction kdiff3fileitemaction.cpp)
target_link_libraries(kdiff3fileitemaction ${KDE4_KIO_LIBS} ${LibKDiff3_LIBRARIES})
install(TARGETS kdiff3fileitemaction DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES kdiff3fileitemaction.desktop DESTINATION ${SERVICES_INSTALL_DIR})
