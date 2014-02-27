include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_mongo

MONGO_CXX_DRIVER_VERSION=v2.4
MONGO_CXX_DRIVER_URL=http://downloads.mongodb.org/cxx-driver
MONGO_CXX_DRIVER_TARBALL=mongodb-linux-x86_64-$(MONGO_CXX_DRIVER_VERSION)-latest.tgz
MONGO_CXX_DRIVER_SRC=$(switch_builddir)/libs/mongo-cxx-driver-$(MONGO_CXX_DRIVER_VERSION)
LIBMONGOCLIENT_A =$(MONGO_CXX_DRIVER_SRC)/libmongoclient.a

mod_LTLIBRARIES = mod_mongo.la
mod_mongo_la_SOURCES  = mod_mongo.cpp
mod_mongo_la_CFLAGS   = $(AM_CFLAGS)
mod_mongo_la_CPPFLAGS = -I$(MONGO_CXX_DRIVER_SRC)/src  -I$(switch_srcdir)/libs/stfu/ -I$(switch_srcdir)/libs/libteletone/src/                                   
mod_mongo_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(LIBMONGOCLIENT_A)
mod_mongo_la_LDFLAGS  = -avoid-version -module -no-undefined -shared -lboost_thread -lboost_filesystem-mt -lboost_system-mt

BUILT_SOURCES=$(LIBMONGOCLIENT_A)

$(MONGO_CXX_DRIVER_SRC):
	$(GETLIB) $(MONGO_CXX_DRIVER_URL) $(MONGO_CXX_DRIVER_TARBALL)
	cd $(MONGO_CXX_DRIVER_SRC) && patch -p0 -i $(switch_srcdir)/src/mod/applications/mod_mongo/fpic_hack.diff
	$(TOUCH_TARGET)


$(LIBMONGOCLIENT_A): $(MONGO_CXX_DRIVER_SRC)
	(cd $(MONGO_CXX_DRIVER_SRC) && \
		sed -i'' SConstruct -e "s|Environment()|Environment(ENV = {'PATH' : os.environ.get('PATH','/bin:/usr/bin'),'CFLAGS' : os.environ.get('CFLAGS',''),'CXXFLAGS' : os.environ.get('CXXFLAGS','')})|" && \
		echo "env.Append(PATH=os.environ.get('PATH','/bin:/usr/bin'))" >> SConstruct && \
		scons)
	$(TOUCH_TARGET)

