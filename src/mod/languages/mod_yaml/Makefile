BASE=../../../..
YAMLLIB=$(switch_srcdir)/libs/yaml
YAMLBUILDLIB=$(switch_builddir)/libs/yaml
LIBYAML_A=$(YAMLBUILDLIB)/src/.libs/libyaml.a
LOCAL_CFLAGS=-I$(YAMLLIB)/include
LOCAL_LIBADD=$(LIBYAML_A)
VERBOSE=1

include $(BASE)/build/modmake.rules

local_depend: $(LIBYAML_A)

$(LIBYAML_A):
	cd $(YAMLBUILDLIB) && $(DEFAULT_VARS) ./configure $(DEFAULT_ARGS)
	cd $(YAMLBUILDLIB) && $(MAKE)

yamlclean:
	cd $(YAMLBUILDLIB) && $(MAKE) clean

