add_subdirectory( tests )

set( maildispatcheragent_SRCS
  maildispatcheragent.cpp
  outboxqueue.cpp
  sendjob.cpp
  sentactionhandler.cpp
  storeresultjob.cpp
  #configdialog.cpp
)

kde4_add_ui_files(maildispatcheragent_SRCS settings.ui)
kde4_add_kcfg_files(maildispatcheragent_SRCS settings.kcfgc)
kcfg_generate_dbus_interface(${CMAKE_CURRENT_SOURCE_DIR}/maildispatcheragent.kcfg org.kde.Akonadi.MailDispatcher.Settings)
qt4_add_dbus_adaptor(maildispatcheragent_SRCS
  ${CMAKE_CURRENT_BINARY_DIR}/org.kde.Akonadi.MailDispatcher.Settings.xml settings.h Settings
)
qt4_add_dbus_adaptor( maildispatcheragent_SRCS
  org.freedesktop.Akonadi.MailDispatcherAgent.xml maildispatcheragent.h MailDispatcherAgent
)

if (RUNTIME_PLUGINS_STATIC)
  add_definitions(-DMAIL_SERIALIZER_PLUGIN_STATIC)
endif ()

kdepim_add_agent(akonadi_maildispatcher_agent ${maildispatcheragent_SRCS})

if (RUNTIME_PLUGINS_STATIC)
  target_link_libraries(akonadi_maildispatcher_agent akonadi_serializer_mail)
endif ()

if (Q_WS_MAC)
  set_target_properties(akonadi_maildispatcher_agent PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/../Info.plist.template)
  set_target_properties(akonadi_maildispatcher_agent PROPERTIES MACOSX_BUNDLE_GUI_IDENTIFIER "org.kde.Akonadi.maildispatcher")
  set_target_properties(akonadi_maildispatcher_agent PROPERTIES MACOSX_BUNDLE_BUNDLE_NAME "KDE Akonadi Maildispatcher")
endif ()

target_link_libraries(akonadi_maildispatcher_agent ${KDEPIMLIBS_AKONADI_LIBS} ${KDEPIMLIBS_AKONADI_KMIME_LIBS} ${QT_QTCORE_LIBRARY} ${QT_QTDBUS_LIBRARY} ${KDE4_KDECORE_LIBS} ${KDE4_KIO_LIBS} ${KDEPIMLIBS_KMIME_LIBS} ${KDEPIMLIBS_MAILTRANSPORT_LIBS} ${KDE4_KNOTIFYCONFIG_LIBS})

install( FILES maildispatcheragent.desktop DESTINATION "${CMAKE_INSTALL_PREFIX}/share/akonadi/agents" )
install( FILES akonadi_maildispatcher_agent.notifyrc DESTINATION "${DATA_INSTALL_DIR}/akonadi_maildispatcher_agent" )
