# SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
# SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
# SPDX-License-Identifier: GPL-2.0-or-later

#cmake < 3.1 has no sane way of checking C++11 features and needed flags
#CMP0077 is defined in 3.13 or later
cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

project(kdiff3 VERSION 1.11.70)

#Avaliable as QtVersionOption starting in ECM 5.82.
option(BUILD_WITH_QT6 "Build against Qt 6" ON)

set(CMAKE_CXX_OUTPUT_EXTENSION_REPLACE ON)
set(CMAKE_CXX_EXTENSIONS ON ) #disable trigraphs in clang/gcc -- officially removed in c++17
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CXX_STANDARD_REQUIRED ON)#Force early fail if we cann't get c++17.

set(KF_MIN_VERSION "5.100.0")
if (BUILD_WITH_QT6)
    set(QT_MAJOR_VERSION "6")
    set(QT_MIN_VERSION "6.6.0")
else()
    set(QT_MAJOR_VERSION "5")
    set(QT_MIN_VERSION "5.15.8")
endif()

if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.2)
    message(WARNING "gcc < 9.2 is not recomended support is best effort-only.")
endif()

find_package(ECM ${KF_MIN_VERSION} CONFIG REQUIRED)
set(
    CMAKE_MODULE_PATH
    ${CMAKE_MODULE_PATH}
    ${ECM_MODULE_PATH}
)

if(BUILD_WITH_QT6)
    include(QtVersionOption)
    set(KF_MAJOR_VERSION "6")
else()
    set(KF_MAJOR_VERSION "5")
endif()

# project uses settings default as of KDECompilerSettings in ECM 5.85.0
set(KDE_QT_MODERNCODE_DEFINITIONS_LEVEL 5.85.0)
set(KDE_COMPILERSETTINGS_LEVEL 5.85.0)
# pedantic compilers are not supported at this time.
set(KDE_SKIP_PEDANTIC_WARNINGS_SETTINGS TRUE)

include(KDEInstallDirs)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(KDECMakeSettings NO_POLICY_SCOPE)
include(FeatureSummary)

#Full QT_NO_CAST_FROM_ASCII is not compatiable with kdiff3 code and not worth inforcing
#we use the slightly less strict QT_RESTRICTED_CAST_FROM_ASCII
#Disallowing implict cast from QByteArray doesn't make sense for this project
remove_definitions(-DQT_NO_CAST_FROM_ASCII -DQT_NO_CAST_FROM_BYTEARRAY)
#Causes multiple build errors related to the use QLatin1String
remove_definitions(-DQT_USE_QSTRINGBUILDER)

include(ECMInstallIcons)
include(ECMAddAppIcon)
include(ECMSetupVersion)
include(ECMAddTests)
include(ECMOptionalAddSubdirectory)
include(ECMSetupQtPluginMacroNames)

ecm_setup_version(${PROJECT_VERSION} VARIABLE_PREFIX KDIFF3 VERSION_HEADER ${CMAKE_BINARY_DIR}/src/version.h)

# Some older versions on boost contain a bug that prevents compiling gcc offers a built-in workaround
# but that isn't enough to ship as clang has no such workaround. 1.65 is known to be affected.
#
# 1.71 or later is required for safe_numerics to work on MSVC otherwise we get a link-time error.
find_package(Boost 1.71 REQUIRED)

#needed on craft and possiablely other custom setups.
include_directories(${Boost_INCLUDE_DIRS})

find_package(
    Qt${QT_MAJOR_VERSION} ${QT_MIN_VERSION}
    CONFIG
    REQUIRED
    COMPONENTS
    Core
    Gui
    Widgets
    PrintSupport
)

if (QT_MAJOR_VERSION STREQUAL "6")
    find_package(Qt6Core5Compat)
    set(COMPATLIB Qt6::Core5Compat)
endif()

find_package(
    KF${KF_MAJOR_VERSION} ${KF_MIN_VERSION}
    REQUIRED
    COMPONENTS
    I18n
    CoreAddons
    XmlGui
    WidgetsAddons
    Config
    Crash
    KIO #Not fully ready for to build without KIO
    OPTIONAL_COMPONENTS
    DocTools
)

set_package_properties(KF${KF_MAJOR_VERSION}DocTools PROPERTIES PURPOSE "Allows generating and installing docs.")

string(REPLACE "." ";" VERSION_LIST "${KF${KF_MAJOR_VERSION}_VERSION}")
list(GET VERSION_LIST 0 KF_VERSION_MAJOR)
list(GET VERSION_LIST 1 KF_VERSION_MINOR)
list(GET VERSION_LIST 2 KF_VERSION_PATCH)

