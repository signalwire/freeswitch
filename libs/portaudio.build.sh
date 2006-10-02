patch <<__EOF__
--- configure.orig	2006-09-24 23:56:36.000000000 -0700
+++ configure	2006-09-24 23:57:32.000000000 -0700
@@ -1886,7 +1886,7 @@
 	OTHER_OBJS="pa_mac_core/pa_mac_core.o";
 	LIBS="-framework CoreAudio -lm";
 	PADLL="libportaudio.dylib";
-	SHARED_FLAGS="-framework CoreAudio -dynamiclib";
+	SHARED_FLAGS="-framework CoreAudio -dynamiclib -install_name \\\$(PREFIX)/lib/\\\$(PADLL)";
 	;;
 
   mingw* )
 
--- pa_unix_oss/pa_unix_oss.c.orig      Sun Oct  1 19:57:15 2006
+++ pa_unix_oss/pa_unix_oss.c   Sun Oct  1 20:00:01 2006
@@ -95,7 +95,9 @@

 #include <stdio.h>
 #include <stdlib.h>
+#if !defined(__FreeBSD__) && !defined(__OpenBSD__)
 #include <malloc.h>
+#endif
 #include <memory.h>
 #include <math.h>
 #include <sys/ioctl.h>
@@ -109,6 +111,10 @@

 #ifdef __linux__
 #include <linux/soundcard.h>
+#elif defined(__FreeBSD__) || defined(NetBSD)
+#include <sys/soundcard.h>
+#elif defined(__OpenBSD__)
+#include <soundcard.h>
 #else
 #include <machine/soundcard.h> /* JH20010905 */
 #endif
__EOF__

arch=`uname -m`

opts=""
if [ $arch = "x86_64" ] ; then
opts="-fPIC"
fi
if [ $arch = "amd64" ] ; then
opts="-fPIC"
fi

CFLAGS=$opts ./configure $@
$MAKE 
$MAKE install




