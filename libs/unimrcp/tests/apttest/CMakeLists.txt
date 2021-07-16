cmake_minimum_required (VERSION 2.8)
project (apttest)

# Set source files
set (APT_TEST_SOURCES
	src/main.c
	src/task_suite.c
	src/consumer_task_suite.c
	src/multipart_suite.c
)
source_group ("src" FILES ${APT_TEST_SOURCES})

# Application declaration
add_executable (${PROJECT_NAME} ${APT_TEST_SOURCES}
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
	${APR_TOOLKIT_DEFINES}
	${APR_DEFINES}
	${APU_DEFINES}
)

# Include directories
include_directories (
	${PROJECT_SOURCE_DIR}/include
	${APR_TOOLKIT_INCLUDE_DIRS}
	${APR_INCLUDE_DIRS}
	${APU_INCLUDE_DIRS}
)
