# define these targets in your makefile if you wish
# local_all local_depend local_clean depend_install local_install local_distclean local_extraclean:

# and define these variables to impact your build
LOCAL_CFLAGS=
LOCAL_LDFLAGS=
LOCAL_OBJS=cdrcontainer.o basecdr.o baseregistry.o pddcdr.o csvcdr.o xmlcdr.o sqlitecdr.o

#LOCAL_CFLAGS  += -DSWITCH_QUEUE_ENHANCED
#LOCAL_LDFLAGS += -lcurl

ifneq ($(shell which mysql_config),)
  LOCAL_CFLAGS  += $(shell mysql_config --include)
  LOCAL_LDFLAGS += $(shell mysql_config --libs)
  LOCAL_OBJS    += mysqlcdr.o
endif

include $(BASE)/build/modmake.rules
