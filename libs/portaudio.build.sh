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
__EOF__

arch=`uname -m`

opts=""
if [ $arch = "x86_64" ] ; then
opts="-fPIC"
fi

CFLAGS=$opts ./configure $@
$MAKE 
$MAKE install




