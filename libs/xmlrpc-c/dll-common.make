# -*-makefile-*-    <-- an Emacs control

# See unix-common.make for an explanation of this file.  This file is
# analogous to unix-common.make, but is for a Windows system

SONAME = $@
IMPLIB = $(@:%:%.dll.a)

SHLIB_CMD = $(CCLD) $(LDFLAGS_SHLIB) -o $@ $^ $(LADD)

.PHONY: $(SHLIB_INSTALL_TARGETS)
.PHONY: install-shared-libraries

SHLIB_INSTALL_TARGETS = $(SHARED_LIBS_TO_INSTALL:%=%/install)

#SHLIB_INSTALL_TARGETS is like "libfoo/install libbar/install"

install-shared-libraries: $(SHLIB_INSTALL_TARGETS)

$(SHLIB_INSTALL_TARGETS):lib%/install:$(SHLIB_PREFIX)%.$(SHLIB_SUFFIX).$(MAJ).$(MIN)
# $< is a library file name, e.g. cygfoo.so.3.1 .
	$(INSTALL_SHLIB) $< $(DESTDIR)$(LIBINST_DIR)/$<
