cmake_minimum_required(VERSION 2.6)
project(kdiff3)

find_package(KDE4 REQUIRED)
include (MacroLibrary)
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules)

add_definitions(${QT_DEFINITIONS} ${KDE4_DEFINITIONS})

include_directories( 
    ${KDE4_INCLUDE_DIR} 
    ${QT_INCLUDES}  
    ${CMAKE_CURRENT_BINARY_DIR}
    )

link_directories(
    ${KDE4_LIB_DIR}
    )

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config-kdiff3.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kdiff3.h )

add_subdirectory(src-QT4)
add_subdirectory(po)
add_subdirectory(doc)


find_file(KFILEITEMACTIONPLUGIN_FOUND kfileitemactionplugin.h PATHS ${KDE4_INCLUDE_DIR} NO_DEFAULT_PATH)
if(KFILEITEMACTIONPLUGIN_FOUND)
  message(STATUS "")
  message(STATUS "kfileitemactionplugin.h found........... YES")
  message(STATUS "   => kdiff3fileitemactionplugin (KDiff3 contextmenu plugin for Konqueror/Dolphin, KDE>4.6) will be built.")
  message(STATUS "")
  add_subdirectory(kdiff3fileitemactionplugin)
else(KFILEITEMACTIONPLUGIN_FOUND)
  message(STATUS "")
  message(STATUS "kfileitemactionplugin.h found........... NO")
  message(STATUS "   => kdiff3fileitemactionplugin (KDiff3 contextmenu plugin for Konqueror/Dolphin, KDE>4.6) will not be built.)")
  message(STATUS "Trying fallback via libkonq (works with Konqueror only) ...")
  MACRO_OPTIONAL_FIND_PACKAGE(LibKonq)
  MACRO_BOOL_TO_01(LIBKONQ_FOUND HAVE_LIBKONQ)
  if(LIBKONQ_FOUND)
    message(STATUS "")
    message(STATUS "libkonq module found........... YES")
    message(STATUS "   => kdiff3plugin (KDiff3 contextmenu plugin for Konqueror) will be built.")
    message(STATUS "")
    add_subdirectory(kdiff3plugin-QT4)
  else(LIBKONQ_FOUND)
    message(STATUS "")
    message(STATUS "libkonq module found........... NO")
    message(STATUS "   => kdiff3plugin (KDiff3 contextmenu plugin for Konqueror) will not be built.)")
    message(STATUS "")
  endif(LIBKONQ_FOUND)
endif(KFILEITEMACTIONPLUGIN_FOUND)
