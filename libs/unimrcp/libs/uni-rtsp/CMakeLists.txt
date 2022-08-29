cmake_minimum_required (VERSION 2.8)
project (unirtsp)

# Set header files
set (UNIRTSP_HEADERS
	include/rtsp.h
	include/rtsp_header.h
	include/rtsp_start_line.h
	include/rtsp_message.h
	include/rtsp_stream.h
	include/rtsp_server.h
	include/rtsp_client.h
)
source_group ("include" FILES ${UNIRTSP_HEADERS})

# Set source files
set (UNIRTSP_SOURCES
	src/rtsp_header.c
	src/rtsp_start_line.c
	src/rtsp_message.c
	src/rtsp_stream.c
	src/rtsp_server.c
	src/rtsp_client.c
)
source_group ("src" FILES ${UNIRTSP_SOURCES})

# Library declaration
add_library (${PROJECT_NAME} OBJECT ${UNIRTSP_SOURCES} ${UNIRTSP_HEADERS})
set_target_properties (${PROJECT_NAME} PROPERTIES FOLDER "libs")

# Preprocessor definitions
add_definitions (
	${RTSP_DEFINES}
	${APR_TOOLKIT_DEFINES} 
	${APR_DEFINES} 
	${APU_DEFINES}
)

# Include directories
include_directories (
	${PROJECT_SOURCE_DIR}/include
	${APR_TOOLKIT_INCLUDE_DIRS}
	${APR_INCLUDE_DIRS}
	${APU_INCLUDE_DIRS}
)
