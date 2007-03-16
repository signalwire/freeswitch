CONF_MODULES=if test ! -f $(switch_builddir)/modules.conf ; then cp $(switch_builddir)/modules.conf.in $(switch_builddir)/modules.conf ; fi ; conf_modules=`grep -v "\#" $(switch_builddir)/modules.conf` ;
FOUND_MODULES=found_modules=`find . -type d -name mod_\*` ;
OUR_MODULES=if test -z "$(MODULES)" ; then our_modules=$$conf_modules ; else our_modules=$(MODULES) ; fi ;
OUR_CLEAN_MODULES=if test -z "$(MODULES)" ; then our_clean_modules=`echo $$conf_modules $$found_modules` ; else our_clean_modules=$(MODULES) ; fi ;
MOD_NAME=`echo $$i | sed -e 's|^.*/||'`
MOD_DIR=`if test -d $(switch_srcdir)/src/mod/$$i ; then echo $(switch_srcdir)/src/mod/$$i ; else echo $$i ; fi;`


all: 
	@$(CONF_MODULES) \
	$(OUR_MODULES) \
	echo making modules ; \
	for i in $$our_modules ; do  \
	   echo making $$i ; \
	   moddir=$(MOD_DIR); \
	   if test -f $$moddir/Makefile ; then \
	      cd $$moddir && MODNAME=$(MOD_NAME) BASE=$(switch_builddir) $(MAKE) || exit 1; else  \
	      cd $$moddir && MODNAME=$(MOD_NAME) BASE=$(switch_builddir) $(MAKE) -f $(switch_builddir)/build/modmake.rules || exit 1; \
	   fi; \
	 done

clean:
	@$(CONF_MODULES) \
	$(FOUND_MODULES) \
	$(OUR_CLEAN_MODULES) \
	echo making clean modules ; \
	for i in $$our_clean_modules ; do \
	   echo making clean $$i ; \
	   moddir=$(MOD_DIR); \
	   if test -f $$moddir/Makefile ; then \
	      cd $$moddir && MODNAME=$(MOD_NAME) BASE=$(switch_builddir) $(MAKE) clean || exit 1; else  \
	      cd $$moddir && MODNAME=$(MOD_NAME) BASE=$(switch_builddir) $(MAKE) -f $(switch_builddir)/build/modmake.rules clean || exit 1; \
	   fi; \
	 done

install:
	@$(CONF_MODULES) \
	$(OUR_MODULES) \
	echo Installing Modules ; \
	for i in $$our_modules ; do  \
	   echo making install $$i ; \
	   moddir=$(MOD_DIR); \
	   if test -f $$moddir/Makefile ; then \
	      cd $$moddir && MODNAME=$(MOD_NAME) BASE=$(switch_builddir) $(MAKE) install || exit 1; else  \
	      cd $$moddir && MODNAME=$(MOD_NAME) BASE=$(switch_builddir) $(MAKE) -f $(switch_builddir)/build/modmake.rules install || exit 1; \
	   fi; \
	done
	@echo done

uninstall:
	@$(CONF_MODULES) \
	$(OUR_MODULES) \
	echo Uninstalling Modules ; \
	for i in $$our_modules ; do  \
	   echo making uninstall $$i ; \
	   moddir=$(MOD_DIR); \
	   if test -f $$moddir/Makefile ; then \
	      cd $$moddir && MODNAME=$(MOD_NAME) BASE=$(switch_builddir) $(MAKE) uninstall || exit 1; else  \
	      cd $$moddir && MODNAME=$(MOD_NAME) BASE=$(switch_builddir) $(MAKE) -f $(switch_builddir)/build/modmake.rules uninstall || exit 1; \
	   fi; \
	done
	@echo done

