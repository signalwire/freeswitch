NEW_MICRO=4
rm -rf freeswitch.tag.working
svn co http://svn.freeswitch.org/svn/freeswitch/trunk freeswitch.tag.working
cd freeswitch.tag.working
SWITCH_VERSION_MAJOR=`grep SWITCH_VERSION_MAJOR configure.in | sed "s|.*\[||" | sed "s|\].*||"`
SWITCH_VERSION_MINOR=`grep SWITCH_VERSION_MINOR configure.in | sed "s|.*\[||" | sed "s|\].*||"`
SWITCH_VERSION_MICRO=`grep SWITCH_VERSION_MICRO configure.in | sed "s|.*\[||" | sed "s|\].*||"`
FREESWITCH_VERSION=$((`svnversion .` + 1))
cat configure.in | sed "s|$SWITCH_VERSION_MICRO|$NEW_MICRO|" | sed "s|svn-revision-here|$FREESWITCH_VERSION|" | sed "s|#AC_SUBST(SWITCH_VERSION_REVISION|AC_SUBST(SWITCH_VERSION_REVISION|" > configure.tmp
cp -f configure.tmp configure.in
rm -f configure.tmp
cd libs/openzap/
OPENZAP_VERSION=$((`svnversion .` + 1))
svn copy . http://svn.openzap.org:81/svn/openzap/tags/v$OPENZAP_VERSION -m"tag"
cd ../..
echo 'cat $1 | sed "s|openzap/trunk|openzap/tags/v'$OPENZAP_VERSION'|" > svn-temp-working' > externaleditor.sh
echo 'cp -f svn-temp-working $1' >> externaleditor.sh
echo 'rm -f svn-temp-working' >> externaleditor.sh
chmod a+x externaleditor.sh
svn propedit svn:externals . --editor-cmd=./externaleditor.sh
svn copy . http://svn.freeswitch.org/svn/freeswitch/tags/$SWITCH_VERSION_MAJOR.$SWITCH_VERSION_MINOR.$NEW_MICRO -m"tag"
cd ..
rm -rf freeswitch.tag.working
svn co http://svn.freeswitch.org/svn/freeswitch/tags/$SWITCH_VERSION_MAJOR.$SWITCH_VERSION_MINOR.$NEW_MICRO freeswitch-$SWITCH_VERSION_MAJOR.$SWITCH_VERSION_MINOR.$NEW_MICRO
cd freeswitch-$SWITCH_VERSION_MAJOR.$SWITCH_VERSION_MINOR.$NEW_MICRO
./bootstrap.sh
rm -rf `find . -name .svn`
mv bootstrap.sh rebootstrap.sh
rm -f docs/AUTHORS
rm -f docs/COPYING
rm -f docs/ChangeLog
cd ..
tar -czvf freeswitch-$SWITCH_VERSION_MAJOR.$SWITCH_VERSION_MINOR.$NEW_MICRO.tar.gz freeswitch-$SWITCH_VERSION_MAJOR.$SWITCH_VERSION_MINOR.$NEW_MICRO/
tar -cjvf freeswitch-$SWITCH_VERSION_MAJOR.$SWITCH_VERSION_MINOR.$NEW_MICRO.tar.bz2 freeswitch-$SWITCH_VERSION_MAJOR.$SWITCH_VERSION_MINOR.$NEW_MICRO/
