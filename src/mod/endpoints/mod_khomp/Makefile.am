include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_khomp
mod_LTLIBRARIES = mod_khomp.la
mod_khomp_la_SOURCES  = mod_khomp.cpp
mod_khomp_la_CFLAGS   = $(AM_CFLAGS)
mod_khomp_la_CFLAGS  += -I./ -I./include -I./commons -I./commons/base -I./support 
mod_khomp_la_CFLAGS  += -D_REENTRANT -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -DK3L_HOSTSYSTEM -DCOMMONS_LIBRARY_USING_FREESWITCH
mod_khomp_la_CFLAGS  += -DFS_VERSION_MAJOR=`./tools/getversion.sh "SWITCH_VERSION_MAJOR" $(switch_srcdir)`
mod_khomp_la_CFLAGS  += -DFS_VERSION_MINOR=`./tools/getversion.sh "SWITCH_VERSION_MINOR" $(switch_srcdir)`
mod_khomp_la_CFLAGS  += -DFS_VERSION_MICRO=`./tools/getversion.sh "SWITCH_VERSION_MICRO" $(switch_srcdir)` 
mod_khomp_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_khomp_la_LDFLAGS  = -avoid-version -module -no-undefined -shared -lk3l
KHOMP_MODDIR=$(switch_srcdir)/src/mod/endpoints/mod_khomp
TOOLS_DIR=$(KHOMP_MODDIR)/commons/tools

mod_khomp_la_SOURCES += ./commons/base/k3lapi.cpp ./commons/base/k3lutil.cpp ./commons/base/config_options.cpp ./commons/base/format.cpp ./commons/base/strings.cpp ./commons/base/ringbuffer.cpp ./commons/base/verbose.cpp ./commons/base/saved_condition.cpp ./commons/base/regex.cpp ./commons/base/timer.cpp ./commons/base/configurator/configfile.cpp ./commons/base/configurator/option.cpp ./commons/base/configurator/section.cpp ./commons/base/configurator/restriction.cpp ./commons/base/verbose_traits.cpp 
mod_khomp_la_SOURCES += ./support/klog-config.cpp ./support/klog-options.cpp ./support/config_defaults.cpp
mod_khomp_la_SOURCES += ./src/globals.cpp ./src/opt.cpp ./src/frame.cpp ./src/utils.cpp ./src/lock.cpp ./src/spec.cpp ./src/applications.cpp ./src/khomp_pvt_fxo.cpp ./src/khomp_pvt_gsm.cpp ./src/khomp_pvt_kxe1.cpp ./src/khomp_pvt_passive.cpp ./src/khomp_pvt.cpp ./src/logger.cpp ./src/cli.cpp

conf_file_name = khomp.conf.xml
conf_file_dir = ./Install/files
conf_file_dir_alt = ./conf
conf_file_install = $(sysconfdir)/autoload_configs

BUILT_SOURCES=verbose_traits.hpp verbose_traits.cpp

verbose_traits.hpp verbose_traits.cpp:
	@if test ! -f $(KHOMP_MODDIR)/commons/base/verbose_traits.hpp || test ! -f $(KHOMP_MODDIR)/commons/base/verbose_traits.cpp ; then \
		echo "Generating verbose_traits" ;\
		bash $(TOOLS_DIR)/generate-verbose-headers.sh $(KHOMP_MODDIR)/commons/base/ include/k3l.h ;\
	fi;

install-data-local:
	@if test "w`kserver --version 2>/dev/null | grep 2.1`" == "w" ; then \
		echo "###############################################################################" ;\
		echo "Install k3l from KHOMP." ;\
		echo "Run: ./tools/getk3l.sh" ;\
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
