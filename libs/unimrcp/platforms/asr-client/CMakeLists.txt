cmake_minimum_required (VERSION 2.8)
project (asrclient-app)

set (PROJECT_OUTPUT_NAME asrclient)

# Set source files
set (ASR_CLIENT_SOURCES
	src/main.c
)
source_group ("src" FILES ${ASR_CLIENT_SOURCES})

# Application declaration
add_executable (${PROJECT_NAME} ${ASR_CLIENT_SOURCES})
set_target_properties (${PROJECT_NAME} PROPERTIES FOLDER "platforms")
set_target_properties (${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${PROJECT_OUTPUT_NAME})

# Input libraries
target_link_libraries(${PROJECT_NAME} asrclient)
  
# Preprocessor definitions
add_definitions (
	${APR_DEFINES} 
	${APU_DEFINES}
)

# Include directories
include_directories (
	${PROJECT_SOURCE_DIR}/include
	${PROJECT_SOURCE_DIR}/../libasr-client/include
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
