# Microsoft Developer Studio Project File - Name="rpctest" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=rpctest - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "rpctest.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "rpctest.mak" CFG="rpctest - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "rpctest - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "rpctest - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "rpctest - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release\rpctest"
# PROP Intermediate_Dir "Release\rpctest"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I ".." /I "../include" /I "../lib/util/include" /I "../.." /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "ABYSS_WIN32" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 ..\lib\xmlrpc.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Ws2_32.lib Wininet.lib /nologo /subsystem:console /machine:I386 /out:"..\bin\rpctest.exe"
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Desc=Copy test files
PostBuild_Cmds=if not exist ..\Bin\data md ..\Bin\data	copy ..\src\test\data\*.* ..\Bin\data
# End Special Build Tool

!ELSEIF  "$(CFG)" == "rpctest - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug\rpctest"
# PROP Intermediate_Dir "Debug\rpctest"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "../.." /I ".." /I "../include" /I "../lib/util/include" /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "ABYSS_WIN32" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 ..\lib\xmlrpcD.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Ws2_32.lib Wininet.lib /nologo /subsystem:console /debug /machine:I386 /out:"..\bin\rpctestD.exe" /pdbtype:sept
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Desc=Copy test files
PostBuild_Cmds=if not exist ..\Bin\data md ..\Bin\data	copy ..\src\test\data\*.* ..\Bin\data
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "rpctest - Win32 Release"
# Name "rpctest - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\src\test\abyss.c
# End Source File
# Begin Source File

SOURCE=..\src\test\cgi.c
# End Source File
# Begin Source File

SOURCE=..\src\test\client.c
# End Source File
# Begin Source File

SOURCE=..\lib\util\casprintf.c
# End Source File
# Begin Source File

SOURCE=..\src\test\method_registry.c
# End Source File
# Begin Source File

SOURCE=..\src\test\parse_xml.c
# End Source File
# Begin Source File

SOURCE=..\src\test\serialize.c
# End Source File
# Begin Source File

SOURCE=..\src\test\serialize_value.c
# End Source File
# Begin Source File

SOURCE=..\src\test\server_abyss.c
# End Source File
# Begin Source File

SOURCE=..\src\test\test.c
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\token.h
# End Source File
# Begin Source File

SOURCE=..\src\test\value.c
# End Source File
# Begin Source File

SOURCE=..\src\test\value_datetime.c
# End Source File
# Begin Source File

SOURCE=..\src\test\value_datetime.h
# End Source File
# Begin Source File

SOURCE=..\src\test\xml_data.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\src\test\client.h
# End Source File
# Begin Source File

SOURCE=..\src\test\parse_xml.h
# End Source File
# Begin Source File

SOURCE=..\src\test\serialize.h
# End Source File
# Begin Source File

SOURCE=..\src\test\serialize_value.h
# End Source File
# Begin Source File

SOURCE=..\src\test\server_abyss.h
# End Source File
# Begin Source File

SOURCE=..\src\test\test.h
# End Source File
# Begin Source File

SOURCE=..\src\test\value.h
# End Source File
# Begin Source File

SOURCE=..\src\test\xml_data.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Group "TestFiles"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\src\testdata\http-req-simple.txt"
# End Source File
# Begin Source File

SOURCE=..\src\testdata\req_no_params.xml
# End Source File
# Begin Source File

SOURCE=..\src\testdata\req_out_of_order.xml
# End Source File
# Begin Source File

SOURCE=..\src\testdata\req_value_name.xml
# End Source File
# End Group
# End Target
# End Project
