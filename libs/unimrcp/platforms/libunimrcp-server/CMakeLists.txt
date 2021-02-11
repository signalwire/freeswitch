cmake_minimum_required (VERSION 2.8)
project (unimrcpserver)

# Set header files
set (UNIMRCP_SERVER_HEADERS
	include/unimrcp_server.h
)
source_group ("include" FILES ${UNIMRCP_SERVER_HEADERS})

# Set source files
set (UNIMRCP_SERVER_SOURCES
	src/unimrcp_server.c
)
source_group ("src" FILES ${UNIMRCP_SERVER_SOURCES})

# Library declaration
set (LIBRARY_TYPE SHARED)
if (MSVC)
	set (LIBRARY_TYPE STATIC)
endif (MSVC)
add_library (${PROJECT_NAME} ${LIBRARY_TYPE} ${UNIMRCP_SERVER_SOURCES} ${UNIMRCP_SERVER_HEADERS} 
	$<TARGET_OBJECTS:mrcpsofiasip>
	$<TARGET_OBJECTS:mrcpunirtsp>
	$<TARGET_OBJECTS:mrcpserver>
	$<TARGET_OBJECTS:mrcpengine>
	$<TARGET_OBJECTS:mrcpsignaling>
	$<TARGET_OBJECTS:mrcpv2transport>
	$<TARGET_OBJECTS:mrcp>
	$<TARGET_OBJECTS:unirtsp>
	$<TARGET_OBJECTS:mpf>
	$<TARGET_OBJECTS:aprtoolkit>
)
set_target_properties (${PROJECT_NAME} PROPERTIES FOLDER "platforms")

# Input libraries
target_link_libraries(${PROJECT_NAME} 
	${APU_LIBRARIES}
	${APR_LIBRARIES}
	${SOFIA_LIBRARIES}
)
# Input system libraries
if (WIN32)
	target_link_libraries(${PROJECT_NAME} ws2_32 winmm)
elseif (UNIX)
	target_link_libraries(${PROJECT_NAME} m)
endif ()

# Preprocessor definitions
add_definitions (
	${MRCP_DEFINES}
	${RTSP_DEFINES}
	${MPF_DEFINES}
	${APR_TOOLKIT_DEFINES} 
	${APR_DEFINES} 
	${APU_DEFINES}
)

# Include directories
include_directories (
	${PROJECT_SOURCE_DIR}/include
	${VERSION_INCLUDE_DIRS}
	${MRCP_SOFIASIP_INCLUDE_DIRS}
	${MRCP_UNIRTSP_INCLUDE_DIRS}
	${MRCP_SERVER_INCLUDE_DIRS}
	${MRCP_ENGINE_INCLUDE_DIRS}
	${MRCP_SIGNALING_INCLUDE_DIRS}
	${MRCPv2_TRANSPORT_INCLUDE_DIRS}
	${MRCP_INCLUDE_DIRS}
	${MPF_INCLUDE_DIRS}
	${APR_TOOLKIT_INCLUDE_DIRS}
	${APR_INCLUDE_DIRS}
	${APU_INCLUDE_DIRS}
)

# Installation directives
install (TARGETS ${PROJECT_NAME}
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
)
