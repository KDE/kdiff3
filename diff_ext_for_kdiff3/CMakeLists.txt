
set(kdiff3ext_SRCS
    class_factory.cpp
    diff_ext.cpp
    server.cpp
)

set(kdiff3ext_RCS
    diff_ext_for_kdiff3.rc
)

add_library(kdiff3ext MODULE ${kdiff3ext_SRCS} ${kdiff3ext_RCS})

target_link_libraries(kdiff3ext KF5::I18n uuid ole32)
target_compile_definitions(kdiff3ext PRIVATE -DTRANSLATION_DOMAIN=\"kdiff3\" -DUNICODE=1)