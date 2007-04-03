CONF_MODULES=if test ! -f $(switch_builddir)/modules.conf ; then cp $(switch_builddir)/build/modules.conf.in $(switch_builddir)/modules.conf ; fi ; conf_modules=`grep -v "\#" $(switch_builddir)/modules.conf` ;
FOUND_MODULES=found_modules=`find . -type d -name mod_\* | sed -e 's|^\./||'` ;
OUR_MODULES=if test -z "$(MODULES)" ; then our_modules=$$conf_modules ; else our_modules="$(MODULES)" ; fi ;
OUR_CLEAN_MODULES=if test -z "$(MODULES)" ; then tmp_clean_modules=`echo $$conf_modules $$found_modules`; \
			our_clean_modules=`for i in $$tmp_clean_modules ; do echo $$i ; done | sort | uniq` ; \
			 else our_clean_modules="$(MODULES)" ; fi ;
MOD_NAME=`echo $$i | sed -e 's|^.*/||'`
MOD_DIR=`if test -d $(switch_srcdir)/src/mod/$$i ; then echo $(switch_srcdir)/src/mod/$$i ; else echo $$i ; fi;`

all: 
	@$(CONF_MODULES) \
	$(OUR_MODULES) \
	echo ;\
	echo making modules ; \
	for i in $$our_modules ; do  \
	   echo ;\
	   echo making $$i ; \
	   moddir=$(MOD_DIR); \
	   if test -f $$moddir/Makefile ; then \
	      cd $$moddir && MODNAME=$(MOD_NAME) BASE=$(switch_builddir) $(MAKE) $(AM_MAKEFLAGS) || exit 1; else  \
	      cd $$moddir && MODNAME=$(MOD_NAME) BASE=$(switch_builddir) $(MAKE) $(AM_MAKEFLAGS) -f $(switch_builddir)/build/modmake.rules || exit 1; \
	   fi; \
	 done
	@echo
	@echo done making modules

clean:
	@$(CONF_MODULES) \
	$(FOUND_MODULES) \
	$(OUR_CLEAN_MODULES) \
	echo ;\
	echo making clean modules ; \
	for i in $$our_clean_modules ; do \
	   echo ;\
	   echo making clean $$i ; \
	   moddir=$(MOD_DIR); \
	   if test -f $$moddir/Makefile ; then \
	      cd $$moddir && MODNAME=$(MOD_NAME) BASE=$(switch_builddir) $(MAKE) $(AM_MAKEFLAGS) clean || exit 1; else  \
	      cd $$moddir && MODNAME=$(MOD_NAME) BASE=$(switch_builddir) $(MAKE) $(AM_MAKEFLAGS) -f $(switch_builddir)/build/modmake.rules clean || exit 1; \
	   fi; \
	 done
	@echo
	@echo done making clean modules

install:
	@$(CONF_MODULES) \
	$(OUR_MODULES) \
	echo ;\
	echo Installing Modules ; \
	for i in $$our_modules ; do  \
	   echo ;\
	   echo making install $$i ; \
	   moddir=$(MOD_DIR); \
	   if test -f $$moddir/Makefile ; then \
	      cd $$moddir && MODNAME=$(MOD_NAME) BASE=$(switch_builddir) $(MAKE) $(AM_MAKEFLAGS) install || exit 1; else  \
	      cd $$moddir && MODNAME=$(MOD_NAME) BASE=$(switch_builddir) $(MAKE) $(AM_MAKEFLAGS) -f $(switch_builddir)/build/modmake.rules install || exit 1; \
	   fi; \
	done
	@echo
	@echo done Installing Modules

uninstall:
	@$(CONF_MODULES) \
	$(OUR_MODULES) \
	echo ;\
	echo Uninstalling Modules ; \
	for i in $$our_modules ; do  \
	   echo ;\
	   echo making uninstall $$i ; \
	   moddir=$(MOD_DIR); \
	   if test -f $$moddir/Makefile ; then \
	      cd $$moddir && MODNAME=$(MOD_NAME) BASE=$(switch_builddir) $(MAKE) $(AM_MAKEFLAGS) uninstall || exit 1; else  \
	      cd $$moddir && MODNAME=$(MOD_NAME) BASE=$(switch_builddir) $(MAKE) $(AM_MAKEFLAGS) -f $(switch_builddir)/build/modmake.rules uninstall || exit 1; \
	   fi; \
	done
	@echo
	@echo done Uninstalling Modules


@am__include@ @am__quote@$(switch_builddir)/$(DEPDIR)/modules.targets@am__quote@

$(MODULE_TARGETS):
	@set fnord $$MAKEFLAGS; amf=$$2; \
	moddir=`grep $@ $(switch_builddir)/modules.conf | sed -e 's|#||'` ; \
	(if test -f $$moddir/Makefile ; then \
		cd $$moddir && MODNAME=$(MOD_NAME) BASE=$(switch_builddir) $(MAKE) $(AM_MAKEFLAGS); else\
		cd $$moddir && MODNAME=$(MOD_NAME) BASE=$(switch_builddir) $(MAKE) $(AM_MAKEFLAGS) -f $(switch_builddir)/build/modmake.rules;\
	fi;)\
	   || case "$$amf" in *=*) exit 1;; *k*) fail=yes;; *) exit 1;; esac; \
	test -z "$$fail"
