cmake_minimum_required(VERSION 3.12)

project(libsrtp2 VERSION 2.4.0 LANGUAGES C)

set(PACKAGE_VERSION ${CMAKE_PROJECT_VERSION})
set(PACKAGE_STRING "${CMAKE_PROJECT_NAME} ${CMAKE_PROJECT_VERSION}")

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(TestBigEndian)
include(CheckIncludeFile)
include(CheckFunctionExists)
include(CheckTypeSize)
include(CheckCSourceCompiles)

test_big_endian(WORDS_BIGENDIAN)

if (NOT APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)|(amd64)|(AMD64)")
  set (HAVE_X86 TRUE)
else ()
  set (HAVE_X86 FALSE)
endif ()

check_include_file(arpa/inet.h HAVE_ARPA_INET_H)
check_include_file(byteswap.h HAVE_BYTESWAP_H)
check_include_file(inttypes.h HAVE_INTTYPES_H)
check_include_file(machine/types.h HAVE_MACHINE_TYPES_H)
check_include_file(netinet/in.h HAVE_NETINET_IN_H)
check_include_file(stdint.h HAVE_STDINT_H)
check_include_file(stdlib.h HAVE_STDLIB_H)
check_include_file(sys/int_types.h HAVE_SYS_INT_TYPES_H)
check_include_file(sys/socket.h HAVE_SYS_SOCKET_H)
check_include_file(sys/types.h HAVE_SYS_TYPES_H)
check_include_file(unistd.h HAVE_UNISTD_H)
check_include_file(windows.h HAVE_WINDOWS_H)
check_include_file(winsock2.h HAVE_WINSOCK2_H)

check_function_exists(sigaction HAVE_SIGACTION)
check_function_exists(inet_aton HAVE_INET_ATON)
check_function_exists(usleep HAVE_USLEEP)

check_type_size(uint8_t UINT8_T)
check_type_size(uint16_t UINT16_T)
check_type_size(uint32_t UINT32_T)
check_type_size(uint64_t UINT64_T)
check_type_size(int32_t INT32_T)
check_type_size("unsigned long" SIZEOF_UNSIGNED_LONG)
check_type_size("unsigned long long" SIZEOF_UNSIGNED_LONG_LONG)

check_c_source_compiles("inline void func(); void func() { } int main() { func(); return 0; }" HAVE_INLINE)
if(NOT HAVE_INLINE)
  check_c_source_compiles("__inline void func(); void func() { } int main() { func(); return 0; }" HAVE___INLINE)
endif()

set(ENABLE_DEBUG_LOGGING OFF CACHE BOOL "Enable debug logging in all modules")
set(ERR_REPORTING_STDOUT OFF CACHE BOOL "Enable logging to stdout")
set(ERR_REPORTING_FILE "" CACHE FILEPATH "Use file for logging")
set(ENABLE_OPENSSL OFF CACHE BOOL "Enable OpenSSL crypto engine")
set(ENABLE_MBEDTLS OFF CACHE BOOL "Enable MbedTLS crypto engine")
set(ENABLE_NSS OFF CACHE BOOL "Enable NSS crypto engine")
set(TEST_APPS ON CACHE BOOL "Build test applications")
set(BUILD_WITH_SANITIZERS OFF CACHE BOOL "Adds sanitizer flags to compiler")
option(BUILD_SHARED_LIBS "Build shared library" OFF)

if(ENABLE_OPENSSL OR ENABLE_MBEDTLS OR ENABLE_NSS)
  set(USE_EXTERNAL_CRYPTO TRUE)
else()
  set(USE_EXTERNAL_CRYPTO FALSE)
endif()

if(ENABLE_OPENSSL)
  if(ENABLE_NSS OR ENABLE_MBEDTLS)
    message(FATAL_ERROR "ssl conflict. can not enable openssl and mbedtls or nss simultaneously.")
  endif()
  find_package(OpenSSL REQUIRED)
  set(OPENSSL ${ENABLE_OPENSSL} CACHE BOOL INTERNAL)
  set(GCM ${ENABLE_OPENSSL} CACHE BOOL INTERNAL)
endif()

if(ENABLE_MBEDTLS)
  if(ENABLE_OPENSSL OR ENABLE_NSS)
    message(FATAL_ERROR "ssl conflict. can not enable mbedtls and openssl or nss simultaneously.")
  endif()
  find_package(MbedTLS REQUIRED)
  set(MBEDTLS ${ENABLE_MBEDTLS} CACHE BOOL INTERNAL)
  set(GCM ${ENABLE_MBEDTLS} CACHE BOOL INTERNAL)
endif()

if(ENABLE_NSS)
  if(ENABLE_OPENSSL OR ENABLE_MBEDTLS)
    message(FATAL_ERROR "ssl conflict. can not enable nss and openssl or mbedtls simultaneously.")
  endif()
  find_package(NSS REQUIRED)
  set(NSS ${ENABLE_NSS} CACHE BOOL INTERNAL)
  set(GCM ${ENABLE_NSS} CACHE BOOL INTERNAL)
