BASE=../../../..
LOCAL_CFLAGS=-Ilua
LIBLUA_A=lua/liblua.a
LOCAL_LIBADD=$(LIBLUA_A)
LOCAL_LDFLAGS=-lm
LOCAL_OBJS=freeswitch_lua.o mod_lua_wrap.o
VERBOSE=1

include $(BASE)/build/modmake.rules

local_depend: $(LOCAL_OBJS)

$(LIBLUA_A):
	cd lua && $(MAKE) CC="$(CC)" CFLAGS="$(ALL_CFLAGS) -DLUA_USE_LINUX -w" liblua.a

luaclean:
	cd lua && $(MAKE) clean

freeswitch_lua.o: freeswitch_lua.h

allclean: clean luaclean
	rm -f mod_lua_wrap.* freeswitch_lua.$(DYNAMIC_LIB_EXTEN)

swigclean: clean
	rm -f mod_lua_wrap.*

mod_lua_wrap.cpp: mod_lua_extra.c
	swig -lua -c++ -I../../../../src/include -oh mod_lua_wrap.h -o mod_lua_wrap.cpp freeswitch.i
	echo "#include \"mod_lua_extra.c\"" >> mod_lua_wrap.cpp
	patch -s -p0 -i hack.diff

freeswitch_lua.$(DYNAMIC_LIB_EXTEN): $(LOCAL_OBJS) $(LOCAL_LIBADD)
	$(LINK) $(SOLINK) -o freeswitch_lua.$(DYNAMIC_LIB_EXTEN)  $(LOCAL_OBJS) $(LOCAL_LIBADD) $(LDFLAGS)
