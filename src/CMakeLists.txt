# SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
# SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
# SPDX-License-Identifier: GPL-2.0-or-later

########### kdiff3 KPart ###############


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
   DefaultFileAccessJobHandler.cpp
   gnudiff_analyze.cpp
   gnudiff_io.cpp
   gnudiff_xmalloc.cpp
   common.cpp
   smalldialogs.cpp
   progress.cpp
   ProgressProxy.cpp
   ProgressProxyExtender.cpp
   PixMapUtils.cpp
   MergeFileInfos.cpp
   Utils.cpp
   selection.cpp
   SourceData.cpp
   Overview.cpp
   Logging.cpp
   FileNameLineEdit.cpp
   MergeEditLine.cpp
   Options.cpp
   CommentParser.cpp
   CvsIgnoreList.cpp
   CompositeIgnoreList.cpp
   DirectoryInfo.cpp
   GitIgnoreList.cpp
)

ki18n_wrap_ui(kdiff3part_PART_SRCS
    scroller.ui
    opendialog.ui
    progressdialog.ui
)

add_library(kdiff3part MODULE ${kdiff3part_PART_SRCS})

set_target_properties(kdiff3part PROPERTIES DEFINE_SYMBOL KDIFF3_PART)
target_link_libraries(kdiff3part ${Qt5PrintSupport_LIBRARIES} KF5::I18n KF5::CoreAddons KF5::Parts KF5::Crash)
target_compile_definitions(kdiff3part PRIVATE -DTRANSLATION_DOMAIN=\"kdiff3\")

install(TARGETS kdiff3part DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf5/parts )

########### kdiff3 executable ###############

set(kdiff3_SRCS
   main.cpp
   kdiff3_shell.cpp
   ${kdiff3part_PART_SRCS}
    )

ecm_optional_add_subdirectory( autotests )

#cann't use add_subdirectory because it changes the scope.
include(icons/CMakeLists.txt)
add_executable(kdiff3 ${kdiff3_SRCS})

target_link_libraries(kdiff3 KF5::ConfigCore KF5::ConfigGui KF5::Parts KF5::Crash ${Qt5PrintSupport_LIBRARIES} KF5::I18n KF5::CoreAddons )

# See https://cmake.org/cmake/help/v3.15/prop_tgt/MACOSX_BUNDLE_INFO_PLIST.html
if(APPLE)
  set_property(
    TARGET kdiff3
    PROPERTY MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/MacOSXBundleInfo.plist.in
  )

  # These are substituted by CMake into plist.in.
  set(MACOSX_BUNDLE_DISPLAY_NAME "KDiff3")
  set(MACOSX_BUNDLE_GUI_IDENTIFIER "org.kde.KDiff3")
  set(MACOSX_BUNDLE_BUNDLE_NAME "KDiff3")
  set(MACOSX_BUNDLE_DISPLAY_NAME "KDiff3")
  set(MACOSX_BUNDLE_INFO_STRING "KDiff3 - Diff/Patch Frontend")
  set(MACOSX_BUNDLE_LONG_VERSION_STRING "KDiff3 ${KDIFF3_VERSION}")
  set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${KDIFF3_VERSION_MAJOR}.${KDIFF3_VERSION_MINOR}")
  set(MACOSX_BUNDLE_BUNDLE_VERSION "${KDIFF3_VERSION}")
  set(MACOSX_BUNDLE_COPYRIGHT "2003-2020 The KDiff3 Authors")
endif()

install(TARGETS kdiff3 ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})


########### install files ###############

install( FILES  kdiff3part.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR} )
install( FILES  kdiff3_part.rc DESTINATION ${KDE_INSTALL_KXMLGUI5DIR}/kdiff3part )
install( FILES  kdiff3_shell.rc DESTINATION ${KDE_INSTALL_KXMLGUI5DIR}/kdiff3 )
install( PROGRAMS org.kde.kdiff3.desktop DESTINATION ${XDG_APPS_INSTALL_DIR} )
install( FILES org.kde.kdiff3.appdata.xml DESTINATION ${KDE_INSTALL_METAINFODIR} )