endif()

set(CONFIG_FILE_DIR ${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CONFIG_FILE_DIR})

configure_file(config_in_cmake.h ${CONFIG_FILE_DIR}/config.h)
add_definitions(-DHAVE_CONFIG_H)

if(BUILD_WITH_SANITIZERS AND NOT WIN32)
  if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(SANITIZERS address,undefined)
  else()
    set(SANITIZERS leak,address,undefined)
  endif()
  message(STATUS "Using sanitizers: ${SANITIZERS}")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-omit-frame-pointer -fsanitize=${SANITIZERS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fsanitize=${SANITIZERS}")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fno-omit-frame-pointer -fsanitize=${SANITIZERS}")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fno-omit-frame-pointer -fsanitize=${SANITIZERS}")
  set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -fno-omit-frame-pointer -fsanitize=${SANITIZERS}")
endif()

set(SOURCES_C
  srtp/srtp.c
)

set(CIPHERS_SOURCES_C
  crypto/cipher/cipher.c
  crypto/cipher/cipher_test_cases.c
  crypto/cipher/cipher_test_cases.h
  crypto/cipher/null_cipher.c
)

if(ENABLE_OPENSSL)
  list(APPEND CIPHERS_SOURCES_C
    crypto/cipher/aes_icm_ossl.c
    crypto/cipher/aes_gcm_ossl.c
  )
elseif(ENABLE_MBEDTLS)
  list(APPEND CIPHERS_SOURCES_C
    crypto/cipher/aes_icm_mbedtls.c
    crypto/cipher/aes_gcm_mbedtls.c
  )
elseif(ENABLE_NSS)
  list(APPEND CIPHERS_SOURCES_C
    crypto/cipher/aes_icm_nss.c
    crypto/cipher/aes_gcm_nss.c
  )
else()
  list(APPEND CIPHERS_SOURCES_C
    crypto/cipher/aes.c
    crypto/cipher/aes_icm.c
  )
endif()

set(HASHES_SOURCES_C
  crypto/hash/auth.c
  crypto/hash/auth_test_cases.c
  crypto/hash/auth_test_cases.h
  crypto/hash/null_auth.c
)

if(ENABLE_OPENSSL)
  list(APPEND HASHES_SOURCES_C
    crypto/hash/hmac_ossl.c
  )
elseif(ENABLE_MBEDTLS)
  list(APPEND HASHES_SOURCES_C
    crypto/hash/hmac_mbedtls.c
  )
elseif(ENABLE_NSS)
  list(APPEND HASHES_SOURCES_C
    crypto/hash/hmac_nss.c
  )
else()
  list(APPEND HASHES_SOURCES_C
    crypto/hash/hmac.c
    crypto/hash/sha1.c
  )
endif()

set(KERNEL_SOURCES_C
  crypto/kernel/alloc.c
  crypto/kernel/crypto_kernel.c
  crypto/kernel/err.c
  crypto/kernel/key.c
)

set(MATH_SOURCES_C
  crypto/math/datatypes.c
)

set(REPLAY_SOURCES_C
  crypto/replay/rdb.c
  crypto/replay/rdbx.c
)

set(SOURCES_H
  crypto/include/aes.h
  crypto/include/aes_icm.h
  crypto/include/alloc.h
  crypto/include/auth.h
  crypto/include/cipher.h
  crypto/include/cipher_types.h
  crypto/include/crypto_kernel.h
  crypto/include/crypto_types.h
  crypto/include/datatypes.h
  crypto/include/err.h
  crypto/include/hmac.h
  crypto/include/integers.h
  crypto/include/key.h
  crypto/include/null_auth.h
  crypto/include/null_cipher.h
  crypto/include/rdb.h
  crypto/include/rdbx.h
  crypto/include/sha1.h
  include/srtp.h
  include/srtp_priv.h
  ${CONFIG_FILE_DIR}/config.h
)

if(BUILD_SHARED_LIBS AND WIN32)
  list(APPEND SOURCES_C
    srtp.def
  )
endif()

source_group("src" FILES ${SOURCES_C})
source_group("src\\Ciphers" FILES ${CIPHERS_SOURCES_C})
source_group("src\\Hashes" FILES ${HASHES_SOURCES_C})
source_group("src\\Kernel" FILES ${KERNEL_SOURCES_C})
source_group("src\\Math" FILES ${MATH_SOURCES_C})
source_group("src\\Replay" FILES ${REPLAY_SOURCES_C})
source_group("include" FILES ${SOURCES_H})

add_library(srtp2
  ${SOURCES_C}
  ${CIPHERS_SOURCES_C}
  ${HASHES_SOURCES_C}
  ${KERNEL_SOURCES_C}
  ${MATH_SOURCES_C}
  ${REPLAY_SOURCES_C}
  ${SOURCES_H}
)

set_target_properties(srtp2 PROPERTIES VERSION ${CMAKE_PROJECT_VERSION})

target_include_directories(srtp2 PUBLIC crypto/include include)
if(ENABLE_OPENSSL)
  target_include_directories(srtp2 PRIVATE ${OPENSSL_INCLUDE_DIR})
  target_link_libraries(srtp2 OpenSSL::Crypto)
