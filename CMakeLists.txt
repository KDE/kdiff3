# SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
# SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
# SPDX-License-Identifier: GPL-2.0-or-later

#cmake < 3.1 has no sane way of checking C++11 features and needed flags
#need at least 3.8 for cxx_std_11.
cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

project(kdiff3)

set(CMAKE_CXX_EXTENSIONS ON ) #disable trigraphs in clang/gcc -- officially removed in c++17
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(ECM_MIN_VERSION "5.10.0")
set(QT_MIN_VERSION "5.8.0")
#KF5 5.27 fixes a crash bug triggered on exit.
set(KF5_MIN_VERSION "5.37.0")

find_package(ECM ${ECM_MIN_VERSION} CONFIG REQUIRED)
set(
    CMAKE_MODULE_PATH
    ${CMAKE_MODULE_PATH}
    ${ECM_MODULE_PATH}
)

include(KDEInstallDirs)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(KDECMakeSettings NO_POLICY_SCOPE)
include(FeatureSummary)

include(ECMInstallIcons)
include(ECMAddAppIcon)
include(ECMSetupVersion)
include(ECMAddTests)

ecm_setup_version(1.9.5 VARIABLE_PREFIX KDIFF3 VERSION_HEADER ${CMAKE_BINARY_DIR}/src/version.h)

# Some older versions on boost contain a bug that prevents compiling gcc offers a built-in workaround
# but that isn't enough to ship as clang has no such workaround. 1.65 is known to be affected.
find_package(Boost 1.66 REQUIRED)

#needed on craft and possiablely other custom setups.
include_directories(${Boost_INCLUDE_DIRS})

find_package(
    Qt5 ${QT_MIN_VERSION}
    CONFIG
    REQUIRED
    COMPONENTS
    Core
    Gui
    Widgets
    PrintSupport
)

find_package(
    KF5 ${KF5_MIN_VERSION}
    REQUIRED
    COMPONENTS
    I18n
    CoreAddons
    Crash
    OPTIONAL_COMPONENTS
    DocTools
)

set_package_properties(KF5DocTools PROPERTIES PURPOSE "Allows generating and installing docs.")

option(ENABLE_AUTO "Enable kdiff3's '--auto' flag" ON)
option(ENABLE_CLANG_TIDY "Run clang-tidy if available and cmake version >=3.6" OFF)

set(KDiff3_LIBRARIES ${Qt5PrintSupport_LIBRARIES} KF5::I18n KF5::CoreAddons )

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    #Adjust clang specific  warnings
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wshadow")
    set(CLANG_WARNING_FLAGS "-Wno-undef -Wno-trigraphs -Wno-invalid-pp-token -Wno-comment -Wshorten-64-to-32 -Wstring-conversion -Wc++11-narrowing -fstack-protector-all")
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${CLANG_WARNING_FLAGS}")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    add_compile_options("/EHs")
    add_definitions(-DNOMINMAX) #Suppress MSVCs min/max macros
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.0)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-check")
    endif()
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 7.0)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-trigraphs -Wduplicated-cond -Wduplicated-branches -Wshadow")
    endif()
endif()

#new in cmake 3.6+ integrate clang-tidy
if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES "clang-tidy" "clang-tidy-7" "clang-tidy-6.0" "clang-tidy-6" DOC "Path to clang-tidy executable")
    if(NOT CLANG_TIDY_EXE)
        message(STATUS "clang-tidy not found disabling integration.")
    else()
        message(STATUS "Found clang-tidy: ${CLANG_TIDY_EXE}")
        set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}" "-header-filter=.*")
    endif()
endif()

set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} ${CMAKE_CXX_FLAGS}")

set(
    needed_features
    cxx_std_11
)

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
    -DQT_DISABLE_DEPRECATED_BEFORE=0x050800#disable deprecated api for Qt<5.8.
    #Boost 1.74 and possibly others trigger these warnings in boosts own internal headers making
    #the warning useless.
    -DBOOST_ALLOW_DEPRECATED_HEADERS
    #KF5 5.64+ flags
    #Don't warn for API depreciated after 5.36.0 (below as hex.hex.hex number)
    -DKF_DEPRECATED_WARNINGS_SINCE=0x052400
    -DKIOCORE_DEPRECATED_WARNINGS_SINCE=0x052400
)

add_subdirectory(src)
if(KF5DocTools_FOUND)
    add_subdirectory(doc)
    kdoctools_install(po)
else()
    message(WARNING "DocTools not found.")
endif()

ki18n_install(po)

add_subdirectory(kdiff3fileitemactionplugin)

if(WIN32)
add_subdirectory(diff_ext_for_kdiff3)
endif()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
