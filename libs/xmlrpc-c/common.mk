# This file contains rules and variable settings for the convenience
# of every other make file in the package.

# No make file is required to use this file, but it usually saves a lot
# of duplication.

# The following make variables are meaningful as input to this file:
#
# SRCDIR:  Name of directory which is the top of the Xmlrpc-c source tree.
# BLDDIR: Name of directory which is the top of the Xmlrpc-c build tree.

include $(SRCDIR)/Makefile.version

# .DELETE_ON_ERROR is a special predefined Make target that says to delete
# the target if a command in the rule for it fails.  That's important,
# because we don't want a half-made target sitting around looking like it's
# fully made.
.DELETE_ON_ERROR:

GCC_WARNINGS = -Wall -Wundef -Wimplicit -W -Winline -Wundef
  # We need -Wwrite-strings after we fix all the missing consts

GCC_C_WARNINGS = $(GCC_WARNINGS) \
  -Wmissing-declarations -Wstrict-prototypes -Wmissing-prototypes

GCC_CXX_WARNINGS = $(GCC_WARNINGS) -Woverloaded-virtual -Wsynth

# The NDEBUG macro says not to build code that assumes there are no bugs.
# This makes the code go faster.  The main thing it does is tell the C library
# to make assert() a no-op as opposed to generating code to check the
# assertion and crash the program if it isn't really true.  You can add
# -UNDEBUG (in any of various ways) to override this.
#
CFLAGS_COMMON = -DNDEBUG
CXXFLAGS_COMMON = -DNDEBUG

ifeq ($(C_COMPILER_GNU),yes)
  CFLAGS_COMMON += $(GCC_C_WARNINGS) -fno-common -g -O3
endif

ifeq ($(CXX_COMPILER_GNU),yes)
  CXXFLAGS_COMMON += $(GCC_CXX_WARNINGS) -g
endif

DISTDIR = $(BLDDIR)/$(PACKAGE)-$(VERSION)/$(SUBDIR)

# MIN is the minor version number for shared libraries.
# MAJ is the major version number, but is set separately by
# individual make files so that the major number of one library can change
# from one release to another while the major number of another does not.
MIN = $(XMLRPC_MINOR_RELEASE)

# CURDIR was introduced in GNU Make 3.77.
ifeq ($(CURDIR)x,x)
  CURDIR := $(shell /bin/pwd)
endif

##############################################################################
#                        STATIC LINK LIBRARY RULES                           #
##############################################################################


# To use this rule, the including make file must set a target_specific
# variable LIBOBJECTS (and declare dependencies that include LIBOBJECTS).
# Example:
#   FOO_OBJECTS = foo1.o foo2.o
#   libfoo.a: LIBOBJECTS = $(FOO_OBJECTS)
#   libfoo.a: $(FOO_OBJECTS)
#   TARGET_LIBRARY_NAMES = libfoo

TARGET_STATIC_LIBRARIES = \
  $(TARGET_LIBRARY_NAMES:%=%.a) $(TARGET_LIB_NAMES_PP:%=%.a)
$(TARGET_STATIC_LIBRARIES):
	-rm -f $@
	$(AR) cru $@ $(LIBOBJECTS)
	$(RANLIB) $@


##############################################################################
#                     SHARED LIBRARY RULES, VARIABLES                        #
##############################################################################

ifeq ($(SHARED_LIB_TYPE),unix)
  include $(SRCDIR)/unix-common.make
  endif

ifeq ($(SHARED_LIB_TYPE),irix)
  include $(SRCDIR)/irix-common.make
  endif

ifeq ($(SHARED_LIB_TYPE),dll)
  include $(SRCDIR)/dll-common.make
  endif

ifeq ($(SHARED_LIB_TYPE),dylib)
  include $(SRCDIR)/dylib-common.make
  endif

ifeq ($(SHARED_LIB_TYPE),NONE)
  install-shared-libraries:
  endif

# To use this rule, the including make file must set a target-specific
# variable LIBOBJECTS (and declare dependencies that include LIBOBJECTS).
# Analogous to static library rule above.

# Optionally, including make file can set LIBDEP (probably
# target-specific) to the -L and -l options necessary to declare the
# libraries the target uses at run time.  (This information gets built
# into the shared library so that the runtime library loader will load
# the specified libraries when asked to load the target library).

