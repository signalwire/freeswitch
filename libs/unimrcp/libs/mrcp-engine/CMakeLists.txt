cmake_minimum_required (VERSION 2.8)
project (mrcpengine)

# Set header files
set (MRCP_ENGINE_HEADERS
	include/mrcp_engine_types.h
	include/mrcp_engine_plugin.h
	include/mrcp_engine_iface.h
	include/mrcp_engine_impl.h
	include/mrcp_synth_engine.h
	include/mrcp_recog_engine.h
	include/mrcp_recorder_engine.h
	include/mrcp_verifier_engine.h
	include/mrcp_resource_engine.h
	include/mrcp_engine_factory.h
	include/mrcp_engine_loader.h
	include/mrcp_state_machine.h
	include/mrcp_synth_state_machine.h
	include/mrcp_recog_state_machine.h
	include/mrcp_recorder_state_machine.h
	include/mrcp_verifier_state_machine.h
)
source_group ("include" FILES ${MRCP_ENGINE_HEADERS})

# Set source files
set (MRCP_ENGINE_SOURCES
	src/mrcp_engine_iface.c
	src/mrcp_engine_impl.c
	src/mrcp_engine_factory.c
	src/mrcp_engine_loader.c
	src/mrcp_synth_state_machine.c
	src/mrcp_recog_state_machine.c
	src/mrcp_recorder_state_machine.c
	src/mrcp_verifier_state_machine.c
)
source_group ("src" FILES ${MRCP_ENGINE_SOURCES})

# Library declaration
add_library (${PROJECT_NAME} OBJECT ${MRCP_ENGINE_SOURCES} ${MRCP_ENGINE_HEADERS})
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
