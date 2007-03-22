# define these targets in your makefile if you wish
# local_all local_depend local_clean depend_install local_install local_distclean local_extraclean:

# and define these variables to impact your build

LVENGINE_DIR=/opt/lumenvox/engine_7.0
LOCAL_CFLAGS= -fpermissive -Wno-deprecated -Wno-conversion -fpermissive -Wno-unused
LOCAL_CFLAGS+= -Wno-comment -Wno-sign-compare -Wno-conversion -Wno-reorder -I$(LVINCLUDE)
LOCAL_LDFLAGS= -L$(LVLIB) -llv_lvspeechport
include ../../../../build/modmake.rules

