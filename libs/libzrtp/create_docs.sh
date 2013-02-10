cd ../../doc
rm -f docs.tar.gz
rm -rf libzrtp-doc
echo "=================> start doxygen."
doxygen > /dev/null 2>&1
mkdir libzrtp-doc
cp -Rf ./out/html/* ./libzrtp-doc
tar -zcvf ./libzrtp-doc.tar.gz ./libzrtp-doc >> /dev/null
rm -rf libzrtp-doc
