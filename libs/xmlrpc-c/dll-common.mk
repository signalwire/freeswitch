# -*-makefile-*-    <-- an Emacs control

# See unix-common.mk for an explanation of this file.  This file is
# analogous to unix-common.mk, but is for a Windows system

SONAME = $@
IMPLIB = $(@:%:%.dll.a)

SHLIB_CMD = $(CCLD) $(LDFLAGS_SHLIB) -o $@ $^ $(LADD)

.PHONY: $(SHLIB_INSTALL_TARGETS)
.PHONY: install-shared-libraries

SHLIB_INSTALL_TARGETS = $(SHARED_LIBS_TO_INSTALL:%=%/install)

#SHLIB_INSTALL_TARGETS is like "libfoo/install libbar/install"

install-shared-libraries: $(SHLIB_INSTALL_TARGETS)

$(SHLIB_INSTALL_TARGETS):%/install:%.$(SHLIB_SUFFIX)
# $< is a library file name, e.g. libfoo.dll .
	$(INSTALL_SHLIB) $< $(DESTDIR)$(LIBINST_DIR)/$<
