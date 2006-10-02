hosttype=`uname -s`

pthreadopts=""

if [ $hosttype = "FreeBSD" ] ; then

patch <<__EOF__ 
--- nsprpub/pr/include/md/_freebsd.h.orig       Sat Sep 30 22:19:12 2006
+++ nsprpub/pr/include/md/_freebsd.h    Sat Sep 30 22:23:16 2006
@@ -106,6 +106,11 @@
 #define _PR_IPV6_V6ONLY_PROBE
 #endif

+#if __FreeBSD_version >= 601103
+#define _PR_HAVE_GETPROTO_R
+#define _PR_HAVE_5_ARG_GETPROTO_R
+#endif
+
 #define USE_SETJMP

 #ifndef _PR_PTHREADS
__EOF__

cp js/src/config/Linux_All.mk js/src/config/`uname -s``uname -r`.mk

pthreadopts=" --with-pthreads=yes"
fi

if [ $hosttype = "OpenBSD" ] ; then
cp js/src/config/Linux_All.mk js/src/config/`uname -s``uname -r`.mk
fi

if [ $hosttype = "NetBSD" ] ; then
cp js/src/config/Linux_All.mk js/src/config/`uname -s``uname -r`.mk
fi

arch=`uname -m`

opts=""
if [ $arch = "x86_64" ] ; then
opts="--enable-64bit"
fi

cd nsprpub && ./configure $opts $pthreadopts && $MAKE

cd ../js/src && JS_THREADSAFE=1 JS_HAS_FILE_OBJECT=1 OTHER_LIBS="-L../../../mozilla/nsprpub/dist/lib" INCLUDES="-I../../../mozilla/nsprpub/dist/include/nspr"  $MAKE -f Makefile.ref `find . -name libjs.a`

