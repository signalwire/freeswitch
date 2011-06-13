include_directories(${CMAKE_SOURCE_DIR}/src)

add_executable(amf0_demo amf0_demo.c)
add_dependencies(amf0_demo amf)