ifeq ($(MUST_BUILD_SHLIB),Y)
  TARGET_SHARED_LIBRARIES = $(call shlibfn, $(TARGET_LIBRARY_NAMES))
  TARGET_SHARED_LIBS_PP = $(call shlibfn, $(TARGET_LIB_NAMES_PP))
  ifeq ($(MUST_BUILD_SHLIBLE),Y)
    TARGET_SHARED_LE_LIBS = \
      $(call shliblefn, $(TARGET_LIBRARY_NAMES) $(TARGET_LIB_NAMES_PP))
  else
    TARGET_SHARED_LE_LIBS =
  endif
else
  TARGET_SHARED_LIBRARIES =
  TARGET_SHARED_LIBS_PP =
  TARGET_SHARED_LE_LIBS =
endif

#------ the actual rules ----------------------------------------------------
$(TARGET_SHARED_LIBRARIES) dummyshlib:
	$(CCLD) $(LDFLAGS_SHLIB) $(LIBDEP) -o $@ $(LIBOBJECTS) $(LADD)

$(TARGET_SHARED_LIBS_PP) dummyshlibpp:
	$(CXXLD) $(LDFLAGS_SHLIB) $(LIBDEP) -o $@ $(LIBOBJECTS) $(LADD)
#----------------------------------------------------------------------------

LIBXMLRPC_UTIL_DIR = $(BLDDIR)/lib/libutil

ifneq ($(OMIT_LIBXMLRPC_UTIL_RULE),Y)
LIBXMLRPC_UTIL           = \
  $(call shliblefn, $(LIBXMLRPC_UTIL_DIR)/libxmlrpc_util)
LIBXMLRPC_UTIL_A         = $(LIBXMLRPC_UTIL_DIR)/libxmlrpc_util.a
endif

ifneq ($(OMIT_XMLRPC_LIB_RULE),Y)

LIBXMLRPC              = \
  $(call shliblefn, $(BLDDIR)/src/libxmlrpc)
LIBXMLRPC_CLIENT       = \
  $(call shliblefn, $(BLDDIR)/src/libxmlrpc_client)
LIBXMLRPC_SERVER       = \
  $(call shliblefn, $(BLDDIR)/src/libxmlrpc_server)
LIBXMLRPC_SERVER_ABYSS = \
  $(call shliblefn, $(BLDDIR)/src/libxmlrpc_server_abyss)
LIBXMLRPC_SERVER_CGI   = \
  $(call shliblefn, $(BLDDIR)/src/libxmlrpc_server_cgi)

LIBXMLRPC_A              = $(BLDDIR)/src/libxmlrpc.a
LIBXMLRPC_CLIENT_A       = $(BLDDIR)/src/libxmlrpc_client.a
LIBXMLRPC_SERVER_A       = $(BLDDIR)/src/libxmlrpc_server.a
LIBXMLRPC_SERVER_ABYSS_A = $(BLDDIR)/src/libxmlrpc_server_abyss.a
LIBXMLRPC_SERVER_CGI_A   = $(BLDDIR)/src/libxmlrpc_server_cgi.a

endif

LIBXMLRPC_XMLTOK_DIR = $(BLDDIR)/lib/expat/xmltok

ifneq ($(OMIT_XMLTOK_LIB_RULE),Y)
LIBXMLRPC_XMLTOK         = \
  $(call shliblefn, $(LIBXMLRPC_XMLTOK_DIR)/libxmlrpc_xmltok)
LIBXMLRPC_XMLTOK_A       = $(LIBXMLRPC_XMLTOK_DIR)/libxmlrpc_xmltok.a
endif

LIBXMLRPC_XMLPARSE_DIR = $(BLDDIR)/lib/expat/xmlparse

ifneq ($(OMIT_XMLPARSE_LIB_RULE),Y)
LIBXMLRPC_XMLPARSE       = \
  $(call shliblefn, $(LIBXMLRPC_XMLPARSE_DIR)/libxmlrpc_xmlparse)
LIBXMLRPC_XMLPARSE_A     = $(LIBXMLRPC_XMLPARSE_DIR)/libxmlrpc_xmlparse.a
endif

