MODNAME := mod_khomp
VERBOSE := 1

ifeq ($(strip $(FREESWITCH_PATH)),)
	BASE := ../../../../
else
	BASE := $(FREESWITCH_PATH)
endif

curr_dir := $(shell pwd)

versions := -DFS_VERSION_MAJOR=$(shell bash $(curr_dir)/tools/getversion.sh "SWITCH_VERSION_MAJOR" $(BASE)) -DFS_VERSION_MINOR=$(shell bash $(curr_dir)/tools/getversion.sh "SWITCH_VERSION_MINOR" $(BASE)) -DFS_VERSION_MICRO=$(shell bash $(curr_dir)/tools/getversion.sh "SWITCH_VERSION_MICRO" $(BASE))

LOCAL_CFLAGS = -I./ -I./include -I./commons -I./support -D_REENTRANT -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -DK3L_HOSTSYSTEM -DCOMMONS_LIBRARY_USING_FREESWITCH -g -ggdb #-DDEBUG_FLAGS
LOCAL_CFLAGS += $(versions)

LOCAL_LDFLAGS = -lk3l

LOCAL_OBJS = ./commons/k3lapi.o ./commons/k3lutil.o ./commons/config_options.o ./commons/format.o ./commons/strings.o ./commons/ringbuffer.o ./commons/verbose.o ./commons/saved_condition.o ./commons/regex.o ./commons/timer.o ./commons/configurator/configfile.o ./commons/configurator/option.o ./commons/configurator/section.o ./commons/configurator/restriction.o
LOCAL_OBJS += ./support/klog-config.o ./support/klog-options.o ./support/config_defaults.o
LOCAL_OBJS += ./src/globals.o ./src/opt.o ./src/frame.o ./src/utils.o ./src/lock.o ./src/spec.o ./src/applications.o ./src/khomp_pvt_fxo.o ./src/khomp_pvt_gsm.o ./src/khomp_pvt_kxe1.o ./src/khomp_pvt_passive.o ./src/khomp_pvt.o ./src/logger.o ./src/cli.o

conf_file_name := khomp.conf.xml
conf_file_dir := $(curr_dir)/Install/files
conf_file_dir_alt := $(curr_dir)/conf
conf_file_install = $(sysconfdir)/autoload_configs

include $(BASE)/build/modmake.rules

depend_install:
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
