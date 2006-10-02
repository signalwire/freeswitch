if [ `uname -s` = "NetBSD" ] ; then
config_TARGET_READLINE_LIBS="-L/usr/pkg/lib -lreadline" ./configure $@
else
./configure $@
fi
$MAKE 
$MAKE install
