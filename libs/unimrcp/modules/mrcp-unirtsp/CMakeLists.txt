cmake_minimum_required (VERSION 2.8)
project (mrcpunirtsp)

# Set header files
set (MRCP_UNIRTSP_HEADERS
	include/mrcp_unirtsp_sdp.h
	include/mrcp_unirtsp_logger.h
	include/mrcp_unirtsp_server_agent.h
	include/mrcp_unirtsp_client_agent.h
)
source_group ("include" FILES ${MRCP_UNIRTSP_HEADERS})

# Set source files
set (MRCP_UNIRTSP_SOURCES
	src/mrcp_unirtsp_sdp.c
	src/mrcp_unirtsp_logger.c
	src/mrcp_unirtsp_server_agent.c
	src/mrcp_unirtsp_client_agent.c
)
source_group ("src" FILES ${MRCP_UNIRTSP_SOURCES})

# Library declaration
add_library (${PROJECT_NAME} OBJECT ${MRCP_UNIRTSP_SOURCES} ${MRCP_UNIRTSP_HEADERS})
set_target_properties (${PROJECT_NAME} PROPERTIES FOLDER "modules")

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
	${MRCP_SIGNALING_INCLUDE_DIRS}
	${MRCP_INCLUDE_DIRS}
	${RTSP_INCLUDE_DIRS}
	${MPF_INCLUDE_DIRS}
	${APR_TOOLKIT_INCLUDE_DIRS}
	${APR_INCLUDE_DIRS}
	${APU_INCLUDE_DIRS}
	${SOFIA_INCLUDE_DIRS}
)