option(ENABLE_AUTO "Enable kdiff3's '--auto' flag" ON)
option(ENABLE_CLANG_TIDY "Run clang-tidy if available and cmake version >=3.6" OFF)
option(ENABLE_GDBINDEX OFF)

set(CMAKE_CXX_STANDARD 17)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    if(ENABLE_CLAZY)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Xclang -plugin-arg-clazy -Xclang level0,level1,no-overloaded-signal,auto-unexpected-qstringbuilder,unused-non-trivial-variable,returning-void-expression,isempty-vs-count,container-inside-loop,assert-with-side-effects")
    endif()
    #Adjust clang specific  warnings
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wshadow -fexceptions")
    set(CLANG_WARNING_FLAGS "-Wno-undef -Wno-trigraphs -Wno-invalid-pp-token -Wno-comment -Wshorten-64-to-32 -Wstring-conversion -Wc++11-narrowing -fstack-protector-all")
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${CLANG_WARNING_FLAGS}")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    #Make MSVC obey standard C++ exception handling by destroying all objects going out of scope as a result of the exception.
    #This also separates a Microsoft specific exention SEH which catches non-c++ asynchronous exceptions.
    add_compile_options("/EHsc")
    #Suppress MSVCs min/max macros
    add_definitions(-DNOMINMAX)
    #Suppress Microsoft specfic C4996 "deprecatation" warnings. Not helpful for code intended to run on any other platform.
    add_definitions(-D_CRT_NONSTDC_NO_WARNINGS -D_CRT_SECURE_NO_WARNINGS -D_CRT_SECURE_NO_WARNINGS_GLOBALS)
    #force compliant use of __cplusplus
    add_compile_options("/Zc:__cplusplus")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-trigraphs -Wduplicated-cond -Wduplicated-branches -Wshadow -fexceptions")
endif()

#new in cmake 3.6+ integrate clang-tidy
if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES "clang-tidy" DOC "Path to clang-tidy executable")
    if(NOT CLANG_TIDY_EXE)
        message(STATUS "clang-tidy not found disabling integration.")
    else()
        message(STATUS "Found clang-tidy: ${CLANG_TIDY_EXE}")
        set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}" "-header-filter=.*")
    endif()
endif()

set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} ${CMAKE_CXX_FLAGS}")

if(ENABLE_AUTO)
    add_definitions(
        -DENABLE_AUTO
    )
endif()

add_definitions(
    -DQT_DEPRECATED_WARNINGS #Get warnings from QT about deprecated functions.
    -DQT_NO_URL_CAST_FROM_STRING # casting from string to url does not always behave as you might think
    -DQT_RESTRICTED_CAST_FROM_ASCII #casting from char*/QByteArray to QString can produce unexpected results for non-latin characters.
    -DQT_NO_KEYWORDS
    -DQT_NO_CAST_TO_ASCII
    -DQT_NO_PROCESS_COMBINED_ARGUMENT_START
    -DQT_DEPRECATED_WARNINGS_SINCE=0x050F00#warn if deprecated at 5.15 or before. Qt default setting is not useful for kdiff3.
    -DQT_DISABLE_DEPRECATED_BEFORE=0x050F00#disable deprecated api for Qt<5.15.
    #Boost 1.74 and possibly others trigger these warnings in boosts own internal headers making
    #the warning useless.
    -DBOOST_ALLOW_DEPRECATED_HEADERS
    #KF5 5.64+ flags
    #Don't warn for API depreciated after 5.110.0 (below as hex.hex.hex number)
    -DKF_DEPRECATED_WARNINGS_SINCE=0x056800
    -DKIOCORE_DEPRECATED_WARNINGS_SINCE=0x056E00
    -DKXMLGUI_DEPRECATED_WARNINGS_SINCE=0x056E00
    -DKF_VERSION_MAJOR=${KF_VERSION_MAJOR}
    -DKF_VERSION_MINOR=${KF_VERSION_MINOR}
    -DKF_VERSION_PATCH=${KF_VERSION_PATCH}
)

#The CMake boolean value is not properly translated if we assign the variable directly to a C/C++ define. clangd will catch this in sytax hightlighting other language servers may not.
if(KF${KF_MAJOR_VERSION}KIO_FOUND)
    add_definitions(
        -DHAS_KFKIO=1
    )
endif()

add_subdirectory(src)
if(KF${KF_MAJOR_VERSION}DocTools_FOUND)
    ecm_optional_add_subdirectory(doc)
    kdoctools_install(po)
else()
    message(WARNING "DocTools not found. Docs will not be generated.")
endif()

ki18n_install(po)

add_subdirectory(kdiff3fileitemactionplugin)

if(WIN32)
add_subdirectory(diff_ext_for_kdiff3)
endif()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
