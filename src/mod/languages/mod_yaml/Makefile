BASE=../../../..
YAMLLIB=$(BASE)/libs/yaml
LIBYAML_A=$(YAMLLIB)/src/.libs/libyaml.a
LOCAL_CFLAGS=-I$(YAMLLIB)/include
LOCAL_LIBADD=$(LIBYAML_A)
VERBOSE=1

include $(BASE)/build/modmake.rules

local_depend: $(LIBYAML_A)

$(LIBYAML_A):
	cd $(YAMLLIB) && $(DEFAULT_VARS) ./configure $(DEFAULT_ARGS)
	cd $(YAMLLIB) && $(MAKE)

yamlclean:
	cd $(YAMLLIB) && $(MAKE) clean

