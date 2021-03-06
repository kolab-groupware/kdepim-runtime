include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared
    ${kdepim-runtime_SOURCE_DIR}
    ${QT_QTDBUS_INCLUDE_DIR}
    ${Boost_INCLUDE_DIR}
)

set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${KDE4_ENABLE_EXCEPTIONS}" )


########### next target ###############

add_definitions( -DSETTINGS_NAMESPACE=Akonadi_Aknotes_Resource )

set( notesresource_SRCS
  ${AKONADI_SINGLEFILERESOURCE_SHARED_SOURCES}
  ../shared/icalresourcebase.cpp
  ../shared/icalresource.cpp
  notesresource.cpp
)

install( FILES notesresource.desktop DESTINATION "${CMAKE_INSTALL_PREFIX}/share/akonadi/agents" )

kde4_add_ui_files(notesresource_SRCS ${AKONADI_SINGLEFILERESOURCE_SHARED_UI})
kde4_add_kcfg_files(notesresource_SRCS settings.kcfgc)
kcfg_generate_dbus_interface(${CMAKE_CURRENT_SOURCE_DIR}/notesresource.kcfg org.kde.Akonadi.Notes.Settings)
qt4_add_dbus_adaptor(notesresource_SRCS
  ${CMAKE_CURRENT_BINARY_DIR}/org.kde.Akonadi.Notes.Settings.xml settings.h Akonadi_Aknotes_Resource::Settings icalsettingsadaptor ICalSettingsAdaptor
)

kde4_add_plugin(akonadi_notes_resource ${notesresource_SRCS})

target_link_libraries(akonadi_notes_resource ${KDEPIMLIBS_AKONADI_LIBS} ${QT_QTDBUS_LIBRARY} ${KDE4_KIO_LIBS} ${KDEPIMLIBS_KCALCORE_LIBS})

install(TARGETS akonadi_notes_resource DESTINATION ${PLUGIN_INSTALL_DIR})
