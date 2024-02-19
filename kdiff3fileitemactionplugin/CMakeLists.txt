add_definitions(-DTRANSLATION_DOMAIN=\"kdiff3fileitemactionplugin\")

find_package(KF${KF_MAJOR_VERSION} ${KF_MIN_VERSION} REQUIRED COMPONENTS KIO
	  WidgetsAddons   # KMessageBox
	)

set(kdiff3_fileitemaction_src kdiff3fileitemaction.cpp ../src/Utils.cpp)

ecm_setup_qtplugin_macro_names(
    JSON_NONE
        "K_PLUGIN_FACTORY"
        "K_PLUGIN_CLASS"
    JSON_ARG2
        "K_PLUGIN_FACTORY_WITH_JSON"
        "K_PLUGIN_CLASS_WITH_JSON"
)

kcoreaddons_add_plugin(kdiff3fileitemaction SOURCES ${kdiff3_fileitemaction_src} INSTALL_NAMESPACE "kf${KF_MAJOR_VERSION}/kfileitemaction")
target_link_libraries(kdiff3fileitemaction KF${KF_MAJOR_VERSION}::I18n KF${KF_MAJOR_VERSION}::WidgetsAddons KF${KF_MAJOR_VERSION}::KIOWidgets)
