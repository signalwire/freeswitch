cmake_minimum_required (VERSION 2.8)
project (mrcpsofiasip)

# Set header files
set (MRCP_SOFIASIP_HEADERS
	include/mrcp_sdp.h
	include/mrcp_sofiasip_task.h
	include/mrcp_sofiasip_logger.h
	include/mrcp_sofiasip_server_agent.h
	include/mrcp_sofiasip_client_agent.h
)
source_group ("include" FILES ${MRCP_SOFIASIP_HEADERS})

# Set source files
set (MRCP_SOFIASIP_SOURCES
	src/mrcp_sdp.c
	src/mrcp_sofiasip_task.c
	src/mrcp_sofiasip_server_agent.c
	src/mrcp_sofiasip_client_agent.c
	src/mrcp_sofiasip_logger.c
)
source_group ("src" FILES ${MRCP_SOFIASIP_SOURCES})

# Library declaration
add_library (${PROJECT_NAME} OBJECT ${MRCP_SOFIASIP_SOURCES} ${MRCP_SOFIASIP_HEADERS})
set_target_properties (${PROJECT_NAME} PROPERTIES FOLDER "modules")

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
	${SOFIA_INCLUDE_DIRS}
)
