set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${KDE4_ENABLE_EXCEPTIONS}")

set(contactsresource_SRCS
  contactsresource.cpp
  settings.cpp
  settingsdialog.cpp
  ../common/googlesettings.cpp
  ../common/googleresource.cpp
  ../common/googleaccountmanager.cpp
  ../common/googlesettingsdialog.cpp
  ${accounts_SRCS}
)

kde4_add_kcfg_files(contactsresource_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/settingsbase.kcfgc)

kcfg_generate_dbus_interface(
  ${CMAKE_CURRENT_SOURCE_DIR}/settingsbase.kcfg
  org.kde.Akonadi.GoogleContacts.Settings
)

qt4_add_dbus_adaptor(contactsresource_SRCS
  ${CMAKE_CURRENT_BINARY_DIR}/org.kde.Akonadi.GoogleContacts.Settings.xml
  ${CMAKE_CURRENT_SOURCE_DIR}/settings.h Settings
)

kde4_add_executable(akonadi_googlecontacts_resource RUN_UNINSTALLED ${contactsresource_SRCS})

if(Q_WS_MAC)
  set_target_properties(akonadi_googlecontacts_resource PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/../../Info.plist.template
  )
  set_target_properties(akonadi_googlecontacts_resource PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.kde.Akonadi.googlecontacts"
  )
  set_target_properties(akonadi_googlecontacts_resource PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "KDE Akonadi Google Contacts Resource"
  )
endif()


target_link_libraries(akonadi_googlecontacts_resource
  ${KDEPIMLIBS_AKONADI_LIBS}
  ${KDEPIMLIBS_KABC_LIBS}
  ${QT_QTCORE_LIBRARY}
  ${QT_QTDBUS_LIBRARY}
  ${QT_QTNETWORK_LIBRARY}
  ${KDE4_KDECORE_LIBS}
  ${KDE4_KIO_LIBS}
  ${LibKGAPI2_LIBRARY}
)

if(${AccountsQt_FOUND} AND ${SignOnQt_FOUND})
  target_link_libraries(akonadi_googlecontacts_resource
    ${ACCOUNTSQT_LIBRARIES}
    ${SIGNONQT_LIBRARIES})
endif()

install(TARGETS akonadi_googlecontacts_resource ${INSTALL_TARGETS_DEFAULT_ARGS})

install(
  FILES googlecontactsresource.desktop
  DESTINATION "${CMAKE_INSTALL_PREFIX}/share/akonadi/agents"
)
