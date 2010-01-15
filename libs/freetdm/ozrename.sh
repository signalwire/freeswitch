# renaming main header and build file
copy="cp -r"

$copy src/include/openzap.h src/include/freetdm.h
svn delete src/include/openzap.h
$copy openzap.pc.in freetdm.pc.in
svn delete openzap.pc.in

# create mod_freetdm
mkdir mod_freetdm
cp mod_openzap/* mod_freetdm/
mv mod_freetdm/mod_openzap.c mod_freetdm/mod_freetdm.c
svn delete --force mod_openzap


##### ozmod stuff ####
# rename anything ozmod to ftmod, including directories first
mkdir ./src/ftmod
for file in `find ./src/ozmod -name *ozmod_* -type d`
do
	$copy ${file} ${file//ozmod/ftmod}
done

#remove .svn directories in the copied ozmod dirs
find ./src/ftmod -name *.svn -exec rm -rf {} \;

# copy ozmod c files
for file in `find ./src/ftmod -name *ozmod_*.c`
do
	mv $file ${file//ozmod/ftmod}
done

# copy ozmod h files
for file in `find ./src/ftmod -name *ozmod_*.h`
do
	mv $file ${file//ozmod/ftmod}
done

#### end ozmod stuff ####

# renaming other zap files
for file in `find ./ -name *zap_*.c`
do
	mv $file ${file//zap_/ftdm_}
done

for file in `find ./ -name *zap_*.h`
do
	mv $file ${file//zap_/ftdm_}
done

svn revert -R src/ozmod
svn delete --force src/ozmod

# replace full openzap occurences first (handles openzap.h, libopenzap etc)
find ./ -name *.c -exec sed -i 's,openzap,freetdm,g' {} \;

find ./ -name *.h -exec sed -i 's,openzap,freetdm,g' {} \;

sed -i 's,openzap,freetdm,g' Makefile.am

sed -i 's,openzap,freetdm,g' configure.ac

sed -i 's,openzap,freetdm,g' mod_freetdm/Makefile.in

# replace inside files
find ./ -name *.c -exec sed -i 's,oz,ft,g' {} \;
find ./ -name *.c -exec sed -i 's,OZ,FT,g' {} \;
find ./ -name *.c -exec sed -i 's,zap,ftdm,g' {} \;
find ./ -name *.c -exec sed -i 's,ZAP,FTDM,g' {} \;
find ./ -name *.c -exec sed -i 's,zchan,ftdmchan,g' {} \;

find ./ -name *.h -exec sed -i 's,oz,ft,g' {} \;
find ./ -name *.h -exec sed -i 's,OZ,FT,g' {} \;
find ./ -name *.h -exec sed -i 's,zap,ftdm,g' {} \;
find ./ -name *.h -exec sed -i 's,ZAP,FTDM,g' {} \;
find ./ -name *.h -exec sed -i 's,zchan,ftdmchan,g' {} \;

sed -i 's,oz,ft,g' Makefile.am 
sed -i 's,OZ,FT,g' Makefile.am
sed -i 's,zap,ftdm,g' Makefile.am
sed -i 's,ZAP,FTDM,g' Makefile.am
sed -i 's,zchan,ftdmchan,g' Makefile.am

sed -i 's,oz,ft,g' configure.ac
sed -i 's,OZ,FT,g' configure.ac
sed -i 's,zap,ftdm,g' configure.ac
sed -i 's,ZAP,FTDM,g' configure.ac
sed -i 's,zchan,ftdmchan,g' configure.ac

sed -i 's,oz,ft,g' mod_freetdm/Makefile.in
sed -i 's,OZ,FT,g' mod_freetdm/Makefile.in
sed -i 's,zap,ftdm,g' mod_freetdm/Makefile.in
sed -i 's,ZAP,FTDM,g' mod_freetdm/Makefile.in
sed -i 's,zchan,ftdmchan,g' mod_freetdm/Makefile.in

svn add src/ftmod/
svn add mod_freetdm/

