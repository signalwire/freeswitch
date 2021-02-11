cmake_minimum_required (VERSION 2.8)
project (UniMRCP C)

# Organize targets into folders
set_property (GLOBAL PROPERTY USE_FOLDERS ON)

# Set source directories for dependencies
set (APR_SOURCE_DIR ${CMAKE_SOURCE_DIR}/libs/apr CACHE PATH "Path to APR source directory")
set (APU_SOURCE_DIR ${CMAKE_SOURCE_DIR}/libs/apr-util CACHE PATH "Path to APR-util source directory")
set (SOFIA_SOURCE_DIR ${CMAKE_SOURCE_DIR}/libs/sofia-sip CACHE PATH "Path to SofiaSIP source directory")

# Set CMake module path
set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/build/cmake)

# Find dependencies
find_package (APR REQUIRED)
find_package (APRUtil REQUIRED)
find_package (Sofia REQUIRED)

# Set API definitions
set (APR_TOOLKIT_DEFINES -DAPT_STATIC_LIB)
set (MPF_DEFINES -DMPF_STATIC_LIB)
set (RTSP_DEFINES -DRTSP_STATIC_LIB)
set (MRCP_DEFINES -DMRCP_STATIC_LIB)

# Set compiler flags
if (CMAKE_C_COMPILER_ID MATCHES MSVC)
	# Microsoft Visual Studio Compiler
	set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W4 /WX /wd4100")
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /WX /wd4100")
elseif (CMAKE_C_COMPILER_ID MATCHES GNU)
	# GNU Compiler
	set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -Wall -Werror")
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -Wall -Werror")
endif ()

# Shared modules (plug-ins) should not have any prefix set
set (CMAKE_SHARED_MODULE_PREFIX "")

# Pertain RPATH upon installation 
set (CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# Add library directory to RPATH
set (CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)

# Set default installation path
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	set (CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/installed" CACHE PATH "Default installation path" FORCE )
endif ()

# Set include directories
set (APR_TOOLKIT_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/libs/apr-toolkit/include)
set (MPF_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/libs/mpf/include)
set (RTSP_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/libs/uni-rtsp/include)
set (MRCP_INCLUDE_DIRS
	${CMAKE_SOURCE_DIR}/libs/mrcp/include
	${CMAKE_SOURCE_DIR}/libs/mrcp/control/include
	${CMAKE_SOURCE_DIR}/libs/mrcp/message/include
	${CMAKE_SOURCE_DIR}/libs/mrcp/resources/include)
set (MRCP_SIGNALING_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/libs/mrcp-signaling/include)
set (MRCPv2_TRANSPORT_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/libs/mrcpv2-transport/include)
set (MRCP_ENGINE_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/libs/mrcp-engine/include)
set (MRCP_CLIENT_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/libs/mrcp-client/include)
set (MRCP_SERVER_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/libs/mrcp-server/include)
set (MRCP_SOFIASIP_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/modules/mrcp-sofiasip/include)
set (MRCP_UNIRTSP_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/modules/mrcp-unirtsp/include)
set (UNIMRCP_CLIENT_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/platforms/libunimrcp-client/include)
set (UNIMRCP_SERVER_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/platforms/libunimrcp-server/include)
set (VERSION_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/build)

# Sub-projects: libraries
add_subdirectory (libs/apr-toolkit)
add_subdirectory (libs/mpf)
add_subdirectory (libs/uni-rtsp)
add_subdirectory (libs/mrcp)
add_subdirectory (libs/mrcp-signaling)
add_subdirectory (libs/mrcpv2-transport)
add_subdirectory (libs/mrcp-engine)
add_subdirectory (libs/mrcp-client)
add_subdirectory (libs/mrcp-server)

# Sub-projects: modules
add_subdirectory (modules/mrcp-sofiasip)
add_subdirectory (modules/mrcp-unirtsp)

# Sub-projects: plug-ins
add_subdirectory (plugins/demo-recog)
add_subdirectory (plugins/demo-synth)
add_subdirectory (plugins/demo-verifier)
add_subdirectory (plugins/mrcp-recorder)

# Sub-projects: platform libraries
add_subdirectory (platforms/libunimrcp-client)
add_subdirectory (platforms/libunimrcp-server)

# Sub-projects: platform applications
add_subdirectory (platforms/umc)
add_subdirectory (platforms/unimrcp-client)
add_subdirectory (platforms/unimrcp-server)

# Sub-projects: asr-client
add_subdirectory (platforms/libasr-client)
add_subdirectory (platforms/asr-client)

# Sub-projects: tests
add_subdirectory (tests/apttest)
add_subdirectory (tests/mpftest)
add_subdirectory (tests/mrcptest)
add_subdirectory (tests/rtsptest)
add_subdirectory (tests/strtablegen)

# Installation directives
install (DIRECTORY DESTINATION log)
install (DIRECTORY DESTINATION var)
install (DIRECTORY data/ DESTINATION data PATTERN Makefile* EXCLUDE)
install (DIRECTORY conf/ DESTINATION conf PATTERN Makefile* EXCLUDE)
