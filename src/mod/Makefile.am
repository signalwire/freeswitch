CONF_MODULES=$(shell grep -v "\#" $(switch_builddir)/modules.conf)
FOUND_MODULES=$(shell find . -type d -name mod_\*)
OUR_MODULES=`if test -z "$(MODULES)" ; then echo $(CONF_MODULES) ; else echo $(MODULES) ; fi ;`
OUR_CLEAN_MODULES=`if test -z "$(MODULES)" ; then echo $(CONF_MODULES) $(FOUND_MODULES) ; else echo $(MODULES) ; fi ;`
MOD_NAME=`echo $$i | sed -e 's|^.*/||'`
MOD_DIR=`if test -d $(switch_srcdir)/src/mod/$$i ; then echo $(switch_srcdir)/src/mod/$$i ; else echo $$i ; fi;`


all: 
	@if [ ! -f $(switch_builddir)/modules.conf ] ; then cp $(switch_builddir)/modules.conf.in $(switch_builddir)/modules.conf ; fi
	@echo making modules
	@for i in $(OUR_MODULES) ; do  \
	   echo making $$i ; \
	   moddir=$(MOD_DIR); \
	   if test -f $$moddir/Makefile ; then \
	      cd $$moddir && MODNAME=$(MOD_NAME) BASE=$(switch_builddir) $(MAKE) ; else  \
	      cd $$moddir && MODNAME=$(MOD_NAME) BASE=$(switch_builddir) $(MAKE) -f $(switch_builddir)/build/modmake.rules ; \
	   fi; \
	 done

clean:
	@for i in $(OUR_CLEAN_MODULES) ; do \
	   echo making clean $$i ; \
	   moddir=$(MOD_DIR); \
	   if test -f $$moddir/Makefile ; then \
	      cd $$moddir && MODNAME=$(MOD_NAME) BASE=$(switch_builddir) $(MAKE) clean ; else  \
	      cd $$moddir && MODNAME=$(MOD_NAME) BASE=$(switch_builddir) $(MAKE) -f $(switch_builddir)/build/modmake.rules clean ; \
	   fi; \
	 done

install:
	@echo Installing Modules
	@for i in $(OUR_MODULES) ; do \
	   echo making install $$i ; \
	   moddir=$(MOD_DIR); \
	   if test -f $$moddir/Makefile ; then \
	      cd $$moddir && MODNAME=$(MOD_NAME) BASE=$(switch_builddir) $(MAKE) install ; else  \
	      cd $$moddir && MODNAME=$(MOD_NAME) BASE=$(switch_builddir) $(MAKE) -f $(switch_builddir)/build/modmake.rules install ; \
	   fi; \
	done
	@echo done

uninstall:
	@echo Uninstalling Modules
	@for i in $(OUR_MODULES) ; do \
	   echo making uninstall $$i ; \
	   moddir=$(MOD_DIR); \
	   if test -f $$moddir/Makefile ; then \
	      cd $$moddir && MODNAME=$(MOD_NAME) BASE=$(switch_builddir) $(MAKE) uninstall ; else  \
	      cd $$moddir && MODNAME=$(MOD_NAME) BASE=$(switch_builddir) $(MAKE) -f $(switch_builddir)/build/modmake.rules uninstall ; \
	   fi; \
	done
	@echo done

