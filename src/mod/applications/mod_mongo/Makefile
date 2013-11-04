BASE=../../../..

MONGO_CXX_DRIVER_VERSION=v2.4
MONGO_CXX_DRIVER_URL=http://downloads.mongodb.org/cxx-driver
MONGO_CXX_DRIVER_TARBALL=mongodb-linux-x86_64-$(MONGO_CXX_DRIVER_VERSION)-latest.tgz
MONGO_CXX_DRIVER_SRC=$(BASE)/libs/mongo-cxx-driver-$(MONGO_CXX_DRIVER_VERSION)
LIBMONGOCLIENT_A =$(MONGO_CXX_DRIVER_SRC)/libmongoclient.a

LOCAL_SOURCES=mongo_conn.cpp
LOCAL_OBJS=mongo_conn.o

LOCAL_CFLAGS=-I$(MONGO_CXX_DRIVER_SRC)/src
LOCAL_LIBADD=$(LIBMONGOCLIENT_A)
LOCAL_LDFLAGS=-lboost_thread -lboost_filesystem-mt -lboost_system-mt
MODDIR=$(shell pwd)


include $(BASE)/build/modmake.rules

$(MONGO_CXX_DRIVER_SRC):
	$(GETLIB) $(MONGO_CXX_DRIVER_URL) $(MONGO_CXX_DRIVER_TARBALL)
	cd $(MONGO_CXX_DRIVER_SRC) && patch -p0 -i $(MODDIR)/fpic_hack.diff
	$(TOUCH_TARGET)


$(LIBMONGOCLIENT_A): $(MONGO_CXX_DRIVER_SRC)
	(cd $(MONGO_CXX_DRIVER_SRC) && \
		sed -i SConstruct -e "s|Environment()|Environment(ENV = {'PATH' : os.environ.get('PATH','/bin:/usr/bin'),'CFLAGS' : os.environ.get('CFLAGS',''),'CXXFLAGS' : os.environ.get('CXXFLAGS','')})|" && \
		echo "env.Append(PATH=os.environ.get('PATH','/bin:/usr/bin'))" >> SConstruct && \
		scons)
	$(TOUCH_TARGET)

