# Attempt to find docbook-to-man binary from docbook2x package
#
# This module defines:
# - DOCBOOK_TO_MAN_EXECUTABLE, path to docbook2x-man binary
#
# Note that the binary docbook-to-man in debian systems is a different application
# than in other distributions.
#
# Debian systems
# * docbook-to-man comes from the package docbook-to-man
# * docbook2man comes from the package docbook-utils
# * docbook2x-man comes from the package docbook2x
# Suse
# * docbook-to-man comes from the package docbook2x
# * docbook2man comes from the package docbook-utils-minimal
# ArchLinux
# * docbook-to-man comes from the package docbook-to-man
# * docbook2man comes from the package docbook2x
#
# We actually want the binary from docbook2x, which supports XML

#=============================================================================
# Copyright 2013 Kevin Funk <kfunk@kde.org>
# Copyright 2015 Alex Merry <alexmerry@kde.org>
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================

macro(_check_docbook2x_executable)
    if (DOCBOOK_TO_MAN_EXECUTABLE)
        execute_process(
            COMMAND ${DOCBOOK_TO_MAN_EXECUTABLE} --version
            OUTPUT_VARIABLE _output
            ERROR_QUIET
        )
        if("${_output}" MATCHES "docbook2X ([0-9]+\\.[0-9]+\\.[0-9]+)")
            set(DOCBOOK_TO_MAN_EXECUTABLE ${_docbook_to_man_executable})
            set(Docbook2X_VERSION ${CMAKE_MATCH_1})
        else()
            unset(DOCBOOK_TO_MAN_EXECUTABLE)
            unset(DOCBOOK_TO_MAN_EXECUTABLE CACHE)
        endif()
    endif()
endmacro()

if (DOCBOOK_TO_MAN_EXECUTABLE)
    _check_docbook2x_executable()
else()
    foreach(test_exec docbook2x-man docbook-to-man db2x_docbook2man docbook2man)
        find_program(DOCBOOK_TO_MAN_EXECUTABLE
            NAMES ${test_exec}
        )
        _check_docbook2x_executable()
        if (DOCBOOK_TO_MAN_EXECUTABLE)
            break()
        endif()
    endforeach()
endif()

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(Docbook2X
    FOUND_VAR Docbook2X_FOUND
    REQUIRED_VARS DOCBOOK_TO_MAN_EXECUTABLE
    VERSION_VAR Docbook2X_VERSION
)

if (Docbook2X_FOUND)
    macro(install_docbook_man_page name section)
        set(inputfn "man-${name}.${section}.docbook")
        set(input "${CMAKE_CURRENT_SOURCE_DIR}/${inputfn}")
        set(outputfn "${name}.${section}")
        set(output "${CMAKE_CURRENT_BINARY_DIR}/${outputfn}")
        set(target "manpage-${outputfn}")

        add_custom_command(
            OUTPUT ${output}
            COMMAND ${DOCBOOK_TO_MAN_EXECUTABLE} --encoding "UTF-8" ${input}
            DEPENDS ${input}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        )
        add_custom_target(${target} ALL
            DEPENDS "${output}"
        )
        install(
            FILES ${output}
            DESTINATION ${CMAKE_INSTALL_MANDIR}/man${section}
        )
    endmacro()
endif()
