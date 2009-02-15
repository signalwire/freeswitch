# FREESWITCH CMAKE BUILD
# Created by SHANE BURRELL 2009 (supjigator)

PROJECT (freeswitch)

#GETSVN VERSION
FIND_PACKAGE(Subversion)
IF(Subversion_FOUND)
	Subversion_WC_INFO(${CMAKE_HOME_DIRECTORY} Project)
	MESSAGE("Current revision is ${Project_WC_REVISION}")
ENDIF(Subversion_FOUND)



# project version 
SET( ${PROJECT_NAME}_MAJOR_VERSION 0 ) 
SET( ${PROJECT_NAME}_MINOR_VERSION 1 ) 
SET( ${PROJECT_NAME}_PATCH_LEVEL 0 ) 


#ADD_DEFINITIONS( "-DSWITCH_VERSION_REVISION=${Project_WC_REVISION}" )


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
check_symbol_exists (iswspace "wctype.h" HAVE_ISWSPACE)
check_symbol_exists (towlower "wctype.h" HAVE_TOWLOWER)

check_include_file (dirent.h HAVE_DIRENT_H)
check_include_file (dlfcn.h HAVE_DLFCN_H)
check_include_file (execinfo.h HAVE_EXECINFO_H)
check_include_file (memory.h HAVE_MEMORY_H)
check_include_file (inttypes.h HAVE_INTTYPES_H)
check_include_file (strings.h HAVE_STRINGS_H)
check_include_file (string.h HAVE_STRING_H)
check_include_file (sys/filio.h HAVE_SYS_FILIO_H)
check_include_file (sys/ndir.h HAVE_SYS_NDIR_H)
CHECK_INCLUDE_FILE(netdb.h HAVE_NETDB_H)


check_include_file (sys/ioctl.h HAVE_SYS_IOCTL_H)
check_include_file (sys/utsname.h HAVE_SYS_UTSNAME_H)
check_include_file (wchar.h HAVE_WCHAR_H)
check_include_file (wctype.h HAVE_WCTYPE_H)
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

check_function_exists (scandir HAVE_SCANDIR)
check_function_exists (alphasort HAVE_ALPHASORT)
check_function_exists (strcasestr HAVE_STRCASESTR)
check_function_exists (strchrnul HAVE_STRCHRNUL)
check_function_exists (setenv HAVE_SETENV)
check_function_exists (setgroups HAVE_SETGROUPS)


check_function_exists (pselect HAVE_PSELECT)
check_function_exists (mlock HAVE_MLOCK)
check_function_exists (mlockall HAVE_MLOCKALL)
check_function_exists (asprintf HAVE_ASPRINTF)
check_function_exists (gethostname HAVE_GETHOSTNAME)
check_function_exists (gettimeofday HAVE_GETTIMEOFDAY)
check_function_exists (gmtime_r HAVE_GMTIME_R)
check_function_exists (initgroups HAVE_INITGROUPS)


# OPTION( VARIABLE "Description" Initial state)
 OPTION( DEBUG "Debug" OFF )
 OPTION( DISABLE_RESAMPLE "Disable RESAMPLE" OFF )
 OPTION( HAVE_ODBC "Enable Core ODBC Support" ON )
 OPTION( OPTIMZER "Enable Optimization" OFF )

 CONFIGURE_FILE( ${CMAKE_SOURCE_DIR}/src/include/switch_private.h.cmake ${CMAKE_SOURCE_DIR}/src/include/switch_private.h )






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

INCLUDE_DIRECTORIES ( ${CMAKE_SOURCE_DIR}/libs/apr/include/ ${CMAKE_SOURCE_DIR}/libs/apr-util/include/)
LINK_LIBRARIES (${CMAKE_SOURCE_DIR}/libs/apr/.libs/libapr-1.a)
LINK_LIBRARIES (${CMAKE_SOURCE_DIR}/libs/apr-util/.libs/libaprutil-1.a)


# DO SNDFILE

FIND_PACKAGE (Sndfile)
LINK_LIBRARIES (${SNDFILE_LIBRARY})
INCLUDE_DIRECTORIES (${SNDFILE_INCLUDE_DIR})


# DO LIBEDIT
#FIND_PACKAGE (Libedit)
#LINK_LIBRARIES ("/Users/shaneburrell/freeswitch-svn-real/libs/libedit/src/.libs/libedit.a")
#INCLUDE_DIRECTORIES("libs/libedit/src/")
#LINK_LIBRARIES (${PATH_LIB_EDIT})
#INCLUDE_DIRECTORIES (${PATH_INC_EDIT})

#if( PATH_LIB_EDIT)
#        add_definitions( -DSWITCH_HAVE_LIBEDIT)
#	add_definitions( -DHAVE_EDITLINE)
#	MESSAGE( STATUS "Found libedit = ${PATH_LIB_EDIT}" )
#endif( PATH_LIB_EDIT)


#TESTING SECTION
add_definitions(-DMACOSX)
#add_definitions("-DSWITCH_PREFIX_DIR=/usr/local/freeswitch")



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

# libspeexdsp
LINK_LIBRARIES (${CMAKE_SOURCE_DIR}/libs/speex/libspeex/.libs/libspeexdsp.a)
INCLUDE_DIRECTORIES ( ${CMAKE_SOURCE_DIR}/libs/speex/include/)


# DO POSTGRES
#FIND_PACKAGE(POSTGRES)
IF(POSTGRES_FOUND)
   INCLUDE_DIRECTORIES( ${POSTGRES_INCLUDE_DIRECTORIES} )
   SET(optionalLibs ${optionalLibs} ${POSTGRES_LIBRARIES} )
ENDIF(POSTGRES_FOUND)




configure_file(src/include/switch_version.h.in src/include/switch_version.h [@ONLY])









INCLUDE_DIRECTORIES(src/include/ libs/libteletone/src/ libs/stfu/ libs/apr/include/ libs/apr-util/include/)

ADD_SUBDIRECTORY(src)
#ADD_SUBDIRECTORY(libs/libresample/)
ADD_SUBDIRECTORY(libs/libteletone/)
#ADD_SUBDIRECTORY(libs/sqlite/)
ADD_SUBDIRECTORY(libs/pcre/)
ADD_SUBDIRECTORY(libs/stfu/)
#ADD_SUBDIRECTORY(libs/libedit/src/)
ADD_SUBDIRECTORY(libs/srtp/)

SET ( freeswitch_SRCS src/switch.c src/include/switch.h)
ADD_EXECUTABLE(freeswitch ${freeswitch_SRCS})


TARGET_LINK_LIBRARIES(freeswitch teletone pcre freeswitch_la stfu ${optionalLibs})


