#cmake < 3.1 has no sane way of checking C++11 features and needed flags
cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

project(kdiff3)

set(CMAKE_CXX_EXTENSIONS OFF ) #don't use non-standard extentions
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(ECM_MIN_VERSION "1.0.0")
set(QT_MIN_VERSION "5.6.0")
set(KF5_MIN_VERSION "5.8.0")

find_package(ECM ${ECM_MIN_VERSION} CONFIG REQUIRED)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR} ${CMAKE_SOURCE_DIR}/cmake)

include(KDEInstallDirs)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(KDECMakeSettings NO_POLICY_SCOPE)
include(FeatureSummary)

include(ECMInstallIcons)

find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Core Gui Widgets PrintSupport)
find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS I18n CoreAddons IconThemes)
find_package(KF5DocTools ${KF5_MIN_VERSION} REQUIRED )

# I don't need warnings about wired things in comments.
SET(CLANG_WARNING_FLAGS -Wno-comment)
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules)
SET(KDiff3_LIBRARIES ${Qt5PrintSupport_LIBRARIES} KF5::I18n KF5::CoreAddons KF5::IconThemes)
SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${CLANG_WARNING_FLAGS}")
SET(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} ${CLANG_WARNING_FLAGS}")
set(needed_features
    cxx_generalized_initializers
    cxx_constexpr
    cxx_auto_type)
#make sure we get warnings about using deprecated functions in qt.
add_definitions(-DQT_DEPRECATED_WARNINGS)
#remove unnneeded errors
remove_definitions(-DQT_NO_CAST_FROM_ASCII -DQT_STRICT_ITERATORS -DQT_NO_KEYWORDS)

include_directories( 
    ${Qt5Core_INCLUDE_DIRS} ${Qt5Widgets_INCLUDE_DIRS} ${Qt5Gui_INCLUDE_DIRS}
    ${Qt5PrintSupport_INCLUDE_DIRS}
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
  message(STATUS "No automatic fallback provided current configuration is not supported for plugin...")
endif(KFILEITEMACTIONPLUGIN_FOUND)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
