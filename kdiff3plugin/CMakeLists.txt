########### kdiff3 plugin ###############
find_package(LibKonq REQUIRED)
find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS KDELibs4Support Parts)

set(kdiff3plugin_SRCS 
   kdiff3plugin.cpp ) 

add_library(kdiff3plugin MODULE ${kdiff3plugin_SRCS})

target_link_libraries(kdiff3plugin KF5::KDELibs4Support KF5::Parts konq )

install(TARGETS kdiff3plugin DESTINATION ${PLUGIN_INSTALL_DIR} )

########## install files ###############

install( FILES kdiff3_plugin.desktop DESTINATION ${SERVICES_INSTALL_DIR} )
