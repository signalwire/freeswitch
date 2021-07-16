cmake_minimum_required (VERSION 2.8)
project (aprtoolkit)

# Set header files
set (APR_TOOLKIT_HEADERS
	include/apt.h
	include/apt_obj_list.h
	include/apt_cyclic_queue.h
	include/apt_dir_layout.h
	include/apt_task.h
	include/apt_task_msg.h
	include/apt_consumer_task.h
	include/apt_pollset.h
	include/apt_poller_task.h
	include/apt_pool.h
	include/apt_log.h
	include/apt_pair.h
	include/apt_string.h
	include/apt_string_table.h
	include/apt_header_field.h
	include/apt_text_stream.h
	include/apt_text_message.h
	include/apt_net.h
	include/apt_nlsml_doc.h
	include/apt_multipart_content.h
	include/apt_timer_queue.h
	include/apt_test_suite.h
)
source_group ("include" FILES ${APR_TOOLKIT_HEADERS})

# Set source files
set (APR_TOOLKIT_SOURCES
	src/apt_obj_list.c
	src/apt_cyclic_queue.c
	src/apt_dir_layout.c
	src/apt_task.c
	src/apt_task_msg.c
	src/apt_consumer_task.c
	src/apt_pollset.c
	src/apt_poller_task.c
	src/apt_pool.c
	src/apt_log.c
	src/apt_pair.c
	src/apt_string_table.c
	src/apt_header_field.c
	src/apt_text_stream.c
	src/apt_text_message.c
	src/apt_net.c
	src/apt_nlsml_doc.c
	src/apt_multipart_content.c
	src/apt_timer_queue.c
	src/apt_test_suite.c
)
source_group ("src" FILES ${APR_TOOLKIT_SOURCES})

# Library declaration
add_library (${PROJECT_NAME} OBJECT ${APR_TOOLKIT_SOURCES} ${APR_TOOLKIT_HEADERS})
set_target_properties (${PROJECT_NAME} PROPERTIES FOLDER "libs")

# Preprocessor definitions
add_definitions (
	${APR_TOOLKIT_DEFINES} 
	${APR_DEFINES} 
	${APU_DEFINES}
)

# Include directories
include_directories (
	${PROJECT_SOURCE_DIR}/include
	${APR_INCLUDE_DIRS}
	${APU_INCLUDE_DIRS}
)
