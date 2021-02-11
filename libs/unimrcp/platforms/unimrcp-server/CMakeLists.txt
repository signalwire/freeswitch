cmake_minimum_required (VERSION 2.8)
project (unimrcpserver-app)

set (PROJECT_OUTPUT_NAME unimrcpserver)

# Set source files
set (UNIMRCP_SERVER_SOURCES
	src/main.c
	src/uni_cmdline.c
	src/uni_daemon.c
)
if (WIN32)
	set (UNIMRCP_SERVER_SOURCES ${UNIMRCP_SERVER_SOURCES} src/uni_service.c)
endif (WIN32)
source_group ("src" FILES ${UNIMRCP_SERVER_SOURCES})

# Application declaration
add_executable (${PROJECT_NAME} ${UNIMRCP_SERVER_SOURCES})
set_target_properties (${PROJECT_NAME} PROPERTIES FOLDER "platforms")
set_target_properties (${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${PROJECT_OUTPUT_NAME})

# Input libraries
target_link_libraries(${PROJECT_NAME} unimrcpserver)

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
	${VERSION_INCLUDE_DIRS}
	${UNIMRCP_SERVER_INCLUDE_DIRS}
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
install (TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
if (MSVC)
	install (FILES ${PROJECT_BINARY_DIR}/Debug/${PROJECT_OUTPUT_NAME}.pdb DESTINATION bin CONFIGURATIONS Debug)
	install (FILES ${PROJECT_BINARY_DIR}/RelWithDebInfo/${PROJECT_OUTPUT_NAME}.pdb DESTINATION bin CONFIGURATIONS RelWithDebInfo)
endif (MSVC)
