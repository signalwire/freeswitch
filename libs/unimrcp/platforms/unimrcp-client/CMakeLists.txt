cmake_minimum_required (VERSION 2.8)
project (unimrcpclient-app)

set (PROJECT_OUTPUT_NAME unimrcpclient)

# Set header files
set (UNIMRCP_CLIENT_HEADERS
	include/demo_framework.h
	include/demo_application.h
	include/demo_util.h
)
source_group ("include" FILES ${UNIMRCP_CLIENT_HEADERS})

# Set source files
set (UNIMRCP_CLIENT_SOURCES
	src/main.c
	src/demo_framework.c
	src/demo_synth_application.c
	src/demo_recog_application.c
	src/demo_bypass_application.c
	src/demo_discover_application.c
	src/demo_util.c
)
source_group ("src" FILES ${UNIMRCP_CLIENT_SOURCES})

# Application declaration
add_executable (${PROJECT_NAME} ${UNIMRCP_CLIENT_SOURCES} ${UNIMRCP_CLIENT_HEADERS})
set_target_properties (${PROJECT_NAME} PROPERTIES FOLDER "platforms")
set_target_properties (${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${PROJECT_OUTPUT_NAME})

# Input libraries
target_link_libraries(${PROJECT_NAME} unimrcpclient)

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
	${UNIMRCP_CLIENT_INCLUDE_DIRS}
	${MRCP_CLIENT_INCLUDE_DIRS}
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
