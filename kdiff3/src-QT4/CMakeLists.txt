

#add_definitions(-DQT3_SUPPORT -DQT3_SUPPORT_WARNINGS)


########### kdiff3 KPart ###############

set(kdiff3part_PART_SRCS 
   kdiff3_part.cpp 
   kdiff3.cpp 
   directorymergewindow.cpp 
   merger.cpp 
   pdiff.cpp 
   difftextwindow.cpp 
   diff.cpp 
   optiondialog.cpp 
   mergeresultwindow.cpp 
   fileaccess.cpp 
   gnudiff_analyze.cpp 
   gnudiff_io.cpp 
   gnudiff_xmalloc.cpp 
   common.cpp 
   smalldialogs.cpp 
   progress.cpp )

#kde4_add_plugin(kdiff3part WITH_PREFIX ${kdiff3part_PART_SRCS})

#set_target_properties(kdiff3part PROPERTIES DEFINE_SYMBOL KDIFF3_PART)

#target_link_libraries(kdiff3part  ${KDE4_KPARTS_LIBS} ${QT_QT3SUPPORT_LIBRARY})

#install(TARGETS kdiff3part DESTINATION ${PLUGIN_INSTALL_DIR} )

########### kdiff3 executable ###############

set(kdiff3_SRCS 
   main.cpp 
   kdiff3_shell.cpp
   ${kdiff3part_PART_SRCS}
    )

kde4_add_executable(kdiff3 ${kdiff3_SRCS})

target_link_libraries(kdiff3  ${KDE4_KPARTS_LIBS} ${QT_QT3SUPPORT_LIBRARY} )

install(TARGETS kdiff3 ${INSTALL_TARGETS_DEFAULT_ARGS})


########### install files ###############

install( FILES  kdiff3part.desktop DESTINATION ${SERVICES_INSTALL_DIR} )
install( FILES  kdiff3_part.rc DESTINATION ${DATA_INSTALL_DIR}/kdiff3part )
install( FILES  kdiff3_shell.rc DESTINATION ${DATA_INSTALL_DIR}/kdiff3 )
install( PROGRAMS  kdiff3.desktop DESTINATION ${XDG_APPS_INSTALL_DIR} )

kde4_install_icons( ${ICON_INSTALL_DIR} )


