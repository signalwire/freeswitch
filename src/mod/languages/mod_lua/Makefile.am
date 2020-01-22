VERBOSE=1
include $(top_srcdir)/build/modmake.rulesam

MODNAME=mod_lua

AM_CFLAGS += $(CFLAGS) -D_GNU_SOURCE

noinst_LTLIBRARIES = libluamod.la
libluamod_la_SOURCES = mod_lua.cpp freeswitch_lua.cpp mod_lua_wrap.cpp
libluamod_la_CXXFLAGS = $(AM_CPPFLAGS) $(LUA_CFLAGS) $(CPPFLAGS)
libluamod_la_CFLAGS = $(AM_CPPFLAGS) $(LUA_CFLAGS) $(CPPFLAGS)

mod_LTLIBRARIES = mod_lua.la
mod_lua_la_SOURCES = mod_lua.cpp freeswitch_lua.cpp mod_lua_wrap.cpp

mod_lua_la_CXXFLAGS = $(AM_CPPFLAGS) $(LUA_CFLAGS) $(CPPFLAGS)
mod_lua_la_CFLAGS   = $(AM_CPPFLAGS) $(LUA_CFLAGS) $(CPPFLAGS)

mod_lua_la_LIBADD = $(switch_builddir)/libfreeswitch.la
mod_lua_la_LDFLAGS = -avoid-version -module -no-undefined -shared -lm $(AM_LDFLAGS) $(SOLINK) $(LUA_LIBS)

reswig: swigclean mod_lua_wrap.cpp

swigclean: clean
	rm -f mod_lua_wrap.*

mod_lua_wrap.cpp: mod_lua_extra.c
	swig3.0 -lua -c++ -I../../../../src/include -oh mod_lua_wrap.h -o mod_lua_wrap.cpp freeswitch.i
	echo "#include \"mod_lua_extra.c\"" >> mod_lua_wrap.cpp
	patch -s -p0 -i hack.diff

noinst_PROGRAMS = test/test_mod_lua

test_test_mod_lua_SOURCES = test/test_mod_lua.c
test_test_mod_lua_CFLAGS = $(AM_CFLAGS) -I. -DSWITCH_TEST_BASE_DIR_FOR_CONF=\"${abs_builddir}/test\" -DSWITCH_TEST_BASE_DIR_OVERRIDE=\"${abs_builddir}/test\"
test_test_mod_lua_LDFLAGS = $(AM_LDFLAGS) -avoid-version -no-undefined $(freeswitch_LDFLAGS) $(switch_builddir)/libfreeswitch.la $(CORE_LIBS) $(APR_LIBS)
test_test_mod_lua_LDADD = libluamod.la $(LUA_LIBS)

TESTS = $(noinst_PROGRAMS)

