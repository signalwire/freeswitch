cmake_minimum_required (VERSION 2.8)
project (mrcpsignaling)

# Set header files
set (MRCP_SIGNALING_HEADERS
	include/mrcp_sig_types.h
	include/mrcp_sig_agent.h
	include/mrcp_session.h
	include/mrcp_session_descriptor.h
)
source_group ("include" FILES ${MRCP_SIGNALING_HEADERS})

# Set source files
set (MRCP_SIGNALING_SOURCES
	src/mrcp_sig_agent.c
	src/mrcp_session_descriptor.c
)
source_group ("src" FILES ${MRCP_SIGNALING_SOURCES})

# Library declaration
add_library (${PROJECT_NAME} OBJECT ${MRCP_SIGNALING_SOURCES} ${MRCP_SIGNALING_HEADERS})
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
	${MRCP_INCLUDE_DIRS}
	${MPF_INCLUDE_DIRS}
	${APR_TOOLKIT_INCLUDE_DIRS}
	${APR_INCLUDE_DIRS}
	${APU_INCLUDE_DIRS}
)