elseif(ENABLE_MBEDTLS)
  target_include_directories(srtp2 PRIVATE ${MBEDTLS_INCLUDE_DIRS})
  target_link_libraries(srtp2 ${MBEDTLS_LIBRARIES})
elseif(ENABLE_NSS)
  target_include_directories(srtp2 PRIVATE ${NSS_INCLUDE_DIRS})
  target_link_libraries(srtp2 ${NSS_LIBRARIES})
endif()
if(WIN32)
  target_link_libraries(srtp2 ws2_32)
  target_compile_definitions(srtp2 PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()

install(TARGETS srtp2 DESTINATION lib)
install(FILES include/srtp.h crypto/include/auth.h
  crypto/include/cipher.h
  crypto/include/crypto_types.h
  DESTINATION include/srtp2)

if(TEST_APPS)
  enable_testing()

  if(NOT (BUILD_SHARED_LIBS AND WIN32))
    if(NOT USE_EXTERNAL_CRYPTO)
      add_executable(aes_calc crypto/test/aes_calc.c test/getopt_s.c test/util.c)
      target_include_directories(aes_calc PRIVATE test)
      target_link_libraries(aes_calc srtp2)
      add_test(aes_calc_128 aes_calc 000102030405060708090a0b0c0d0e0f
                                     00112233445566778899aabbccddeeff
                                     69c4e0d86a7b0430d8cdb78070b4c55a)
      add_test(aes_calc_256 aes_calc 000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f
                                     00112233445566778899aabbccddeeff
                                     8ea2b7ca516745bfeafc49904b496089)

      add_executable(sha1_driver crypto/test/sha1_driver.c test/util.c)
      target_include_directories(sha1_driver PRIVATE test)
      target_link_libraries(sha1_driver srtp2)
      add_test(sha1_driver sha1_driver -v)
    endif()

    add_executable(datatypes_driver crypto/test/datatypes_driver.c test/util.c)
    target_include_directories(datatypes_driver PRIVATE test)
    target_link_libraries(datatypes_driver srtp2)
    add_test(datatypes_driver datatypes_driver -v)

    add_executable(cipher_driver crypto/test/cipher_driver.c test/getopt_s.c)
    target_include_directories(cipher_driver PRIVATE test)
    target_link_libraries(cipher_driver srtp2)
    add_test(cipher_driver cipher_driver -v)

    add_executable(kernel_driver crypto/test/kernel_driver.c test/getopt_s.c)
    target_include_directories(kernel_driver PRIVATE test)
    target_link_libraries(kernel_driver srtp2)
    add_test(kernel_driver kernel_driver -v)

    add_executable(rdbx_driver test/rdbx_driver.c test/getopt_s.c test/ut_sim.c)
    target_include_directories(rdbx_driver PRIVATE test)
    target_link_libraries(rdbx_driver srtp2)
    add_test(rdbx_driver rdbx_driver -v)

    add_executable(replay_driver test/replay_driver.c test/ut_sim.c)
    target_include_directories(replay_driver PRIVATE test)
    target_link_libraries(replay_driver srtp2)
    add_test(replay_driver replay_driver -v)

    add_executable(roc_driver test/roc_driver.c test/ut_sim.c)
    target_include_directories(roc_driver PRIVATE test)
    target_link_libraries(roc_driver srtp2)
    add_test(roc_driver roc_driver -v)
  endif()

  add_executable(srtp_driver test/srtp_driver.c
    test/util.c test/getopt_s.c)
  target_link_libraries(srtp_driver srtp2)
  add_test(srtp_driver srtp_driver -v)

  if(NOT (BUILD_SHARED_LIBS AND WIN32))
    add_executable(test_srtp test/test_srtp.c)
    if(ENABLE_OPENSSL)
      target_include_directories(test_srtp PRIVATE ${OPENSSL_INCLUDE_DIR})
    elseif(ENABLE_MBEDTLS)
      target_include_directories(test_srtp PRIVATE ${MBEDTLS_INCLUDE_DIRS})
    elseif(ENABLE_NSS)
      target_include_directories(test_srtp PRIVATE ${NSS_INCLUDE_DIRS})
    endif()
    target_link_libraries(test_srtp srtp2)
    add_test(test_srtp test_srtp)
  endif()

  find_program(BASH_PROGRAM bash)
  if(BASH_PROGRAM AND NOT WIN32)
    add_executable(rtpw test/rtpw.c test/rtp.c test/util.c test/getopt_s.c)
    target_link_libraries(rtpw srtp2)
    add_test(NAME rtpw_test
             COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test/rtpw_test.sh -w ${CMAKE_CURRENT_SOURCE_DIR}/test/words.txt
             WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
    if(GCM)
      add_test(NAME rtpw_test_gcm
               COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test/rtpw_test_gcm.sh -w ${CMAKE_CURRENT_SOURCE_DIR}/test/words.txt
               WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
    endif()
  endif()
endif()
