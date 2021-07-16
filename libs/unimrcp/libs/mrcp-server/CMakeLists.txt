cmake_minimum_required (VERSION 2.8)
project (mrcpserver)

# Set header files
set (MRCP_SERVER_HEADERS
	include/mrcp_server_types.h
	include/mrcp_server.h
	include/mrcp_server_session.h
)
source_group ("include" FILES ${MRCP_SERVER_HEADERS})

# Set source files
set (MRCP_SERVER_SOURCES
	src/mrcp_server.c
	src/mrcp_server_session.c
)
source_group ("src" FILES ${MRCP_SERVER_SOURCES})

# Library declaration
add_library (${PROJECT_NAME} OBJECT ${MRCP_SERVER_SOURCES} ${MRCP_SERVER_HEADERS})
set_target_properties (${PROJECT_NAME} PROPERTIES FOLDER "libs")

# Preprocessor definitions
add_definitions (
	${MRCP_DEFINES}
	${MPF_DEFINES}
	${APR_TOOLKIT_DEFINES} 
	${APR_DEFINES} 
	${APU_DEFINES}
)

# Include directories
include_directories (
	${PROJECT_SOURCE_DIR}/include
	${MRCP_SIGNALING_INCLUDE_DIRS}
	${MRCPv2_TRANSPORT_INCLUDE_DIRS}
	${MRCP_ENGINE_INCLUDE_DIRS}
	${MRCP_INCLUDE_DIRS}
	${MPF_INCLUDE_DIRS}
	${APR_TOOLKIT_INCLUDE_DIRS}
	${APR_INCLUDE_DIRS}
	${APU_INCLUDE_DIRS}
)
