NEW_MICRO=4pre3
rm -rf freeswitch-1.0.$(NEW_MICRO)
svn co http://svn.freeswitch.org/svn/freeswitch/trunk freeswitch-1.0.$(NEW_MICRO)
cd freeswitch-1.0.$(NEW_MICRO)
SWITCH_VERSION_MAJOR=`grep SWITCH_VERSION_MAJOR configure.in | sed "s|.*\[||" | sed "s|\].*||"`
SWITCH_VERSION_MINOR=`grep SWITCH_VERSION_MINOR configure.in | sed "s|.*\[||" | sed "s|\].*||"`
SWITCH_VERSION_MICRO=`grep SWITCH_VERSION_MICRO configure.in | sed "s|.*\[||" | sed "s|\].*||"`
FREESWITCH_VERSION=$((`svnversion .` + 1))
cat configure.in | sed "s|$SWITCH_VERSION_MICRO|$NEW_MICRO|" | sed "s|svn-revision-here|$FREESWITCH_VERSION|" | sed "s|#AC_SUBST(SWITCH_VERSION_REVISION|AC_SUBST(SWITCH_VERSION_REVISION|" > configure.tmp
cp -f configure.tmp configure.in
rm -f configure.tmp
./bootstrap.sh
rm -f docs/AUTHORS
rm -f docs/COPYING
rm -f docs/ChangeLog
cd ..
tar -czvf freeswitch-$SWITCH_VERSION_MAJOR.$SWITCH_VERSION_MINOR.$NEW_MICRO.tar.gz freeswitch-$SWITCH_VERSION_MAJOR.$SWITCH_VERSION_MINOR.$NEW_MICRO/
tar -cjvf freeswitch-$SWITCH_VERSION_MAJOR.$SWITCH_VERSION_MINOR.$NEW_MICRO.tar.bz2 freeswitch-$SWITCH_VERSION_MAJOR.$SWITCH_VERSION_MINOR.$NEW_MICRO/