LIBXMLRPC_ABYSS_DIR = $(BLDDIR)/lib/abyss/src

ifneq ($(OMIT_ABYSS_LIB_RULE),Y)
LIBXMLRPC_ABYSS          = \
  $(call shliblefn, $(LIBXMLRPC_ABYSS_DIR)/libxmlrpc_abyss)
LIBXMLRPC_ABYSS_A        = $(LIBXMLRPC_ABYSS_DIR)/libxmlrpc_abyss.a
endif

ifneq ($(OMIT_CPP_LIB_RULES),Y)
LIBXMLRPC_CPP              = \
  $(call shliblefn, $(BLDDIR)/src/cpp/libxmlrpc_cpp)
LIBXMLRPC_CPP_A            = $(BLDDIR)/src/cpp/libxmlrpc_cpp.a
LIBXMLRPCPP                = \
  $(call shliblefn, $(BLDDIR)/src/cpp/libxmlrpc++)
LIBXMLRPCPP_A              = $(BLDDIR)/src/cpp/libxmlrpc++.a
LIBXMLRPC_PACKETSOCKET     = \
  $(call shliblefn, $(BLDDIR)/src/cpp/libxmlrpc_packetsocket)
LIBXMLRPC_PACKETSOCKET_A   = $(BLDDIR)/src/cpp/libxmlrpc_packetsocket.a
LIBXMLRPC_CLIENTPP         =  \
  $(call shliblefn, $(BLDDIR)/src/cpp/libxmlrpc_client++)
LIBXMLRPC_CLIENTPP_A       = $(BLDDIR)/src/cpp/libxmlrpc_client++.a
LIBXMLRPC_SERVERPP         = \
  $(call shliblefn, $(BLDDIR)/src/cpp/libxmlrpc_server++)
LIBXMLRPC_SERVERPP_A       = $(BLDDIR)/src/cpp/libxmlrpc_server++.a
LIBXMLRPC_SERVER_ABYSSPP   = \
  $(call shliblefn, $(BLDDIR)/src/cpp/libxmlrpc_server_abyss++)
LIBXMLRPC_SERVER_ABYSSPP_A = $(BLDDIR)/src/cpp/libxmlrpc_server_abyss++.a
LIBXMLRPC_SERVER_PSTREAMPP = \
  $(call shliblefn, $(BLDDIR)/src/cpp/libxmlrpc_server_pstream++)
LIBXMLRPC_SERVER_PSTREAMPP_A = $(BLDDIR)/src/cpp/libxmlrpc_server_pstream++.a
endif

# LIBXMLRPC_XML is the list of Xmlrpc-c libraries we need to parse
# XML.  If we're using an external library to parse XML, this is null.
# LDLIBS_XML is the corresponding -L/-l options

ifneq ($(ENABLE_LIBXML2_BACKEND),yes)
  # We're using the internal Expat XML parser
  LIBXMLRPC_XML = $(LIBXMLRPC_XMLPARSE) $(LIBXMLRPC_XMLTOK)
  LDLIBS_XML = \
        -L$(BLDDIR)/lib/expat/xmlparse -lxmlrpc_xmlparse \
        -L$(BLDDIR)/lib/expat/xmltok   -lxmlrpc_xmltok
else
  LDLIBS_XML = $(shell xml2-config --libs)
endif


##############################################################################
#            RULES TO BUILD OBJECT FILES TO LINK INTO LIBRARIES              #
##############################################################################

# The including make file sets TARGET_MODS to a list of all modules that
# might go into a library.  Its a list of the bare module names.  The
# including make file also sets INCLUDES, in a target-dependent manner,
# to the string of -I options needed for each target.  Example:

#   TARGET_MODS = foo bar
#
#   foo.o foo.osh: INCLUDES = -Iinclude -I/usr/include/foostuff
#   bar.o bar.osh: INCLUDES = -Iinclude -I/usr/include/barstuff
#
#   include Makefile.common
#
# The above generates rules to build foo.o, bar.o, foo.osh, and bar.osh
#
# For C++ source files, use TARGET_MODS_PP instead.

# CFLAGS and CXXFLAGS are designed to be overridden on the make command
# line.  We pile all the options except -I into these variables so the
# user can override them all if he wants.

