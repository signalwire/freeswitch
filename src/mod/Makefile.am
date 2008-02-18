all: $(OUR_MODULES)
clean: $(OUR_CLEAN_MODULES)
install: $(OUR_INSTALL_MODULES)
uninstall: $(OUR_UNINSTALL_MODULES)

$(OUR_MODULES) $(OUR_CLEAN_MODULES) $(OUR_INSTALL_MODULES) $(OUR_UNINSTALL_MODULES):
	@set fnord $$MAKEFLAGS; amf=$$2; \
	target=`echo $@ | sed -e 's|^.*-||'`; \
	modname=`echo $@ | sed -e 's|-.*||'`; \
	confmoddir=`cat $(switch_builddir)/modules.conf | sed -e 's| ||' | grep $$modname$$ | sed -e 's|#||' | head -1`; \
	if test -z "$$confmoddir" ; then moddir=$@ ; else  \
		if test -d  "$(switch_srcdir)/src/mod/$$confmoddir" ; then \
			moddir="$(switch_srcdir)/src/mod/$$confmoddir" ; else \
			moddir="$$confmoddir" ; \
		fi ; \
	fi ; \
	if test -z "$$target" ; then target="all" ; fi ; \
	if ! test -d "$$moddir" ; then echo ; echo "WARNING $$moddir does not exist, skipping" ; else \
		echo ;\
		echo making $$target $$modname ;\
		(if test -f "$$moddir/Makefile" ; then \
			cd $$moddir && MODNAME=$$modname BASE=$(switch_builddir) $(MAKE) $(AM_MAKEFLAGS) $$target; else\
			cd $$moddir && MODNAME=$$modname BASE=$(switch_builddir) $(MAKE) $(AM_MAKEFLAGS) -f $(switch_builddir)/build/modmake.rules $$target ;\
		fi;) || case "$$amf" in *=*) exit 1;; *k*) fail=yes;; *) exit 1;; esac; \
	fi; \
	test -z "$$fail" ;

.DEFAULT:
	@if test -z "`echo $@ | grep all`"; then  $(MAKE) $(AM_MAKEFLAGS) $@-all ; else echo Unknown target `echo $@ | sed -e 's|-all||'`; exit 1; fi



