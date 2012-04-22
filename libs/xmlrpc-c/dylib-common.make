# -*-makefile-*-    <-- an Emacs control

# See unix-common.make for an explanation of this file.  This file is
# analogous to unix-common.make, but is for an Irix system.

SONAME = $(@:%.$(MIN)=%)

SHLIB_CMD = $(CCLD) $(LDFLAGS_SHLIB) -o $@ $^ $(LADD)

SHLIBPP_CMD = $(CXXLD) $(LDFLAGS_SHLIB) -o $@ $^ $(LADD)

SHLIB_LE_TARGETS = $(call shliblefn, $(SHARED_LIBS_TO_BUILD))

$(SHLIB_LE_TARGETS):%.$(SHLIB_SUFFIX):%.$(MAJ).$(MIN).$(SHLIB_SUFFIX)
	rm -f $@
	$(LN_S) $< $@


.PHONY: $(SHLIB_INSTALL_TARGETS)
.PHONY: install-shared-libraries

SHLIB_INSTALL_TARGETS = $(SHARED_LIBS_TO_INSTALL:%=%/install)

#SHLIB_INSTALL_TARGETS is like "libfoo/install libbar/install"

install-shared-libraries: $(SHLIB_INSTALL_TARGETS)

$(SHLIB_INSTALL_TARGETS):%/install:%.$(MAJ).$(MIN).$(SHLIB_SUFFIX)
# $< is a library file name, e.g. libfoo.so.3.1 .
	$(INSTALL_SHLIB) $< $(DESTDIR)$(LIBINST_DIR)/$<
	cd $(DESTDIR)$(LIBINST_DIR); \
	  rm -f $(<:%.$(MIN).$(SHLIB_SUFFIX)=%.$(SHLIB_SUFFIX)); \
	  $(LN_S) $< $(<:%.$(MIN).$(SHLIB_SUFFIX)=%.$(SHLIB_SUFFIX))
	cd $(DESTDIR)$(LIBINST_DIR); \
	  rm -f $(<:%.$(MAJ).$(MIN).$(SHLIB_SUFFIX)=%.$(SHLIB_SUFFIX)); \
	  $(LN_S) $(<:%.$(MIN).$(SHLIB_SUFFIX)=%.$(SHLIB_SUFFIX)) \
	    $(<:%.$(MAJ).$(MIN).$(SHLIB_SUFFIX)=%.$(SHLIB_SUFFIX))
