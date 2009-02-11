# FREESWITCH CMAKE BUILD
# Created by SHANE BURRELL 2009 (supjigator)

PROJECT (freeswitch)

# project version 
SET( ${PROJECT_NAME}_MAJOR_VERSION 0 ) 
SET( ${PROJECT_NAME}_MINOR_VERSION 1 ) 
SET( ${PROJECT_NAME}_PATCH_LEVEL 0 ) 


set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules/")
INCLUDE (CheckSymbolExists)
INCLUDE (CheckIncludeFile)
INCLUDE (CheckIncludeFiles)
INCLUDE (CheckSymbolExists)
INCLUDE (CheckFunctionExists)
INCLUDE (CheckLibraryExists)
INCLUDE (CheckTypeSize)
INCLUDE (CheckCXXSourceCompiles)

CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

# ADD_DEFINITIONS( "-Wall -ansi -pedantic" ) 

MESSAGE( STATUS ) 
MESSAGE( STATUS "FREESWITCH CMAKE Build - Shane Burrell-----------------------------------------" ) 
MESSAGE( STATUS "BUILD_SHARED_LIBS = ${BUILD_SHARED_LIBS}" ) 
MESSAGE( STATUS "CMAKE_INSTALL_PREFIX = ${CMAKE_INSTALL_PREFIX}" ) 
MESSAGE( STATUS "CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}" ) 
MESSAGE( STATUS "CMAKE_MODULE_PATH = ${CMAKE_MODULE_PATH}" ) 
MESSAGE( STATUS "${PROJECT_NAME}_DEPENDS = \"${${PROJECT_NAME}_DEPENDS}\"" ) 
MESSAGE( STATUS "BUILD_WITH = \"${BUILD_WITH}\"" ) 
MESSAGE( STATUS "INSTALL_DOC = ${INSTALL_DOC}" ) 
MESSAGE( STATUS "Change a value with: cmake -D<Variable>=<Value>" ) 
MESSAGE( STATUS "-------------------------------------------------------------------------------" ) 
MESSAGE( STATUS ) 





# Configuration checks
check_symbol_exists (strncasecmp "string.h" HAVE_STRNCASECMP)
check_symbol_exists (strcasecmp "string.h" HAVE_STRCASECMP)
check_function_exists (strcasestr HAVE_STRCASESTR)
check_function_exists (strchrnul HAVE_STRCHRNUL)
check_include_file (dirent.h HAVE_DIRENT_H)
check_include_file (sys/ioctl.h HAVE_SYS_IOCTL_H)
check_include_file (sys/utsname.h HAVE_SYS_UTSNAME_H)
check_include_file (wchar.h HAVE_WCHAR_H)
check_include_file (wctype.h HAVE_WCTYPE_H)
check_function_exists (scandir HAVE_SCANDIR)
check_function_exists (alphasort HAVE_ALPHASORT)
check_symbol_exists (iswspace "wctype.h" HAVE_ISWSPACE)
check_symbol_exists (towlower "wctype.h" HAVE_TOWLOWER)
check_include_file(sys/devpoll.h HAVE_SYS_DEVPOLL_H)
check_include_file(sys/epoll.h HAVE_SYS_EPOLL_H)
check_include_file(sys/event.h HAVE_SYS_EVENT_H)
check_include_file(sys/mman.h HAVE_SYS_MMAN_H)
check_include_file(sys/poll.h HAVE_SYS_POLL_H)
check_include_file(sys/port.h HAVE_SYS_PORT_H)
check_include_file(sys/prctl.h HAVE_SYS_PRCTL_H)
check_include_file(sys/resource.h HAVE_SYS_RESOURCE_H)
check_include_file(sys/sendfile.h HAVE_SYS_SENDFILE_H)
check_include_file(sys/select.h HAVE_SYS_SELECT_H)
check_include_file(sys/syslimits.h HAVE_SYS_SYSLIMITS_H)
check_include_file(sys/types.h HAVE_SYS_TYPES_H)
check_include_file(sys/uio.h HAVE_SYS_UIO_H)
check_include_file(sys/un.h HAVE_SYS_UN_H)
check_include_file(sys/wait.h HAVE_SYS_WAIT_H)
check_include_file(sys/time.h HAVE_SYS_TIME_H)
check_include_file(time.h HAVE_TIME_H)
check_include_file(unistd.h HAVE_UNISTD_H)
check_include_file(pthread.h HAVE_PTHREAD_H)


