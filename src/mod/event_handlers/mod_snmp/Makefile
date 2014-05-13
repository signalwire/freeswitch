include ../../../../build/modmake.rules

LOCAL_CFLAGS=`net-snmp-config --cflags`
LOCAL_LDFLAGS=-lnetsnmpmibs -lnetsnmpagent -lnetsnmp
LOCAL_OBJS=subagent.o

local_depend: $(LOCAL_OBJS)
