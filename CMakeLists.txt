#cmake < 3.1 has no sane way of checking C++11 features and needed flags
cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

project(kdiff3)

set(CMAKE_CXX_EXTENSIONS OFF ) #don't use non-standard extentions
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(ECM_MIN_VERSION "1.7.0")
set(QT_MIN_VERSION "5.6.0")
set(KF5_MIN_VERSION "5.16.0")

find_package(ECM ${ECM_MIN_VERSION} CONFIG REQUIRED)
set(
    CMAKE_MODULE_PATH
    ${CMAKE_MODULE_PATH}
    ${ECM_MODULE_PATH}
    ${ECM_KDE_MODULE_DIR}
)

include(KDEInstallDirs)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(KDECMakeSettings NO_POLICY_SCOPE)
include(FeatureSummary)

include(ECMInstallIcons)
include(ECMAddAppIcon)
# include(GenerateExportHeader)

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
    DocTools
    IconThemes
)


set(KDiff3_LIBRARIES ${Qt5PrintSupport_LIBRARIES} KF5::I18n KF5::CoreAddons KF5::Crash KF5::IconThemes )

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # I don't need warnings about wired things in comments.
    set(CLANG_WARNING_FLAGS -Wno-comment)
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${CLANG_WARNING_FLAGS}")
    set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} ${CLANG_WARNING_FLAGS}")
endif()

set(
    needed_features
    cxx_nullptr
    cxx_override
)


add_definitions(
    -DQT_DEPRECATED_WARNINGS #Get warnings from QT about deprecated functions.
    -DQT_NO_URL_CAST_FROM_STRING #implict casting from string to url does not always behave as you might think
)
#remove unnneeded errors
remove_definitions(
    -DQT_NO_CAST_FROM_ASCII #While a good idea in gerneral this triggers for many simple non-translatable strings
    )

add_subdirectory(src)
add_subdirectory(doc)

add_subdirectory(kdiff3fileitemactionplugin)

ki18n_install(po)
kdoctools_install(po)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
