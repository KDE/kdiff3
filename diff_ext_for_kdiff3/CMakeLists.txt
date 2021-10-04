#  SPDX-FileCopyrightText: 2003-2006, Sergey Zorin. All rights reserved.
#  SPDX-FileCopyrightText:  2018-2020 Michael Reeves reeves.87@gmail.com
#  SPDX-License-Identifier: BSD-2-Clause

# Work around buggy windows sdk headers that prevent certain symbols from being exported the normal way
# even though they are required exports for shell exentions.
if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
set(kdiff3extdefs diff_ext_for_kdiff3_msvc.def)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
set(kdiff3extdefs diff_ext_for_kdiff3.def)
endif()

set(kdiff3ext_SRCS
    class_factory.cpp
    diff_ext.cpp
    server.cpp
    ${kdiff3extdefs}
)

set_source_files_properties(${kdiff3extdefs} PROPERTIES HEADER_FILE_ONLY TRUE)

set(kdiff3ext_RCS
    diff_ext_for_kdiff3.rc
)

add_library(kdiff3ext MODULE ${kdiff3ext_SRCS} ${kdiff3ext_RCS})

target_link_libraries(kdiff3ext KF5::I18n uuid ole32)
target_compile_definitions(kdiff3ext PRIVATE -DTRANSLATION_DOMAIN=\"kdiff3\" -DUNICODE=1 -Dkdiff3ext)

#If we don't do this the nsi packer won't pickup the shell extention library.
install(TARGETS kdiff3ext DESTINATION  ${KDE_INSTALL_PLUGINDIR} )
