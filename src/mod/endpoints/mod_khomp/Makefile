MODNAME := mod_khomp
VERBOSE := 1

#FreeSWITCH source PATH is needed: 
# Set FREESWITCH_PATH

ifeq ($(strip $(FREESWITCH_PATH)),)
	BASE := ../../../../
else
	BASE := $(FREESWITCH_PATH)
endif

curr_dir := $(shell pwd)

versions := -DFS_VERSION_MAJOR=$(shell bash $(curr_dir)/tools/getversion.sh "SWITCH_VERSION_MAJOR" $(BASE)) -DFS_VERSION_MINOR=$(shell bash $(curr_dir)/tools/getversion.sh "SWITCH_VERSION_MINOR" $(BASE)) -DFS_VERSION_MICRO=$(shell bash $(curr_dir)/tools/getversion.sh "SWITCH_VERSION_MICRO" $(BASE))

LOCAL_CFLAGS = -I./ -I./include -I./commons -I./commons/base -I./support -D_REENTRANT -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -DK3L_HOSTSYSTEM -DCOMMONS_LIBRARY_USING_FREESWITCH -g -ggdb #-DDEBUG_FLAGS
LOCAL_CFLAGS += $(versions)

LOCAL_LDFLAGS = -lk3l

LOCAL_OBJS = ./commons/base/k3lapi.o ./commons/base/k3lutil.o ./commons/base/config_options.o ./commons/base/format.o ./commons/base/strings.o ./commons/base/ringbuffer.o ./commons/base/verbose.o ./commons/base/saved_condition.o ./commons/base/regex.o ./commons/base/timer.o ./commons/base/configurator/configfile.o ./commons/base/configurator/option.o ./commons/base/configurator/section.o ./commons/base/configurator/restriction.o ./commons/base/verbose_traits.o 
LOCAL_OBJS += ./support/klog-config.o ./support/klog-options.o ./support/config_defaults.o
LOCAL_OBJS += ./src/globals.o ./src/opt.o ./src/frame.o ./src/utils.o ./src/lock.o ./src/spec.o ./src/applications.o ./src/khomp_pvt_fxo.o ./src/khomp_pvt_gsm.o ./src/khomp_pvt_kxe1.o ./src/khomp_pvt_passive.o ./src/khomp_pvt.o ./src/logger.o ./src/cli.o

conf_file_name := khomp.conf.xml
conf_file_dir := $(curr_dir)/Install/files
conf_file_dir_alt := $(curr_dir)/conf
conf_file_install = $(sysconfdir)/autoload_configs

include $(BASE)/build/modmake.rules

local_depend:
	@if test ! -f $(curr_dir)/commons/base/verbose_traits.hpp || test ! -f $(curr_dir)/commons/base/verbose_traits.cpp ; then \
		echo "Generating verbose_traits" ;\
		bash $(curr_dir)/commons/tools/generate-verbose-headers.sh commons/base/ include/k3l.h ;\
	fi;

depend_install:
	@if test "w`kserver --version 2>/dev/null | grep 2.1`" == "w" ; then \
		echo "###############################################################################" ;\
		echo "Install k3l from KHOMP." ;\
		echo "Run: $(curr_dir)/tools/getk3l.sh" ;\
		echo "###############################################################################" ;\
		exit 1;\
	fi;
	@echo "Copy $(conf_file_name)"
	@if test -d  $(conf_file_install) ; then \
		if test -f $(conf_file_dir)/$(conf_file_name) ; then \
			cp $(conf_file_dir)/$(conf_file_name) $(conf_file_install)/$(conf_file_name).new ;\
		else \
			cp $(conf_file_dir_alt)/$(conf_file_name) $(conf_file_install)/$(conf_file_name).new ;\
		fi; \
		if test ! -f "$(conf_file_install)/$(conf_file_name)" ; then \
			mv $(conf_file_install)/$(conf_file_name).new $(conf_file_install)/$(conf_file_name) ;\
		fi; \
	fi;
