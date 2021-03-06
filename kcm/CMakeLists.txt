add_definitions( -DQT_NO_CAST_FROM_ASCII )
add_definitions( -DQT_NO_CAST_TO_ASCII )

include_directories( ${kdepim-runtime_SOURCE_DIR} )

set(akonadi_resources_kcm_srcs configmodule.cpp resourcesmanagementwidget.cpp)
kde4_add_ui_files(akonadi_resources_kcm_srcs resourcesmanagementwidget.ui )
kde4_add_plugin(kcm_akonadi_resources ${akonadi_resources_kcm_srcs})
target_link_libraries(kcm_akonadi_resources  ${KDE4_KDEUI_LIBS} ${KDEPIMLIBS_AKONADI_LIBS})


set(akonadi_server_kcm_srcs serverconfigmodule.cpp)
kde4_add_ui_files(akonadi_server_kcm_srcs serverconfigmodule.ui
                                          servermysqlstorage.ui
                                          serverpsqlstorage.ui
                                          serverstoragedriver.ui)
kde4_add_plugin(kcm_akonadi_server ${akonadi_server_kcm_srcs})
target_link_libraries(kcm_akonadi_server  ${KDE4_KIO_LIBS} ${KDEPIMLIBS_AKONADI_LIBS} ${AKONADI_COMMON_LIBRARIES})


set(akonadi_kcm_srcs akonadiconfigmodule.cpp)
kde4_add_plugin(kcm_akonadi ${akonadi_kcm_srcs})
target_link_libraries(kcm_akonadi  ${KDE4_KCMUTILS_LIBS})


install(TARGETS
  kcm_akonadi_resources
  kcm_akonadi_server
  kcm_akonadi
  DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES
  kcm_akonadi_resources.desktop
  kcm_akonadi_server.desktop
  kcm_akonadi.desktop
  DESTINATION ${SERVICES_INSTALL_DIR})
