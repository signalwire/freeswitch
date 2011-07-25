cmake_minimum_required(VERSION 2.6)
project(libamf C)

# generic variables
set(PACKAGE           "libamf")
set(PACKAGE_NAME      ${PACKAGE})
set(PACKAGE_BUGREPORT "libamf@sf.net")
set(PACKAGE_VERSION   "0.1.0")
set(PACKAGE_STRING    "${PACKAGE_NAME} ${PACKAGE_VERSION}")

#platform tests
include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckTypeSize)
include(TestBigEndian)

check_include_file(sys/types.h  HAVE_SYS_TYPES_H)
check_include_file(stdint.h     HAVE_STDINT_H)
check_include_file(stddef.h     HAVE_STDDEF_H)
check_include_file(inttypes.h   HAVE_INTTYPES_H)

check_type_size("double"      SIZEOF_DOUBLE)
check_type_size("float"       SIZEOF_FLOAT)
check_type_size("long double" SIZEOF_LONG_DOUBLE)

if(WIN32)
  set(int16_t  1)
  set(int32_t  1)
  set(int64_t  1)
  set(int8_t   1)
  set(uint16_t 1)
  set(uint32_t 1)
  set(uint64_t 1)
  set(uint8_t  1)
endif(WIN32)

test_big_endian(IS_BIGENDIAN)
if(IS_BIGENDIAN)
  set(WORDS_BIGENDIAN 1)
endif(IS_BIGENDIAN)

# configuration file
configure_file(amf-cmake.h.in ${CMAKE_BINARY_DIR}/amf.h)
include_directories(${CMAKE_BINARY_DIR})

install(
  FILES ${CMAKE_BINARY_DIR}/amf.h
  DESTINATION include/amf
)

# Visual C++ specific configuration
if(MSVC)
  # use static library
  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT")
  
  # C runtime deprecation in Visual C++ 2005 and later
  add_definitions(-D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE)
endif(MSVC)

add_subdirectory(src)

# tests
enable_testing()
add_subdirectory(tests)