MESSAGE( STATUS "-------------------------------------------------------------------------------" )
MESSAGE( STATUS )
MESSAGE( STATUS "-------------------------------------------------------------------------------" )
MESSAGE( STATUS )
# the complete system name, e.g. "Linux-2.4.22", "FreeBSD-5.4-RELEASE" or "Windows 5.1"
MESSAGE( STATUS "CMAKE_SYSTEM: " ${CMAKE_SYSTEM} )
# the short system name, e.g. "Linux", "FreeBSD" or "Windows"
MESSAGE( STATUS "CMAKE_SYSTEM_NAME: " ${CMAKE_SYSTEM_NAME} )
# only the version part of CMAKE_SYSTEM
MESSAGE( STATUS "CMAKE_SYSTEM_VERSION: " ${CMAKE_SYSTEM_VERSION} )
# the processor name (e.g. "Intel(R) Pentium(R) M processor 2.00GHz")
MESSAGE( STATUS "CMAKE_SYSTEM_PROCESSOR: " ${CMAKE_SYSTEM_PROCESSOR} )
# is TRUE on all UNIX-like OS's, including Apple OS X and CygWin
MESSAGE( STATUS "UNIX: " ${UNIX} )
# is TRUE on Windows, including CygWin
MESSAGE( STATUS "WIN32: " ${WIN32} )
# is TRUE on Apple OS X
MESSAGE( STATUS "APPLE: " ${APPLE} )
# is TRUE when using the MinGW compiler in Windows
MESSAGE( STATUS "MINGW: " ${MINGW} )
# is TRUE on Windows when using the CygWin version of cmake
MESSAGE( STATUS "CYGWIN: " ${CYGWIN} )
# is TRUE on Windows when using a Borland compiler
MESSAGE( STATUS "BORLAND: " ${BORLAND} )
# Microsoft compiler
MESSAGE( STATUS "MSVC: " ${MSVC} )
MESSAGE( STATUS "MSVC_IDE: " ${MSVC_IDE} )
MESSAGE( STATUS "MSVC60: " ${MSVC60} )
MESSAGE( STATUS "MSVC70: " ${MSVC70} )
MESSAGE( STATUS "MSVC71: " ${MSVC71} )
MESSAGE( STATUS "MSVC80: " ${MSVC80} )
MESSAGE( STATUS "CMAKE_COMPILER_2005: " ${CMAKE_COMPILER_2005} )
MESSAGE( STATUS "-------------------------------------------------------------------------------" )
MESSAGE( STATUS )

# DO THREADS
FIND_PACKAGE(Threads)


# DO APR
FIND_PACKAGE (APR)
FIND_PACKAGE (APRUtil)

LINK_LIBRARIES (${APR_LIBS} ${APRUTIL_LIBS})

MESSAGE( STATUS "APR INFO------------------------------------------" )
MESSAGE( STATUS "APR_LIBS = ${APR_LIBS}" )
MESSAGE( STATUS "APRUTIL_LIBS = ${APRUTIL_LIBS}" )
MESSAGE( STATUS "--------------------------------------------------" )
INCLUDE_DIRECTORIES (${APR_INCLUDES} ${APRUTIL_INCLUDES})


# DO SNDFILE

FIND_PACKAGE (Sndfile)
LINK_LIBRARIES (${SNDFILE_LIBRARY})
INCLUDE_DIRECTORIES (${SNDFILE_INCLUDE_DIR})


# DO LIBEDIT
FIND_PACKAGE (Libedit)
LINK_LIBRARIES (${PATH_LIB_EDIT})
INCLUDE_DIRECTORIES (${PATH_INC_EDIT})

# DO SRTP
LINK_LIBRARIES (${CMAKE_SOURCE_DIR}/libs/srtp/.libs/libcryptomath.a)
LINK_LIBRARIES (${CMAKE_SOURCE_DIR}/libs/srtp/.libs/libsrtp.a)
INCLUDE_DIRECTORIES ( ${CMAKE_SOURCE_DIR}/libs/srtp/crypto/include)

# SQLITE
LINK_LIBRARIES (${CMAKE_SOURCE_DIR}/libs/sqlite/.libs/libsqlite3.a)
INCLUDE_DIRECTORIES ( ${CMAKE_SOURCE_DIR}/libs/sqlite/)

# VOIPCODECS
LINK_LIBRARIES (${CMAKE_SOURCE_DIR}/libs/voipcodecs/src/.libs/libvoipcodecs.a)
INCLUDE_DIRECTORIES ( ${CMAKE_SOURCE_DIR}/libs/voipcodecs/src/)

# DO POSTGRES
#FIND_PACKAGE(POSTGRES)
IF(POSTGRES_FOUND)
   INCLUDE_DIRECTORIES( ${POSTGRES_INCLUDE_DIRECTORIES} )
   SET(optionalLibs ${optionalLibs} ${POSTGRES_LIBRARIES} )
ENDIF(POSTGRES_FOUND)

CHECK_INCLUDE_FILE(netdb.h HAVE_NETDB_H)
if( HAVE_NETDB_H)
	add_definitions( -DHAVE_NETDB_H)
endif( HAVE_NETDB_H)


INCLUDE_DIRECTORIES(src/include/ libs/libteletone/src/ libs/stfu/ libs/apr/include/ libs/apr-util/include/)

ADD_SUBDIRECTORY(src)
ADD_SUBDIRECTORY(libs/libresample/)
ADD_SUBDIRECTORY(libs/libteletone/)
#ADD_SUBDIRECTORY(libs/sqlite/)
ADD_SUBDIRECTORY(libs/pcre/)
ADD_SUBDIRECTORY(libs/stfu/)
#ADD_SUBDIRECTORY(libs/libedit/)
ADD_SUBDIRECTORY(libs/srtp/)

SET ( freeswitch_SRCS src/switch.c src/include/switch.h)
ADD_EXECUTABLE(freeswitch ${freeswitch_SRCS})


TARGET_LINK_LIBRARIES(freeswitch teletone pcre resample switch_ivr switch_ivr_async switch_core switch_core_asr switch_channel switch_core_io switch_apr switch_utils switch_xml switch_buffer switch_caller switch_buffer switch_config switch_console switch_core_codec switch_core_db switch_core_directory switch_core_event_hook switch_core_file switch_core_hash switch_core_media_bug switch_core_memory switch_core_port_allocator switch_core_rwlock switch_core_session switch_core_speech switch_core_sqldb switch_core_state_machine switch_core_timer switch_dso switch_event switch_ivr_async switch_ivr_bridge switch_ivr_menu switch_ivr_originate switch_ivr_play_say switch_loadable_module switch_log switch_odbc switch_pcm switch_regex switch_resample switch_rtp switch_scheduler switch_stun switch_swig switch_time stfu ${optionalLibs})


