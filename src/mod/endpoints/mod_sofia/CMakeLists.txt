cmake_minimum_required(VERSION 3.1)
project(mod_sofia C)
set(PROJECT_VERSION 1.0.0)

find_package(PkgConfig REQUIRED)
set(ENV{PKG_CONFIG_PATH} "/usr/local/freeswitch/lib/pkgconfig:/usr/local/sofia-sip/lib/pkgconfig")

pkg_check_modules(FS REQUIRED freeswitch)
pkg_check_modules(SOFIA REQUIRED sofia-sip-ua)

execute_process(
    COMMAND ${PKG_CONFIG_EXECUTABLE} freeswitch --variable=modulesdir
    OUTPUT_VARIABLE FS_MODULES_DIRS
)
string(REPLACE "\n" "" FS_MODULES_DIRS ${FS_MODULES_DIRS})
string(REGEX REPLACE "[.-]" ";" FS_VERSION_LIST ${FS_VERSION})
list(GET FS_VERSION_LIST 0 FS_MAJOR_VERSION)
list(GET FS_VERSION_LIST 1 FS_MINOR_VERSION)
list(GET FS_VERSION_LIST 2 FS_MICRO_VERSION)

message(STATUS "FreeSWITCH version: " ${FS_VERSION})

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/mod_config.h.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/mod_config.h
)

set(FREESWITCH_SRC_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../../../include")
include_directories(
    ${FREESWITCH_SRC_INCLUDE}
    ${FS_INCLUDE_DIRS}
    ${SOFIA_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

link_directories(
    ${FS_LIBRARY_DIRS}
    ${SOFIA_LIBRARY_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
)

set(sources
    mod_sofia.c
    rtp.c
    sip-dig.c
    sofia.c
    sofia_glue.c
    sofia_json_api.c
    sofia_media.c
    sofia_presence.c
    sofia_reg.c
)

set(libraries
    ${FS_LIBRARIES}
    ${SOFIA_LIBRARIES}
)

add_library(mod_sofia SHARED ${sources})
set_target_properties(mod_sofia PROPERTIES PREFIX "")
target_link_libraries(mod_sofia ${libraries})

add_executable(test_sofia_funcs test/test_sofia_funcs.c)
target_compile_definitions(test_sofia_funcs PRIVATE "-DSWITCH_TEST_BASE_DIR_FOR_CONF=\"${CMAKE_SOURCE_DIR}/test\"")
target_compile_definitions(test_sofia_funcs PRIVATE "-DSWITCH_TEST_BASE_DIR_OVERRIDE=\"${CMAKE_BINARY_DIR}/test\"")
target_link_libraries(test_sofia_funcs mod_sofia ${libraries})

if (FS_MODULES_DIRS)
    install(
        TARGETS mod_sofia
        DESTINATION ${FS_MODULES_DIRS}
    )
endif()
