BASE=../../../..
LOCAL_CFLAGS=-Ilua
LIBLUA_A=lua/liblua.a
LOCAL_LIBADD=$(LIBLUA_A)
LOCAL_LDFLAGS=-lm
LOCAL_OBJS=freeswitch_lua.o mod_lua_wrap.o #glua.o
VERBOSE=1

include $(BASE)/build/modmake.rules

glua.o: mod_lua_wrap.cpp

$(LIBLUA_A):
	cd lua && $(MAKE) CFLAGS="$(ALL_CFLAGS) -DLUA_USE_LINUX -w" liblua.a

allclean: clean
	cd lua && $(MAKE) clean
	rm -f mod_lua_wrap.* freeswitch_lua.$(DYNAMIC_LIB_EXTEN)

swigclean:
	rm mod_lua_wrap.*

mod_lua_wrap.cpp:
	swig -lua -c++ -I../../../../src/include -oh mod_lua_wrap.h -o mod_lua_wrap.cpp freeswitch.i

freeswitch_lua.$(DYNAMIC_LIB_EXTEN): $(LOCAL_OBJS) $(LOCAL_LIBADD)
	$(LINK) $(SOLINK) -o freeswitch_lua.$(DYNAMIC_LIB_EXTEN)  $(LOCAL_OBJS) $(LOCAL_LIBADD) $(LDFLAGS)
