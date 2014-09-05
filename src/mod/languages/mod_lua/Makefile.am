VERBOSE=1
include $(top_srcdir)/build/modmake.rulesam

MODNAME=mod_lua

LUA_DIR=$(switch_srcdir)/src/mod/languages/mod_lua/lua
LIBLUA_A=$(LUA_DIR)/liblua.a

AM_CFLAGS += $(CFLAGS) -D_GNU_SOURCE
mod_LTLIBRARIES = mod_lua.la
mod_lua_la_SOURCES = mod_lua.cpp freeswitch_lua.cpp mod_lua_wrap.cpp lua/lapi.c lua/lcode.c lua/lctype.c lua/ldebug.c lua/ldo.c lua/ldump.c lua/lfunc.c lua/lgc.c lua/llex.c lua/lmem.c lua/lobject.c lua/lopcodes.c lua/lparser.c lua/lstate.c lua/lstring.c lua/ltable.c lua/ltm.c lua/lundump.c lua/lvm.c lua/lzio.c lua/lauxlib.c lua/lbaselib.c lua/lbitlib.c lua/lcorolib.c lua/ldblib.c lua/liolib.c lua/lmathlib.c lua/loslib.c lua/lstrlib.c lua/ltablib.c lua/loadlib.c lua/linit.c
mod_lua_la_CXXFLAGS = -I$(LUA_DIR) $(AM_CPPFLAGS) -DLUA_USE_LINUX
mod_lua_la_CFLAGS = -I$(LUA_DIR) $(AM_CPPFLAGS) -DLUA_USE_LINUX
mod_lua_la_LIBADD = $(switch_builddir)/libfreeswitch.la 
mod_lua_la_LDFLAGS = -avoid-version -module -no-undefined -shared -lm $(AM_LDFLAGS) $(SOLINK) #$(LIBLUA_A)

#BUILT_SOURCES = $(LIBLUA_A)
#$(mod_lua_la_SOURCES) : $(BUILT_SOURCES)
#
#$(LIBLUA_A):
#	cd $(LUA_DIR) && $(MAKE) CC="$(CC)" AR="$(AR) rcu" CFLAGS="$(AM_CFLAGS) -DLUA_USE_LINUX -w" liblua.a
#luaclean:
#	cd $(LUA_DIR) && $(MAKE) clean
#
#allclean: clean luaclean

reswig: swigclean lua_wrap

swigclean: clean
	rm -f mod_lua_wrap.*

lua_wrap: mod_lua_extra.c
	swig -lua -c++ -I../../../../src/include -oh mod_lua_wrap.h -o mod_lua_wrap.cpp freeswitch.i
	echo "#include \"mod_lua_extra.c\"" >> mod_lua_wrap.cpp
	patch -s -p0 -i hack.diff