$(TARGET_MODS:%=%.o):%.o:%.c
	$(CC) -c -o $@ $(INCLUDES) $(CFLAGS) $<

$(TARGET_MODS:%=%.osh): CFLAGS_COMMON += $(CFLAGS_SHLIB)

$(TARGET_MODS:%=%.osh):%.osh:%.c
	$(CC) -c -o $@ $(INCLUDES) $(CFLAGS) $(CFLAGS_SHLIB) $<

$(TARGET_MODS_PP:%=%.o):%.o:%.cpp
	$(CXX) -c -o $@ $(INCLUDES) $(CXXFLAGS) $<

$(TARGET_MODS_PP:%=%.osh): CXXFLAGS_COMMON += $(CFLAGS_SHLIB)

$(TARGET_MODS_PP:%=%.osh):%.osh:%.cpp
	$(CXX) -c -o $@ $(INCLUDES) $(CXXFLAGS) $<


##############################################################################
#                           MISC BUILD RULES                                 #
##############################################################################

# We use the srcdir symbolic link simply to make the make
# rules easier to read in the make output.  We could use the $(SRCDIR)
# variable, but that makes the compile and link commands
# a mile long.  Note that Make sometime figures that a directory which
# is a dependency is newer than the symbolic link pointing to it and wants
# to rebuild the symbolic link.  So we don't make $(SRCDIR) a
# dependency of 'srcdir'.

# We should do the same for 'blddir'.  We did once before, then undid
# it in an erroneous effort to enable parallel make.  It's a little harder
# with blddir; when we did it before, we had to use the non-symlink
# version in a few places.

srcdir:
	$(LN_S) $(SRCDIR) $@
blddir:
	$(LN_S) $(BLDDIR) $@

##############################################################################
#                    RECURSIVE SUBDIRECTORY BUILD RULES                      #
##############################################################################

.PHONY: $(SUBDIRS:%=%/all)
$(SUBDIRS:%=%/all): %/all: $(CURDIR)/%
	$(MAKE) -C $(dir $@) -f $(SRCDIR)/$(SUBDIR)/$(dir $@)Makefile \
	    $(notdir $@) 

.PHONY: $(SUBDIRS:%=%/install)
$(SUBDIRS:%=%/install): %/install: $(CURDIR)/%
	$(MAKE) -C $(dir $@) -f $(SRCDIR)/$(SUBDIR)/$(dir $@)Makefile \
	    $(notdir $@) 

.PHONY: $(SUBDIRS:%=%/clean)
$(SUBDIRS:%=%/clean): %/clean: $(CURDIR)/%
	$(MAKE) -C $(dir $@) -f $(SRCDIR)/$(SUBDIR)/$(dir $@)Makefile \
	    $(notdir $@) 

.PHONY: $(SUBDIRS:%=%/distclean)
$(SUBDIRS:%=%/distclean): %/distclean: $(CURDIR)/%
	$(MAKE) -C $(dir $@) -f $(SRCDIR)/$(SUBDIR)/$(dir $@)Makefile \
	    $(notdir $@) 

.PHONY: $(SUBDIRS:%=%/check)
$(SUBDIRS:%=%/check): %/check: $(CURDIR)/%
	$(MAKE) -C $(dir $@) -f $(SRCDIR)/$(SUBDIR)/$(dir $@)Makefile \
	    $(notdir $@) 

.PHONY: $(SUBDIRS:%=%/distdir)
$(SUBDIRS:%=%/distdir): %/distdir: $(CURDIR)/%
	$(MAKE) -C $(dir $@) -f $(SRCDIR)/$(SUBDIR)/$(dir $@)Makefile \
	    $(notdir $@) 

.PHONY: $(SUBDIRS:%=%/dep)
$(SUBDIRS:%=%/dep): %/dep: $(CURDIR)/%
	$(MAKE) -C $(dir $@) -f $(SRCDIR)/$(SUBDIR)/$(dir $@)Makefile \
	    $(notdir $@) 


##############################################################################
#                         CROSS-COMPONENT BUILD RULES                        #
##############################################################################

