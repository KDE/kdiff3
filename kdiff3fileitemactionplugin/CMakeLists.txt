find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS KIO
	  WidgetsAddons   # KMessageBox
	)

add_library(kdiff3fileitemaction MODULE kdiff3fileitemaction.cpp)
target_link_libraries(kdiff3fileitemaction KF5::WidgetsAddons KF5::KIOWidgets ${LibKDiff3_LIBRARIES})

install(TARGETS kdiff3fileitemaction DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES kdiff3fileitemaction.desktop DESTINATION ${SERVICES_INSTALL_DIR})
