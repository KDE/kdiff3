

########### kdiff3 KPart ###############
find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS Parts WidgetsAddons)

set(kdiff3part_PART_SRCS
   kdiff3_part.cpp
   kdiff3.cpp
   directorymergewindow.cpp
   merger.cpp
   pdiff.cpp
   difftextwindow.cpp
   diff.cpp
   optiondialog.cpp
   mergeresultwindow.cpp
   fileaccess.cpp
   gnudiff_analyze.cpp
   gnudiff_io.cpp
   gnudiff_xmalloc.cpp
   common.cpp
   smalldialogs.cpp
   progress.cpp
   ProgressProxyExtender.cpp
   PixMapUtils.cpp
   MergeFileInfos.cpp
   Utils.cpp
   selection.cpp
   cvsignorelist.cpp
   SourceData.cpp
   Overview.cpp
   Logging.cpp )

add_library(kdiff3part MODULE ${kdiff3part_PART_SRCS})

set_target_properties(kdiff3part PROPERTIES DEFINE_SYMBOL KDIFF3_PART)
target_compile_features(kdiff3part PRIVATE ${needed_features})
target_link_libraries(kdiff3part ${KDiff3_LIBRARIES} KF5::Parts)
target_compile_definitions(kdiff3part PRIVATE -DTRANSLATION_DOMAIN=\"kdiff3\")

install(TARGETS kdiff3part DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf5/parts )

########### kdiff3 executable ###############
find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS Parts WidgetsAddons Config)

set(kdiff3_SRCS
   main.cpp
   kdiff3_shell.cpp
   ${kdiff3part_PART_SRCS}
    )

add_executable(kdiff3 ${kdiff3_SRCS})

target_link_libraries(kdiff3 KF5::ConfigCore KF5::ConfigGui KF5::Parts ${KDiff3_LIBRARIES} )
target_compile_features(kdiff3 PRIVATE ${needed_features})

install(TARGETS kdiff3 ${INSTALL_TARGETS_DEFAULT_ARGS})


########### install files ###############

install( FILES  kdiff3part.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR} )
install( FILES  kdiff3_part.rc DESTINATION ${KDE_INSTALL_KXMLGUI5DIR}/kdiff3part )
install( FILES  kdiff3_shell.rc DESTINATION ${KDE_INSTALL_KXMLGUI5DIR}/kdiff3 )
#install( PROGRAMS  kdiff3.desktop DESTINATION ${XDG_APPS_INSTALL_DIR} )
install( PROGRAMS org.kde.kdiff3.desktop DESTINATION ${XDG_APPS_INSTALL_DIR} )
install( FILES org.kde.kdiff3.appdata.xml DESTINATION ${KDE_INSTALL_METAINFODIR} )

add_subdirectory(icons)
