
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${KDE4_ENABLE_EXCEPTIONS}" )

add_subdirectory( tests )

set( resourcetester_SRCS
  global.cpp
  main.cpp
  resource.cpp
  script.cpp
  test.cpp
  xmloperations.cpp
  collectiontest.cpp
  itemtest.cpp
  qemu.cpp
  system.cpp
  wrappedobject.cpp
)

kde4_add_executable(resourcetester ${resourcetester_SRCS})

target_link_libraries( resourcetester
  ${KDEPIMLIBS_AKONADI_XML_LIBS}
  ${KDEPIMLIBS_AKONADI_LIBS}
  ${QT_QTCORE_LIBRARY}
  ${QT_QTGUI_LIBRARY}
  ${QT_QTXML_LIBRARY}
  ${QT_QTDBUS_LIBRARY}
  ${QT_QTNETWORK_LIBRARY}
  ${QT_QTTEST_LIBRARY}
  ${KDE4_KROSSCORE_LIBS}
  ${KDE4_KROSSUI_LIBS}
  ${KDE4_KDECORE_LIBS}
  ${KDE4_KDEUI_LIBS}
  ${KDE4_KIO_LIBS}
)

