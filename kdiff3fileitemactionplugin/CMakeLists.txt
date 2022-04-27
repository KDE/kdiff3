add_definitions(-DTRANSLATION_DOMAIN=\"kdiff3fileitemactionplugin\")

find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS KIO
	  WidgetsAddons   # KMessageBox
	)

set(kdiff3_fileitemaction_src kdiff3fileitemaction.cpp ../src/Utils.cpp)

kcoreaddons_add_plugin(kdiff3fileitemaction SOURCES ${kdiff3_fileitemaction_src} JSON kdiff3fileitemaction.json INSTALL_NAMESPACE "kf5/kfileitemaction")
target_link_libraries(kdiff3fileitemaction KF5::I18n KF5::WidgetsAddons KF5::KIOWidgets)
