#  SPDX-FileCopyrightText: 2003-2006, Sergey Zorin. All rights reserved.
#  SPDX-FileCopyrightText:  2018-2020 Michael Reeves reeves.87@gmail.com
#  SPDX-License-Identifier: BSD-2-Clause

set(kdiff3ext_SRCS
    class_factory.cpp
    diff_ext.cpp
    server.cpp
)

set(kdiff3ext_RCS
    diff_ext_for_kdiff3.rc
)

add_library(kdiff3ext MODULE ${kdiff3ext_SRCS} ${kdiff3ext_RCS})

target_link_libraries(kdiff3ext KF${KF_MAJOR_VERSION}::I18n uuid ole32)
target_compile_definitions(kdiff3ext PRIVATE -DTRANSLATION_DOMAIN=\"kdiff3\" -DUNICODE=1 -Dkdiff3ext)
# Work around buggy windows sdk headers that prevent certain symbols from being exported the normal way
# even though they are required exports for shell exentions.
#
# This works here because only acutally need four "C" style symbols exported.
# C++ symbols are not properly supported by this mechanism
set_target_properties(kdiff3ext PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)


#If we don't do this the nsi packer won't pickup the shell extention library.
install(TARGETS kdiff3ext DESTINATION  ${KDE_INSTALL_PLUGINDIR} )
