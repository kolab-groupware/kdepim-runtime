set( EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR} )
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${KDE4_ENABLE_EXCEPTIONS}" )

include_directories(
    ${kdepim-runtime_SOURCE_DIR}/plugins
    ${kdepim-runtime_BINARY_DIR}/plugins
    ${Boost_INCLUDE_DIR}
)

# convenience macro to add akonadi qtestlib unit-tests
macro(add_akonadiplugin_test _source _libs _additionalSources)
  set(_test ${_source})
  get_filename_component(_name ${_source} NAME_WE)
  set(srcs ${_test} ${_additionalSources})
  kde4_add_unit_test(${_name} TESTNAME akonadiplugin-${_name} ${srcs})
  target_link_libraries(${_name} ${KDEPIMLIBS_AKONADI_LIBS} ${KDEPIMLIBS_AKONADI_KMIME_LIBS} ${KDE4_KDECORE_LIBS}
                        ${QT_QTGUI_LIBRARY} ${QT_QTTEST_LIBRARY} ${AKONADI_COMMON_LIBRARIES}
                        ${AKONADI_COMMON_LIBRARIES} ${_libs})
endmacro()

# qtestlib unit tests
add_akonadiplugin_test(mailserializertest.cpp "${KDEPIMLIBS_KMIME_LIBS}" "")
add_akonadiplugin_test(mailserializerplugintest.cpp "${KDEPIMLIBS_KMIME_LIBS}" "")

add_akonadiplugin_test(kcalcoreserializertest.cpp "${KDEPIMLIBS_KCALCORE_LIBS}" "")

add_akonadiplugin_test(addresseeserializertest.cpp "${KDEPIMLIBS_KABC_LIBS};${KDEPIMLIBS_AKONADI_KABC_LIBS}" "../akonadi_serializer_addressee.cpp")

if (KDEPIMLIBS_KCAL_LIBS)
  add_akonadiplugin_test(kcalserializertest.cpp "${KDEPIMLIBS_KCAL_LIBS}" "")
endif ()