ifneq ($(OMIT_WININET_TRANSPORT_RULE),Y)
$(BLDDIR)/lib/wininet_transport/xmlrpc_wininet_transport.o \
$(BLDDIR)/lib/wininet_transport/xmlrpc_wininet_transport.osh \
: FORCE
	$(MAKE) -C $(dir $@) -f $(SRCDIR)/lib/wininet_transport/Makefile \
	    $(notdir $@)
endif

ifneq ($(OMIT_CURL_TRANSPORT_RULE),Y)
$(BLDDIR)/lib/curl_transport/xmlrpc_curl_transport.o \
$(BLDDIR)/lib/curl_transport/xmlrpc_curl_transport.osh \
: FORCE
	$(MAKE) -C $(dir $@) -f $(SRCDIR)/lib/curl_transport/Makefile \
	    $(notdir $@)
endif

ifneq ($(OMIT_LIBWWW_TRANSPORT_RULE),Y)
$(BLDDIR)/lib/libwww_transport/xmlrpc_libwww_transport.o \
$(BLDDIR)/lib/libwww_transport/xmlrpc_libwww_transport.osh \
: FORCE
	$(MAKE) -C $(dir $@) -f $(SRCDIR)/lib/libwww_transport/Makefile \
	    $(notdir $@)
endif

$(LIBXMLRPC) \
$(LIBXMLRPC_CLIENT) \
$(LIBXMLRPC_SERVER) \
$(LIBXMLRPC_SERVER_ABYSS) \
$(LIBXMLRPC_SERVER_CGI) \
$(LIBXMLRPC_A) \
$(LIBXMLRPC_CLIENT_A) \
$(LIBXMLRPC_SERVER_A) \
$(LIBXMLRPC_SERVER_ABYSS_A) \
$(LIBXMLRPC_SERVER_CGI_A): FORCE
	$(MAKE) -C $(dir $@) -f $(SRCDIR)/src/Makefile \
	    $(notdir $@)

$(LIBXMLRPC_UTIL) $(LIBXMLRPC_UTIL_A) : FORCE
	$(MAKE) -C $(dir $@) -f $(SRCDIR)/lib/libutil/Makefile \
	    $(notdir $@)

$(LIBXMLRPC_XMLPARSE) $(LIBXMLRPC_XMLPARSE_A) : FORCE
	$(MAKE) -C $(dir $@) -f $(SRCDIR)/lib/expat/xmlparse/Makefile \
	    $(notdir $@)

$(LIBXMLRPC_XMLTOK) $(LIBXMLRPC_XMLTOK_A) : FORCE
	$(MAKE) -C $(dir $@) -f $(SRCDIR)/lib/expat/xmltok/Makefile \
	    $(notdir $@)

$(LIBXMLRPC_ABYSS) $(LIBXMLRPC_ABYSS_A): FORCE
	$(MAKE) -C $(dir $@) -f $(SRCDIR)/lib/abyss/src/Makefile \
	    $(notdir $@)

$(LIBXMLRPCPP) $(LIBXMLRPCPP_A) \
$(LIBXMLRPC_PACKETSOCKET) $(LIBXMLRPC_PACKETSOCKET_A) \
$(LIBXMLRPC_CLIENTPP) $(LIBXMLRPC_CLIENTPP_A) \
$(LIBXMLRPC_SERVERPP) $(LIBXMLRPC_SERVERPP_A) \
$(LIBXMLRPC_SERVER_ABYSSPP) $(LIBXMLRPC_SERVER_ABYSSPP_A) \
$(LIBXMLRPC_SERVER_PSTREAMPP) $(LIBXMLRPC_SERVER_PSTREAMPP_A) \
$(LIBXMLRPC_CPP) $(LIBXMLRPC_CPP_A) : FORCE
	$(MAKE) -C $(dir $@) -f $(SRCDIR)/src/cpp/Makefile \
	    $(notdir $@)

# For the following utilities, we don't bother with a library -- we
# just explicitly link the object file we need.  This is to save
# complexity.  If the list gets too big, we may need a library just to
# keep link commands short.

UTIL_DIR = $(BLDDIR)/lib/util

UTILS = \
  casprintf.o \
  cmdline_parser.o \
  cmdline_parser_cpp.o \
  getoptx.o \
  stripcaseeq.o \
  string_parser.o \

