cmake_minimum_required (VERSION 2.8)
project (mrcp)

# Set header files (include)
set (MRCP_INCLUDE_HEADERS
	include/mrcp.h
	include/mrcp_types.h
)
source_group ("include" FILES ${MRCP_INCLUDE_HEADERS})

# Set header files (control)
set (MRCP_CONTROL_HEADERS
	control/include/mrcp_resource.h
	control/include/mrcp_resource_factory.h
	control/include/mrcp_resource_loader.h
	control/include/mrcp_stream.h
)
source_group ("control\\include" FILES ${MRCP_CONTROL_HEADERS})

# Set source files (control)
set (MRCP_CONTROL_SOURCES
	control/src/mrcp_resource_factory.c
	control/src/mrcp_resource_loader.c
	control/src/mrcp_stream.c
)
source_group ("control\\src" FILES ${MRCP_CONTROL_SOURCES})

# Set header files (message)
set (MRCP_MESSAGE_HEADERS
	message/include/mrcp_start_line.h
	message/include/mrcp_header_accessor.h
	message/include/mrcp_generic_header.h
	message/include/mrcp_header.h
	message/include/mrcp_message.h
)
source_group ("message\\include" FILES ${MRCP_MESSAGE_HEADERS})

# Set source files (message)
set (MRCP_MESSAGE_SOURCES
	message/src/mrcp_start_line.c
	message/src/mrcp_header_accessor.c
	message/src/mrcp_generic_header.c
	message/src/mrcp_header.c
	message/src/mrcp_message.c
)
source_group ("message\\src" FILES ${MRCP_MESSAGE_SOURCES})

# Set header files (resources)
set (MRCP_RESOURCES_HEADERS
	resources/include/mrcp_synth_header.h
	resources/include/mrcp_synth_resource.h
	resources/include/mrcp_recog_header.h
	resources/include/mrcp_recog_resource.h
	resources/include/mrcp_recorder_header.h
	resources/include/mrcp_recorder_resource.h
	resources/include/mrcp_verifier_header.h
	resources/include/mrcp_verifier_resource.h
)
source_group ("resources\\include" FILES ${MRCP_RESOURCES_HEADERS})

# Set source files (resources)
set (MRCP_RESOURCES_SOURCES
	resources/src/mrcp_synth_header.c
	resources/src/mrcp_synth_resource.c
	resources/src/mrcp_recog_header.c
	resources/src/mrcp_recog_resource.c
	resources/src/mrcp_recorder_header.c
	resources/src/mrcp_recorder_resource.c
	resources/src/mrcp_verifier_header.c
	resources/src/mrcp_verifier_resource.c
)
source_group ("resources\\src" FILES ${MRCP_RESOURCES_SOURCES})

# Library declaration
add_library (${PROJECT_NAME} OBJECT
	${MRCP_CONTROL_SOURCES}
	${MRCP_MESSAGE_SOURCES}
	${MRCP_RESOURCES_SOURCES}
	${MRCP_INCLUDE_HEADERS}
	${MRCP_CONTROL_HEADERS}
	${MRCP_MESSAGE_HEADERS}
	${MRCP_RESOURCES_HEADERS}
)
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
	${PROJECT_SOURCE_DIR}/message/include
	${PROJECT_SOURCE_DIR}/control/include
	${PROJECT_SOURCE_DIR}/resources/include
	${APR_TOOLKIT_INCLUDE_DIRS}
	${APR_INCLUDE_DIRS}
	${APU_INCLUDE_DIRS}
)
