UNAME := $(shell uname -s)
ifeq ($(UNAME),SunOS)
ISA64 := $(shell isainfo -n)
LOCAL_CFLAGS=-I/usr/postgres/8.3/include
ifneq (,$(findstring m64,$(CFLAGS)))
LOCAL_LDFLAGS=-L/usr/postgres/8.3/lib/$(ISA64) -R/usr/postgres/8.3/lib/$(ISA64) -lpq -static
else 
LOCAL_LDFLAGS=-L/usr/postgres/8.3/lib -R/usr/postgres/8.3/lib -lpq -static
endif
else
LOCAL_LDFLAGS=-lpq -static
endif
include ../../../../build/modmake.rules
