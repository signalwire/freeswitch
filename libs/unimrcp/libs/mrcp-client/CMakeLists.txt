cmake_minimum_required (VERSION 2.8)
project (mrcpclient)

# Set header files
set (MRCP_CLIENT_HEADERS
	include/mrcp_client_types.h
	include/mrcp_client.h
	include/mrcp_client_session.h
	include/mrcp_application.h
)
source_group ("include" FILES ${MRCP_CLIENT_HEADERS})

# Set source files
set (MRCP_CLIENT_SOURCES
	src/mrcp_client.c
	src/mrcp_client_session.c
	src/mrcp_application.c
)
source_group ("src" FILES ${MRCP_CLIENT_SOURCES})

# Library declaration
add_library (${PROJECT_NAME} OBJECT ${MRCP_CLIENT_SOURCES} ${MRCP_CLIENT_HEADERS})
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
	${MRCP_INCLUDE_DIRS}
	${MPF_INCLUDE_DIRS}
	${APR_TOOLKIT_INCLUDE_DIRS}
	${APR_INCLUDE_DIRS}
	${APU_INCLUDE_DIRS}
)
