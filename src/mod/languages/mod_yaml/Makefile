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
	mkdir -p $(YAMLBUILDLIB)
	cd $(YAMLBUILDLIB) && $(DEFAULT_VARS) $(YAMLLIB)/configure $(DEFAULT_ARGS) --srcdir=$(YAMLLIB)
	cd $(YAMLBUILDLIB) && $(MAKE)

yamlclean:
	cd $(YAMLBUILDLIB) && $(MAKE) clean

