##### ozmod stuff ####
# replace full openzap occurences first (handles openzap.h, libopenzap etc)
sed -i 's,openzap,freetdm,g' $1
sed -i 's,ozmod,ftmod,g' $1

sed -i 's,zchan,ftdmchan,g' $1
sed -i 's,oz,ft,g' $1
sed -i 's,OZ,FT,g' $1
sed -i 's,zap,ftdm,g' $1
sed -i 's,ZAP,FTDM,g' $1
sed -i 's,zio,fio,g' $1
sed -i 's,ZIO,FIO,g' $1

