MAINTAINERCLEANFILES 	 = Makefile.in

INCLUDES                 = -I$(top_srcdir)/libs/apr-toolkit/include \
                           $(UNIMRCP_APR_INCLUDES) $(UNIMRCP_APU_INCLUDES)

noinst_LTLIBRARIES       = libaprtoolkit.la

include_HEADERS          = include/apt.h \
                           include/apt_obj_list.h \
                           include/apt_cyclic_queue.h \
                           include/apt_task.h \
                           include/apt_task_msg.h \
                           include/apt_consumer_task.h \
                           include/apt_net_server_task.h \
                           include/apt_net_client_task.h \
                           include/apt_pollset.h \
                           include/apt_pool.h \
                           include/apt_log.h \
                           include/apt_pair.h \
                           include/apt_string.h \
                           include/apt_string_table.h \
                           include/apt_text_stream.h \
                           include/apt_net.h \
                           include/apt_nlsml_doc.h \
                           include/apt_dir_layout.h \
                           include/apt_test_suite.h

libaprtoolkit_la_SOURCES = src/apt_obj_list.c \
                           src/apt_cyclic_queue.c \
                           src/apt_task.c \
                           src/apt_task_msg.c \
                           src/apt_consumer_task.c \
                           src/apt_net_server_task.c \
                           src/apt_net_client_task.c \
                           src/apt_pollset.c \
                           src/apt_pool.c \
                           src/apt_log.c \
                           src/apt_pair.c \
                           src/apt_string_table.c \
                           src/apt_text_stream.c \
                           src/apt_net.c \
                           src/apt_nlsml_doc.c \
                           src/apt_dir_layout.c \
                           src/apt_test_suite.c
                           