ifneq ($(OMIT_UTILS_RULE),Y)
$(UTILS:%=$(UTIL_DIR)/%): FORCE
	$(MAKE) -C $(dir $@) -f $(SRCDIR)/lib/util/Makefile \
	    $(notdir $@)
endif

CASPRINTF = $(UTIL_DIR)/casprintf.o


# About version.h:  This is a built header file, which means it is a supreme
# pain in the ass.  The biggest problem is that when we automatically make
# dependencies (Makefile.depend), it doesn't exist yet.  This means Gcc
# generates a dependency on it being in the local directory.  Therefore,
# we generate it in the local directory, as a symbolic link, wherever it
# is needed.  But the original is always in the top level directory,
# generated by a rule in that directory's make file.  Problem 2 is that
# the top directory's make file includes Makefile.common, so the rules
# below conflict with it.  That's what OMIT_VERSION_H is for.

ifneq ($(OMIT_VERSION_H),Y)

$(BLDDIR)/version.h:
	$(MAKE) -C $(dir $@) -f $(SRCDIR)/GNUmakefile $(notdir $@)

version.h: $(BLDDIR)/version.h
	$(LN_S) $< $@

endif

ifneq ($(OMIT_CONFIG_H_RULE),Y)
$(BLDDIR)/include/xmlrpc-c/config.h:
	$(MAKE) -C $(BLDDIR)/include -f $(SRCDIR)/include/Makefile \
          xmlrpc-c/config.h
endif

ifneq ($(OMIT_TRANSPORT_CONFIG_H),Y)
$(BLDDIR)/transport_config.h:
	$(MAKE) -C $(dir $@) -f $(SRCDIR)/GNUmakefile $(notdir $@)
endif

ifneq ($(OMIT_XMLRPC_C_CONFIG_TEST),Y)
$(BLDDIR)/xmlrpc-c-config.test:
	$(MAKE) -C $(dir $@) -f $(SRCDIR)/GNUmakefile $(notdir $@)
endif

$(TARGET_MODS:%=%.o) $(TARGET_MODS:%=%.osh): \
  $(BLDDIR)/include/xmlrpc-c/config.h

# With a separate build directory, you have to make the directory itself
# before you can make anything in it.  Here's the rule to do that.
$(SUBDIRS:%=$(CURDIR)/%):
	mkdir $@


##############################################################################
#                           INSTALL RULES                                    #
#                      (except shared libraries)                             #
##############################################################################

MKINSTALLDIRS = $(SHELL) $(SRCDIR)/mkinstalldirs

.PHONY: install-common install-ltlibraries install-headers install-bin
install-common: \
  install-ltlibraries install-static-libraries install-shared-libraries \
  install-headers install-bin

INSTALL_LIB_CMD = $(INSTALL_DATA) $$p $(DESTDIR)$(LIBINST_DIR)/$$p
RANLIB_CMD = $(RANLIB) $(DESTDIR)$(LIBINST_DIR)/$$p

install-static-libraries: $(STATIC_LIBRARIES_TO_INSTALL)
	$(MKINSTALLDIRS) $(DESTDIR)$(LIBINST_DIR)
	@list='$(STATIC_LIBRARIES_TO_INSTALL)'; for p in $$list; do \
	  if test -f $$p; then \
	    echo " $(INSTALL_LIB_CMD)"; \
	    $(INSTALL_LIB_CMD); \
	  else :; fi; \
	done
	@$(POST_INSTALL)
	@list='$(STATIC_LIBRARIES_TO_INSTALL)'; for p in $$list; do \
	  if test -f $$p; then \
	    echo " $(RANLIB_CMD)"; \
	    $(RANLIB_CMD); \
	  else :; fi; \
	done

HEADERDESTDIR = $(DESTDIR)$(HEADERINST_DIR)
INSTALL_HDR_CMD = $(INSTALL_DATA) $$d$$p $(HEADERDESTDIR)/$$p 

install-headers: $(HEADERS_TO_INSTALL)
	$(MKINSTALLDIRS) $(HEADERDESTDIR)
	$(MKINSTALLDIRS) $(HEADERDESTDIR)/xmlrpc-c
	@list='$(HEADERS_TO_INSTALL)'; for p in $$list; do \
	  if test -f "$$p"; then d= ; else d="$(SRCDIR)/$(SUBDIR)/"; fi; \
	  echo " $(INSTALL_HDR_CMD)"; \
	  $(INSTALL_HDR_CMD); \
	done


