include ../../../../build/modmake.rules

MONGODB_DRIVER=./driver/src
LOCAL_CFLAGS=-I$(MONGODB_DRIVER)
LOCAL_OBJS=$(MONGODB_DRIVER)/encoding.o $(MONGODB_DRIVER)/env_posix.o \
	   $(MONGODB_DRIVER)/bson.o $(MONGODB_DRIVER)/md5.o \
	   $(MONGODB_DRIVER)/mongo.o $(MONGODB_DRIVER)/numbers.o

local_depend: $(LOCAL_OBJS)
