set(libamf_src
  amf0.c
  amf0.h
  hash.c
  hash.h
  io.c
  io.h
  ptrarray.c
  ptrarray.h
  types.c
  types.h
  ${CMAKE_BINARY_DIR}/amf.h
)

add_library(amf ${libamf_src})

install(TARGETS amf
  RUNTIME DESTINATION lib
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
)

install(
  FILES amf0.h amf3.h
  DESTINATION include/amf
)