INSTALL_PROGRAM_CMD = $(INSTALL_PROGRAM) $$p $(DESTDIR)$(PROGRAMINST_DIR)/$$p

install-bin: $(PROGRAMS_TO_INSTALL) $(DESTDIR)$(PROGRAMINST_DIR)
	@list='$(PROGRAMS_TO_INSTALL)'; \
         for p in $$list; do \
	   echo "$(INSTALL_PROGRAM_CMD)"; \
	   $(INSTALL_PROGRAM_CMD); \
	   done

$(DESTDIR)$(PROGRAMINST_DIR):
	$(MKINSTALLDIRS) $@


##############################################################################
#                           MISCELLANEOUS RULES                              #
##############################################################################

.PHONY: clean-common
clean-common:
	rm -f *.o *.osh *.a *.s *.i *.la *.lo
	rm -f *.$(SHLIB_SUFFIX) *.$(SHLIB_SUFFIX).*
	rm -rf .libs
ifneq ($(OMIT_VERSION_H),Y)
	rm -f version.h
endif

.PHONY: distclean-common
distclean-common:
# Makefile.depend is generated by 'make dep' and contains only dependencies
# that make parts get _rebuilt_ when parts upon which they depend change.
# It does not contain dependencies that are necessary to cause a part to
# get built in the first place.  E.g. if foo.c uses bar.h and bar.h gets built
# by a make rule, you must put the dependency of foo.c on bar.h somewhere
# besides Makefile.depend.
#
# Because of this, a user doesn't need Makefile.depend, because he
# doesn't modify source files.  A developer, on the other hand, must make his
# own Makefile.depend, because 'make dep' creates Makefile.depend with 
# absolute pathnames, specific to the developer's system.
#
# So we obliterate Makefile.depend here.  The build will automatically
# create an empty Makefile.depend when it is needed for the user.  The
# developer must do 'make dep' if he wants to edit and rebuild.
#
# Other projects have the build automatically build a true
# Makefile.depend, suitable for a developer.  We have found that to be
# an utter disaster -- it's way too complicated and prone to failure,
# especially with built .h files.  Better not to burden the user, who
# gains nothing from it, with that.
#
	rm -f Makefile.depend
	rm -f srcdir blddir

.PHONY: distdir-common
distdir-common:
	@for file in $(DISTFILES); do \
	  d=$(SRCDIR); \
	  if test -d $$d/$$file; then \
	    cp -pr $$d/$$file $(DISTDIR)/$$file; \
	  else \
	    test -f $(DISTDIR)/$$file \
	    || ln $$d/$$file $(DISTDIR)/$$file 2> /dev/null \
	    || cp -p $$d/$$file $(DISTDIR)/$$file || :; \
	  fi; \
	done

DEP_SOURCES = $(wildcard *.c *.cpp)

# This is a filter to turn "foo.o:" rules into "foo.o foo.lo foo.osh:"
# to make dependencies for all the various forms of object file out of
# a file made by a depedency generator that knows only about .o.

DEPEND_MASSAGER = perl -walnpe's{^(.*)\.o:}{$$1.o $$1.lo $$1.osh:}'



.PHONY: dep-common
dep-common: FORCE
ifneq ($(DEP_SOURCES)x,x)
	-$(CC) -MM -MG -I. $(INCLUDES) $(DEP_SOURCES) | \
	  $(DEPEND_MASSAGER) \
	  >Makefile.depend
endif

Makefile.depend:
	cat /dev/null >$@

# The automatic dependency generation is a pain in the butt and
# totally unnecessary for people just installing the distributed code,
# so to avoid needless failures in the field and a complex build, the
# 'distclean' target simply makes Makefile.depend an empty file.  A
# developer may do 'make dep' to create a Makefile.depend full of real
# dependencies.

# Tell versions [3.59,3.63) of GNU make to not export all variables.
# Otherwise a system limit (for SysV at least) may be exceeded.
.NOEXPORT:


# Use the FORCE target as a dependency to force a target to get remade
FORCE:
