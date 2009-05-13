# DO NOT EDIT. AUTOMATICALLY GENERATED.

crypto/apr_md5.lo: crypto/apr_md5.c .make.dirs include/apr_xlate.h include/apr_md5.h include/apr_sha1.h
crypto/uuid.lo: crypto/uuid.c .make.dirs include/apr_uuid.h
crypto/apr_sha1.lo: crypto/apr_sha1.c .make.dirs include/apr_xlate.h include/apr_sha1.h include/apr_base64.h
crypto/getuuid.lo: crypto/getuuid.c .make.dirs include/apr_uuid.h include/apr_md5.h include/apr_xlate.h
crypto/apr_md4.lo: crypto/apr_md4.c .make.dirs include/apr_md4.h include/apr_xlate.h
encoding/apr_base64.lo: encoding/apr_base64.c .make.dirs include/apr_base64.h include/apr_xlate.h
hooks/apr_hooks.lo: hooks/apr_hooks.c .make.dirs include/apr_optional_hooks.h include/apr_optional.h include/apr_hooks.h
misc/apr_reslist.lo: misc/apr_reslist.c .make.dirs include/apr_reslist.h
misc/apr_rmm.lo: misc/apr_rmm.c .make.dirs include/apr_rmm.h include/apr_anylock.h
misc/apr_date.lo: misc/apr_date.c .make.dirs include/apr_date.h
misc/apu_version.lo: misc/apu_version.c .make.dirs include/apu_version.h
misc/apr_queue.lo: misc/apr_queue.c .make.dirs include/apr_queue.h
uri/apr_uri.lo: uri/apr_uri.c .make.dirs include/apr_uri.h
xml/apr_xml.lo: xml/apr_xml.c .make.dirs include/apr_xml.h include/apr_xlate.h
strmatch/apr_strmatch.lo: strmatch/apr_strmatch.c .make.dirs include/apr_strmatch.h
xlate/xlate.lo: xlate/xlate.c .make.dirs include/apr_xlate.h

OBJECTS_all = crypto/apr_md5.lo crypto/uuid.lo crypto/apr_sha1.lo crypto/getuuid.lo crypto/apr_md4.lo encoding/apr_base64.lo hooks/apr_hooks.lo misc/apr_reslist.lo misc/apr_rmm.lo misc/apr_date.lo misc/apu_version.lo misc/apr_queue.lo uri/apr_uri.lo xml/apr_xml.lo strmatch/apr_strmatch.lo xlate/xlate.lo

OBJECTS_unix = $(OBJECTS_all)

OBJECTS_aix = $(OBJECTS_all)

OBJECTS_beos = $(OBJECTS_all)

OBJECTS_os2 = $(OBJECTS_all)

OBJECTS_os390 = $(OBJECTS_all)

HEADERS = $(top_srcdir)/include/apr_optional.h $(top_srcdir)/include/apu_version.h $(top_srcdir)/include/apr_strmatch.h $(top_srcdir)/include/apr_optional_hooks.h $(top_srcdir)/include/apr_sdbm.h $(top_srcdir)/include/apr_md4.h $(top_srcdir)/include/apr_reslist.h $(top_srcdir)/include/apr_base64.h $(top_srcdir)/include/apr_xml.h $(top_srcdir)/include/apr_anylock.h $(top_srcdir)/include/apr_rmm.h $(top_srcdir)/include/apr_md5.h $(top_srcdir)/include/apr_date.h $(top_srcdir)/include/apr_hooks.h $(top_srcdir)/include/apr_xlate.h $(top_srcdir)/include/apr_queue.h $(top_srcdir)/include/apr_uri.h $(top_srcdir)/include/apr_uuid.h $(top_srcdir)/include/apr_sha1.h

SOURCE_DIRS = xml encoding hooks misc crypto uri strmatch xlate $(EXTRA_SOURCE_DIRS)

BUILD_DIRS = crypto encoding hooks misc strmatch uri xlate xml

.make.dirs: $(srcdir)/build-outputs.mk
	@for d in $(BUILD_DIRS); do test -d $$d || mkdir $$d; done
	@echo timestamp > $@
