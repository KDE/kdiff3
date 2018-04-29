kdoctools_create_handbook(index.docbook INSTALL_DESTINATION ${HTML_INSTALL_DIR}/en SUBDIR kdiff3)

if (Docbook2X_FOUND)
    install_docbook_man_page(kdiff3 1)
endif()