cmake_minimum_required (VERSION 2.8)
project (mrcptest)

# Set source files
set (MRCP_TEST_SOURCES
	src/main.c
	src/parse_gen_suite.c
	src/set_get_suite.c
	src/transparent_set_get_suite.c
)
source_group ("src" FILES ${MRCP_TEST_SOURCES})

# Application declaration
add_executable (${PROJECT_NAME} ${MRCP_TEST_SOURCES}
	$<TARGET_OBJECTS:mrcp>
	$<TARGET_OBJECTS:aprtoolkit>
)
set_target_properties (${PROJECT_NAME} PROPERTIES FOLDER "tests")

# Input libraries
target_link_libraries(${PROJECT_NAME} 
	${APU_LIBRARIES}
	${APR_LIBRARIES}
)
# Input system libraries
if (WIN32)
	target_link_libraries(${PROJECT_NAME} ws2_32 winmm)
elseif (UNIX)
	target_link_libraries(${PROJECT_NAME} m)
endif ()

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
