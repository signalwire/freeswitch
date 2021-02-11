cmake_minimum_required (VERSION 2.8)
project (mrcpv2transport)

# Set header files
set (MRCPv2_TRANSPORT_HEADERS
	include/mrcp_connection_types.h
	include/mrcp_control_descriptor.h
	include/mrcp_connection.h
	include/mrcp_client_connection.h
	include/mrcp_server_connection.h
	include/mrcp_ca_factory.h
)
source_group ("include" FILES ${MRCPv2_TRANSPORT_HEADERS})

# Set source files
set (MRCPv2_TRANSPORT_SOURCES
	src/mrcp_control_descriptor.c
	src/mrcp_connection.c
	src/mrcp_client_connection.c
	src/mrcp_server_connection.c
	src/mrcp_ca_factory.c
)
source_group ("src" FILES ${MRCPv2_TRANSPORT_SOURCES})

# Library declaration
add_library (${PROJECT_NAME} OBJECT ${MRCPv2_TRANSPORT_SOURCES} ${MRCPv2_TRANSPORT_HEADERS})
set_target_properties (${PROJECT_NAME} PROPERTIES FOLDER "libs")

# Preprocessor definitions
add_definitions (
	${MRCP_DEFINES}
	${APR_TOOLKIT_DEFINES} 
	${APR_DEFINES} 
	${APU_DEFINES}
)

# Include directories
include_directories (
	${PROJECT_SOURCE_DIR}/include
	${MRCP_INCLUDE_DIRS}
	${APR_TOOLKIT_INCLUDE_DIRS}
	${APR_INCLUDE_DIRS}
	${APU_INCLUDE_DIRS}
)
