# -*-makefile-*-    <-- an Emacs control

# See unix-common.make for an explanation of this file.  This file is
# analogous to unix-common.make, but is for an Irix system.

SONAME = $(@:%.$(MAJ):%)

SHLIB_CMD = $(CCLD) $(LDFLAGS_SHLIB) -o $@ $^ $(LADD)

SHLIB_LE_TARGETS = $(call shliblefn, $(SHARED_LIBS_TO_BUILD))

$(SHLIB_LE_TARGETS):%:%.$(MAJ)
	rm -f $@
	$(LN_S) $< $@


.PHONY: $(SHLIB_INSTALL_TARGETS)
.PHONY: install-shared-libraries

SHLIB_INSTALL_TARGETS = $(SHARED_LIBS_TO_INSTALL:%=%/install)

#SHLIB_INSTALL_TARGETS is like "libfoo/install libbar/install"

install-shared-libraries: $(SHLIB_INSTALL_TARGETS)

$(SHLIB_INSTALL_TARGETS):%/install:%.$(SHLIB_SUFFIX).$(MAJ)
# $< is a library file name, e.g. libfoo.so.3.1 .
	$(INSTALL_SHLIB) $< $(DESTDIR)$(LIBINST_DIR)/$<
	cd $(DESTDIR)$(LIBINST_DIR); \
	  rm -f $< $(<:%.$(MAJ)=%); \
	  $(LN_S) $< $(<:%.$(MAJ)=%)
