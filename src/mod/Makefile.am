MODULES = `grep -v "\#" $(switch_builddir)/modules.conf`

all: 
	@if [ ! -f $(switch_builddir)/modules.conf ] ; then cp $(switch_builddir)/modules.conf.in $(switch_builddir)/modules.conf ; fi
	@echo making modules
	@for i in $(MODULES) ; do  \
	   echo making $$i ; \
	   modname=`echo $$i | sed "s/^.*\///g"`; \
	   if test -z $$modname ; then  modname=$$i ; fi; \
	   if test -d $(switch_srcdir)/src/mod/$$i ; then moddir=$(switch_srcdir)/src/mod/$$i ; else moddir=$$i ; fi; \
	   if test -f $$moddir/Makefile ; then \
	      cd $$moddir && MODNAME=$$modname BASE=$(switch_builddir) $(MAKE) ; else  \
	      cd $$moddir && MODNAME=$$modname BASE=$(switch_builddir) $(MAKE) -f $(switch_builddir)/build/modmake.rules ; \
	   fi; \
	 done

clean:
	@for i in `find . -type d -name mod_\*` ; do \
	   echo making clean $$i ; \
	   modname=`echo $$i | sed "s/^.*\///g"`; \
	   if test -z $$modname ; then  modname=$$i ; fi; \
	   if test -d $(switch_srcdir)/src/mod/$$i ; then moddir=$(switch_srcdir)/src/mod/$$i ; else moddir=$$i ; fi; \
	   if test -f $$moddir/Makefile ; then \
	      cd $$moddir && MODNAME=$$modname BASE=$(switch_builddir) $(MAKE) clean ; else  \
	      cd $$moddir && MODNAME=$$modname BASE=$(switch_builddir) $(MAKE) -f $(switch_builddir)/build/modmake.rules clean ; \
	   fi; \
	 done

install:
	@echo Installing Modules
	@for i in $(MODULES) ; do \
	   echo making install $$i ; \
	   modname=`echo $$i | sed "s/^.*\///g"`; \
	   if test -z $$modname ; then  modname=$$i ; fi; \
	   if test -d $(switch_srcdir)/src/mod/$$i ; then moddir=$(switch_srcdir)/src/mod/$$i ; else moddir=$$i ; fi; \
	   if test -f $$moddir/Makefile ; then \
	      cd $$moddir && MODNAME=$$modname BASE=$(switch_builddir) $(MAKE) install ; else  \
	      cd $$moddir && MODNAME=$$modname BASE=$(switch_builddir) $(MAKE) -f $(switch_builddir)/build/modmake.rules clean ; \
	   fi; \
	done
	@echo done

