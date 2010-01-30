VERBOSE=1
include $(top_srcdir)/build/modmake.rulesam

MODNAME=mod_lua

LIBLUA_A=lua/liblua.a

AM_CFLAGS += $(CFLAGS) -D_GNU_SOURCE
mod_LTLIBRARIES = mod_lua.la
mod_lua_la_SOURCES = mod_lua.cpp freeswitch_lua.cpp mod_lua_wrap.cpp
mod_lua_la_CXXFLAGS = -Ilua $(AM_CPPFLAGS)
mod_lua_la_LIBADD = $(switch_builddir)/libfreeswitch.la 
mod_lua_la_LDFLAGS = -avoid-version -module -no-undefined -shared -lm $(AM_LDFLAGS) $(LIBLUA_A) $(SOLINK)

BUILT_SOURCES = $(LIBLUA_A)
$(mod_lua_la_SOURCES) : $(BUILT_SOURCES)

$(LIBLUA_A):
	cd lua && $(MAKE) CC="$(CC)" AR="$(AR) rcu" CFLAGS="$(AM_CFLAGS) -DLUA_USE_LINUX -w" liblua.a

reswig: swigclean lua_wrap

luaclean:
	cd lua && $(MAKE) clean

allclean: clean luaclean

swigclean: clean
	rm -f mod_lua_wrap.*

lua_wrap: mod_lua_extra.c
	swig -lua -c++ -I../../../../src/include -oh mod_lua_wrap.h -o mod_lua_wrap.cpp freeswitch.i
	echo "#include \"mod_lua_extra.c\"" >> mod_lua_wrap.cpp
	patch -s -p0 -i hack.diff

