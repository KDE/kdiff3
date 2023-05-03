add_definitions(-DTRANSLATION_DOMAIN=\"kdiff3fileitemactionplugin\")

find_package(KF${KF_MAJOR_VERSION} ${KF_MIN_VERSION} REQUIRED COMPONENTS KIO
	  WidgetsAddons   # KMessageBox
	)

set(kdiff3_fileitemaction_src kdiff3fileitemaction.cpp ../src/Utils.cpp)

kcoreaddons_add_plugin(kdiff3fileitemaction SOURCES ${kdiff3_fileitemaction_src} JSON kdiff3fileitemaction.json INSTALL_NAMESPACE "kf5/kfileitemaction")
target_link_libraries(kdiff3fileitemaction KF${KF_MAJOR_VERSION}::I18n KF${KF_MAJOR_VERSION}::WidgetsAddons KF${KF_MAJOR_VERSION}::KIOWidgets)
