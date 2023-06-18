# SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
# SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
# SPDX-License-Identifier: GPL-2.0-or-later

########### kdiff3 executable ###############

set(kdiff3_SRCS
   main.cpp
   kdiff3_shell.cpp
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

ki18n_wrap_ui(kdiff3_SRCS
    scroller.ui
    opendialog.ui
    progressdialog.ui
    FontChooser.ui
)

ecm_optional_add_subdirectory( autotests )

#cann't use add_subdirectory because it changes the scope.
include(icons/CMakeLists.txt)
add_executable(kdiff3 ${kdiff3_SRCS})


target_link_libraries(kdiff3 Qt::PrintSupport ${COMPATLIB} KF${KF_MAJOR_VERSION}::ConfigCore KF${KF_MAJOR_VERSION}::ConfigGui KF${KF_MAJOR_VERSION}::XmlGui KF${KF_MAJOR_VERSION}::KIOWidgets KF${KF_MAJOR_VERSION}::Crash KF${KF_MAJOR_VERSION}::I18n KF${KF_MAJOR_VERSION}::CoreAddons )

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
  set(MACOSX_BUNDLE_COPYRIGHT "2003- The KDiff3 Authors")
endif()

install(TARGETS kdiff3 ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})


########### install files ###############

install( FILES  kdiff3_shell.rc DESTINATION ${KDE_INSTALL_KXMLGUIDIR}/kdiff3 )
install( PROGRAMS org.kde.kdiff3.desktop DESTINATION ${KDE_INSTALL_APPDIR} )
install( FILES org.kde.kdiff3.appdata.xml DESTINATION ${KDE_INSTALL_METAINFODIR} )
