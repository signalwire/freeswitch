cmake_minimum_required (VERSION 2.8)
project (asrclient)

# Set header files
set (ASR_CLIENT_HEADERS
	include/asr_engine.h
	include/asr_engine_common.h
)
source_group ("include" FILES ${ASR_CLIENT_HEADERS})

# Set source files
set (ASR_CLIENT_SOURCES
	src/asr_engine.c
)
source_group ("src" FILES ${ASR_CLIENT_SOURCES})

# Library declaration
add_library (${PROJECT_NAME} SHARED ${ASR_CLIENT_SOURCES} ${ASR_CLIENT_HEADERS})
set_target_properties (${PROJECT_NAME} PROPERTIES FOLDER "platforms")

# Input libraries
target_link_libraries(${PROJECT_NAME} unimrcpclient)

# Preprocessor definitions
set (ASR_CLIENT_DEFINES -DASR_CLIENT_LIB_EXPORT)
add_definitions (
	${ASR_CLIENT_DEFINES}
	${MRCP_DEFINES}
	${MPF_DEFINES}
	${APR_TOOLKIT_DEFINES} 
	${APR_DEFINES} 
	${APU_DEFINES}
)

# Include directories
include_directories (
	${PROJECT_SOURCE_DIR}/include
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
install (TARGETS ${PROJECT_NAME}
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
)
if (MSVC)
	install (FILES ${PROJECT_BINARY_DIR}/Debug/${PROJECT_NAME}.pdb DESTINATION bin CONFIGURATIONS Debug)
	install (FILES ${PROJECT_BINARY_DIR}/RelWithDebInfo/${PROJECT_NAME}.pdb DESTINATION bin CONFIGURATIONS RelWithDebInfo)
endif (MSVC)
