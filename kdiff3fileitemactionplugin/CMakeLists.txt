find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS KIO
	  WidgetsAddons   # KMessageBox
	)

kcoreaddons_add_plugin(kdiff3fileitemaction SOURCES kdiff3fileitemaction.cpp INSTALL_NAMESPACE "kf5/kfileitemaction")
target_link_libraries(kdiff3fileitemaction ${KDiff3_LIBRARIES} KF5::WidgetsAddons KF5::KIOWidgets)

install(TARGETS kdiff3fileitemaction DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES kdiff3fileitemaction.desktop DESTINATION ${SERVICES_INSTALL_DIR})
