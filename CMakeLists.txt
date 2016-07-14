cmake_minimum_required(VERSION 2.8.12 FATAL_ERROR)
project(kdiff3)

set(ECM_MIN_VERSION "1.0.0")
set(QT_MIN_VERSION "5.3.0")
set(KF5_MIN_VERSION "5.5.0")

find_package(ECM ${ECM_MIN_VERSION} CONFIG REQUIRED)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR} ${CMAKE_SOURCE_DIR}/cmake)

include(KDEInstallDirs)
include(KDECompilerSettings)
include(KDECMakeSettings)
include(KDEFrameworkCompilerSettings)
include(FeatureSummary)

include(ECMInstallIcons)

find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Core Gui)
find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS I18n DocTools)
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules)

add_definitions(${QT_DEFINITIONS} ${KF5_DEFINITIONS})

include_directories( 
    ${QT_INCLUDES} ${KF5_INCLUDES} ${Qt5Core_INCLUDE_DIRS} ${Qt5Core_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
    )

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config-kdiff3.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kdiff3.h )

add_subdirectory(src)
#add_subdirectory(po)
add_subdirectory(doc)

find_file(KFILEITEMACTIONPLUGIN_FOUND kabstractfileitemactionplugin.h PATHS)
if(KFILEITEMACTIONPLUGIN_FOUND)
  message(STATUS "")
  message(STATUS "kabstractfileitemactionplugin.h found........... YES")
  message(STATUS "   => kdiff3fileitemactionplugin (KDiff3 contextmenu plugin for Konqueror/Dolphin, KDE>4.6) will be built.")
  message(STATUS "")
  add_subdirectory(kdiff3fileitemactionplugin)
else(KFILEITEMACTIONPLUGIN_FOUND)
  message(STATUS "")
  message(STATUS "kabstractfileitemactionplugin.h found........... NO")
  message(STATUS "   => kdiff3fileitemactionplugin (KDiff3 contextmenu plugin for Konqueror/Dolphin, KDE>4.6) will not be built.)")
  message(STATUS "Trying fallback via libkonq (works with Konqueror only) ...")
  FIND_PACKAGE(LibKonq)
  if(LIBKONQ_FOUND)
    set(HAVE_LIBKONQ 1)
    
    message(STATUS "")
    message(STATUS "libkonq module found........... YES")
    message(STATUS "   => kdiff3plugin (KDiff3 contextmenu plugin for Konqueror) will be built.")
    message(STATUS "")
    add_subdirectory(kdiff3plugin)
  else(LIBKONQ_FOUND)
    set(HAVE_LIBKONQ 0)
    
    message(STATUS "")
    message(STATUS "libkonq module found........... NO")
    message(STATUS "   => kdiff3plugin (KDiff3 contextmenu plugin for Konqueror) will not be built.)")
    message(STATUS "")
  endif(LIBKONQ_FOUND)
endif(KFILEITEMACTIONPLUGIN_FOUND)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)