# define these targets in your makefile if you wish
# local_all local_depend local_clean depend_install local_install local_distclean local_extraclean:

# and define these variables to impact your build

LOCAL_CFLAGS=
LOCAL_LDFLAGS=
LOCAL_OBJS=cdrcontainer.o basecdr.o baseregistry.o pddcdr.o csvcdr.o xmlcdr.o sqlitecdr.o

#LOCAL_CFLAGS  += -DSWITCH_QUEUE_ENHANCED
#LOCAL_LDFLAGS += -lcurl

LOCAL_INSERT_CFLAGS=mysql_config="\`which mysql-config 2>/dev/null \`"; if test -n "$$mysql_config"; then echo "\`mysql_config --include\`" ; fi 
LOCAL_INSERT_LDFLAGS=mysql_config="\`which mysql-config 2>/dev/null \`"; if test -n "$$mysql_config"; then echo "\`mysql_config --libs\`" ; fi 
LOCAL_INSERT_OBJS=mysql_config="\`which mysql-config 2>/dev/null \`"; if test -n "$$mysql_config"; then echo "mysqlcdr.o" ; fi 

include ../../../../build/modmake.rules
