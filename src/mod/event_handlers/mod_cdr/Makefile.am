#we should set all these vars from configure, no reason to have these in each Makefile.am
#LIBTOOL = compile=`echo $<|grep .c`;link=`echo $@|grep .la;echo $@|grep .so;echo $@|grep .dll`; if test -n "$$compile"; then echo Compiling $<;fi; if test -n "$$link"; then echo Creating $@;fi; `if test -z "$(VERBOSE)" ; then echo $(SHELL) $(switch_builddir)/quiet_libtool ;else echo $(switch_builddir)/libtool;  fi;`
#LIBTOOL=`if test -z "$(VERBOSE)" ; then echo $(SHELL) $(switch_builddir)/quiet_libtool ;else echo $(switch_builddir)/libtool;  fi;`
LIBTOOL = 	link=`echo $@|grep .la;echo $@|grep .so;echo $@|grep .dll`; \
		if test -n "$$link"; then echo Creating $@;fi; \
		`if test -z "$(VERBOSE)" ; \
		then echo $(SHELL) $(switch_builddir)/quiet_libtool ;\
		else echo $(switch_builddir)/libtool;  fi;`

AM_MAKEFLAGS=`test -n "$(VERBOSE)" || echo -s`

# Dirty trick to override the link output
LIBS+=> $(MODNAME).log || error="yes" ; \
			if test -n "$(VERBOSE)" -o "$$error" = "yes"; then \
			  cat $(MODNAME).log ; \
			fi ;\
			if test "$$error" = "yes"; then \
			  exit 1 ;\
			fi

moddir=$(prefix)/mod

MODNAME=mod_cdr
mod_LTLIBRARIES = mod_cdr.la
mod_cdr_la_SOURCES = mod_cdr.cpp cdrcontainer.cpp basecdr.cpp baseregistry.cpp pddcdr.cpp csvcdr.cpp xmlcdr.cpp sqlitecdr.cpp
mod_cdr_la_CFLAGS = $(AM_CFLAGS)
mod_cdr_la_LIBADD=$(switch_builddir)/libfreeswitch.la
mod_cdr_la_LDFLAGS=-module -avoid-version -no-undefined -export-symbols-regex ^switch_module_.*$ -rpath $(PREFIX)/$(libdir)

#Build mysqlcdr if we have mysql client
if HAVE_MYSQL
mod_cdr_la_CFLAGS += -DMYSQL
mod_cdr_la_LDFLAGS += -lmysql-client
mod_cdr_la_SOURCES += mysqlcdr.cpp
endif

#mod_cdr_la_CFLAGS  += -DSWITCH_QUEUE_ENHANCED
#mod_cdr_la_LDFLAGS += -lcurl

#Override the linstall target so we just install the .so/.dylib

install-data-am: $(DESTDIR)$(PREFIX)/$(moddir)/$(MODNAME).$(DYNAMIC_LIB_EXTEN)

$(DESTDIR)$(PREFIX)/$(moddir)/$(MODNAME).$(DYNAMIC_LIB_EXTEN): $(MODNAME).la
	@echo installing $(MODNAME).$(DYNAMIC_LIB_EXTEN)
	@if [ -f .libs/$(MODNAME).$(DYNAMIC_LIB_EXTEN) ] ; then \
	  $(LIBTOOL) --mode=install $(INSTALL) .libs/$(MODNAME).$(DYNAMIC_LIB_EXTEN) $(DESTDIR)$(PREFIX)/$(moddir) >/dev/null  ; \
	else \
	  $(LIBTOOL) --mode=install $(INSTALL) $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(DESTDIR)$(PREFIX)/$(moddir) >/dev/null  ; \
	fi
