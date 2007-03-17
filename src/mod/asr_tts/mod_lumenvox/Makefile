# define these targets in your makefile if you wish
# local_all local_depend local_clean depend_install local_install local_distclean local_extraclean:

# and define these variables to impact your build

LOCAL_CFLAGS= -fpermissive -Wno-deprecated -Wno-conversion -fpermissive -Wno-unused -Wno-comment -Wno-sign-compare \
	-Wno-conversion -Wno-reorder -I/opt/lumenvox/eng\ine_7.0/include
LOCAL_LDFLAGS= -L/opt/lumenvox/engine_7.0/lib -llv_lvspeechport
LOCAL_OBJS=
include ../../../../build/modmake.rules

