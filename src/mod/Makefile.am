-include modules.inc

all: $(OUR_MODULES)
clean: $(OUR_CLEAN_MODULES) $(OUR_DISABLED_CLEAN_MODULES)
install: $(OUR_INSTALL_MODULES)
uninstall: $(OUR_UNINSTALL_MODULES) $(OUR_DISABLED_UNINSTALL_MODULES)
print_tests: $(OUR_TEST_MODULES)
check: $(OUR_CHECK_MODULES)

mod_skypopen-all: mod_gsmopen-all
mod_gsmopen-all: mod_spandsp-all
mod_rayo-all: mod_ssml-all
mod_ssml-all: mod_rayo-all

$(OUR_MODULES) $(OUR_CLEAN_MODULES) $(OUR_INSTALL_MODULES) $(OUR_UNINSTALL_MODULES) $(OUR_DISABLED_MODULES) $(OUR_DISABLED_CLEAN_MODULES) $(OUR_DISABLED_INSTALL_MODULES) $(OUR_DISABLED_UNINSTALL_MODULES) $(OUR_TEST_MODULES) $(OUR_CHECK_MODULES):
	@set fnord $$MAKEFLAGS; amf=$$2; \
	target=`echo $@ | sed -e 's|^.*-||'`; \
	modname=`echo $@ | sed -e 's|-.*||' | sed -e 's|^.*/||'`; \
	enabled=`echo $(CONF_MODULES) | grep -w $$modname`; \
	if ! test -z "$$enabled"; then \
		confmoddir=`cat $(switch_builddir)/modules.conf | sed -e 's| ||' | sed 's/|.*//' | grep $$modname$$ | sed -e 's|#||' | head -n 1`; \
		modsource=`cat $(switch_builddir)/modules.conf | grep "$$modname.*|" | sed 's/.*|//' | head -n 1`; \
		if test -z "$$confmoddir" ; then \
			moddir=$@ ; \
			buildmoddir=$(switch_builddir)/src/mod/$@ ;\
		else  \
			if [[ $$confmoddir == /* ]]; then \
				moddir="$$confmoddir" ; \
				buildmoddir="$$moddir" ; \
			else \
				if test -d  "$(switch_srcdir)/src/mod/$$confmoddir" ; then \
					moddir="$(switch_srcdir)/src/mod/$$confmoddir" ; \
					buildmoddir="$(switch_builddir)/src/mod/$$confmoddir" ; \
				else \
					if ! test -z "$$modsource" ; then \
						moddir="$(switch_srcdir)/src/mod/outoftree/$$confmoddir" ; \
						buildmoddir="$(switch_builddir)/src/mod/outoftree/$$confmoddir" ; \
						if ! test -d "$$buildmoddir"; then \
							echo "Cloning from git: $$modsource" ; \
							mkdir -p $$buildmoddir ; \
							git clone $$modsource $$buildmoddir ; \
						fi ; \
						if ! test -d "$$buildmoddir"; then \
							echo ; echo "WARNING There is no $$buildmoddir...failing ;" ; \
							fail=yes ; \
						fi ; \
					else \
						moddir="$$confmoddir" ; \
						buildmoddir="$(switch_builddir)/src/mod/$$confmoddir" ; \
					fi ; \
				fi ; \
			fi ; \
		fi ; \
		if test -f  "$$buildmoddir/bootstrap.sh" -a ! -f  "$$buildmoddir/configure" -a ! -f  "$$buildmoddir/configure.sh" ; then \
			cd $$buildmoddir && MODDIR=$$moddir MODNAME=$$modname BASE=$(switch_builddir) PKG_CONFIG_PATH=$(switch_builddir)/build/standalone_module:$(PKG_CONFIG_PATH) eval $$buildmoddir/bootstrap.sh ; \
		fi ; \
		if test -f  "$$buildmoddir/configure.sh" -a ! -f "$$buildmoddir/Makefile" ; then \
			cd $$buildmoddir && MODDIR=$$moddir MODNAME=$$modname BASE=$(switch_builddir) PKG_CONFIG_PATH=$(switch_builddir)/build/standalone_module:$(PKG_CONFIG_PATH) eval $$buildmoddir/configure.sh ; \
		else \
			if test -f  "$$buildmoddir/configure" -a ! -f "$$buildmoddir/Makefile" ; then \
				cd $$buildmoddir && MODDIR=$$moddir MODNAME=$$modname BASE=$(switch_builddir) PKG_CONFIG_PATH=$(switch_builddir)/build/standalone_module:$(PKG_CONFIG_PATH) eval $$buildmoddir/configure ; \
			fi ; \
		fi ; \
		if test -z "$$target" ; then target="all" ; fi ; \
		if ! test -z "$$fail" || ! test -f $$moddir/$$modname.c && ! test -f $$moddir/$$modname.cpp && test $$modname != "mod_com_g729" ;	\
		then echo ; echo "WARNING $$modname is not a valid FreeSWITCH module dir, skipping it..." ; else \
			if test "$$target" != "print_tests" ; then echo; echo making $$target $$modname ; fi;\
			test -d "$$buildmoddir" || mkdir -p $$buildmoddir ; \
			(if test -f "$$moddir/Makefile" ; then \
				test -f "$$buildmoddir/Makefile" || cp $$moddir/Makefile $$buildmoddir/Makefile ; \
				cd $$buildmoddir && MODDIR=$$moddir MODNAME=$$modname BASE=$(switch_builddir) $(MAKE) $(AM_MAKEFLAGS) $$target; \
			else\
				cd $$buildmoddir && MODDIR=$$moddir MODNAME=$$modname BASE=$(switch_builddir) $(MAKE) $(AM_MAKEFLAGS) -f $(switch_builddir)/build/modmake.rules $$target ;\
			fi;) || case "$$amf" in *=*) exit 1;; *k*) fail=yes;; *) exit 1;; esac; \
		fi; \
		test -z "$$fail" ; \
	fi;

mod_com_g729-activate:
	cd $(switch_builddir)/src/mod/codecs/mod_com_g729 && $(MAKE) $(AM_MAKEFLAGS) activate

.DEFAULT:
	@if test -z "`echo $@ | grep '\-all$$'`"; then  $(MAKE) $(AM_MAKEFLAGS) $@-all ; else echo Unknown target `echo $@ | sed -e 's|-all||'`; exit 1; fi


