# renaming main header and build file
move="mv"
$move src/include/openzap.h src/include/freetdm.h
$move openzap.pc.in freetdm.pc.in
$move mod_openzap/mod_openzap.c mod_openzap/mod_freetdm.c
$move mod_openzap mod_freetdm

# rename anything ozmod to ftmod, including directories first
$move src/ozmod src/ftmod
for file in `find ./ -name *ozmod_* -type d`
do
	$move $file ${file//ozmod/ftmod}
done

# move ozmod c files
for file in `find ./ -name *ozmod_*.c`
do
	$move $file ${file//ozmod/ftmod}
done

# move ozmod h files
for file in `find ./ -name *ozmod_*.h`
do
	$move $file ${file//ozmod/ftmod}
done

# renaming other files
for file in `find ./ -name *zap_*.c`
do
	$move $file ${file//zap_/ftdm_}
done

for file in `find ./ -name *zap_*.h`
do
	$move $file ${file//zap_/ftdm_}
done

# replace full openzap occurences first (handles openzap.h, libopenzap etc)
find ./ -name *.c -exec sed -i 's,openzap,freetdm,g' {} \;

find ./ -name *.h -exec sed -i 's,openzap,freetdm,g' {} \;

sed -i 's,openzap,freetdm,g' Makefile.am

sed -i 's,openzap,freetdm,g' configure.ac

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


