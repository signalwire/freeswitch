include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_yaml

YAMLLIB=$(switch_srcdir)/libs/yaml
YAMLBUILDLIB=$(switch_builddir)/libs/yaml
LIBYAML_A=$(YAMLBUILDLIB)/src/.libs/libyaml.a

mod_LTLIBRARIES = mod_yaml.la
mod_yaml_la_SOURCES  = mod_yaml.c
mod_yaml_la_CFLAGS   = $(AM_CFLAGS)
mod_yaml_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(YAMLLIB)/include
mod_yaml_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(LIBYAML_A)
mod_yaml_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

BUILT_SOURCES=$(LIBYAML_A)

$(LIBYAML_A):
	mkdir -p $(YAMLBUILDLIB)
	cd $(YAMLBUILDLIB) && $(DEFAULT_VARS) $(YAMLLIB)/configure $(DEFAULT_ARGS) --srcdir=$(YAMLLIB) --with-pic
	cd $(YAMLBUILDLIB) && $(MAKE)

yamlclean:
	cd $(YAMLBUILDLIB) && $(MAKE) clean

