\Qt\4.6.0\bin\qmake.exe -spec win32-msvc2008 -tp vc
copy fscomm.vcproj FSComm.2008.vcproj /y
echo "Manually copy/replace $QTDIR environment variable into project file"
echo "Push top GUID block back current"
pause