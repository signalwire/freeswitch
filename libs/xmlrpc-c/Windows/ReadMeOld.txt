Current information is in ReadMeWin32.txt.

What follows is previous contents of that file.



Markus Hoffrogge:

Compilation for WIN32 is possible now again for version 1.6.10 - non
CPP parts only.  Project files have been tested under VicualStudio
6.0.  Build project all - this will build all other projects.  CPP
code is not compiled and adopted right now.

This release excludes the option to compile an "http.sys" version of
an XML-RPC server. If you do wish to build in the http.sys server, set
the MUST_BUILD_HTTP_SYS_SERVER to 1 in the transport_config_win32.h
and/or the transport_config.h file.  Successful conpilation requires
installation of the Microsoft Platform SDK for Windows XP SP2 (or
later) to get the latest header and link libraries required to support
this functionality.  After installation, be sure to properly register
the directories as documented in the Platform SDK help file topic
"Installing the Platform SDK with Visual Studio".  Download the
Platform SDK from:
http://www.microsoft.com/msdownload/platformsdk/sdkupdate/

To create the three headers required for Win32 WinInet compilation,
run the ConfigureWin32.bat found in the Windows directory.  If you
wish to alter the transports that are built to include curl or libwww,
adjust the preprocessor definitions at the top of the
transport_config_win32.h and/or the transport_config.h files.  See the
file UsingCURLinWin32.txt for more information on using the curl
transport.

To compile, open the xmlrpc.dsw file in Visual Studio 6 or greater.
The project will convert and work fine in Visual Studio 2003 as well -
other versions of Visual Studio were not tested.

NOTE: If you get an error while opening or converting the project
files, it is likely due to using WinRar or similar to decompress the
distribution tarball.  You can use WinZip or another utility to
correctly decompress the .tgz file.

Suggested testing for evaluation of the library involves a few
projects.  Here is a quick getting started guide:

1) Set the Active Project to query_meerkat and build it in release or
   debug modes.  The dependent projects will be built automatically.
   In the project settings dialog, add the argument for what you wish
   to query meerkat for - "Windows" is a good query.  Run the project.
   This will query the meerkat server for articles related to windows
   and output the results to the console.

2) Set the Active Project to xmlrpc_sample_add_server and build it in
   release or debug modes.  The dependent projects will be built
   automatically.  In the project settings dialog, add the argument
   for the port to 8080.  This will run the server sample which adds
   two numbers and returns a result.  You should run this from a
   command prompt instead of through Visual Studio so you may run the
   sample client as well.

3) Set the Active Project to xmlrpc_sample_add_sync_client or
   xmlrpc_sample_add_async_client and build it in release or debug
   modes.  The dependent projects will be built automatically.  This
   will run the client sample which submits two numbers to be added to
   the server application as described above and displays the result.
   Note that the client example comes in the sync and async varieties.

Steven Bone
July 27, 2005
sbone@pobox.com

WIN32 CHANGES

Changes from the 1.02 release for Win32:

1) Option to easily disable the http.sys server for those who do not
   need it or wish to download the Platform SDK.

Changes from the 1.01 -> 1.02 release for Win32:

1) Project files for gennmtab, xmlparse, and xmltok updated to include the
   path to the xmlrpc_config.h file.

2) Bugfix for WinInet authentication.

3) Supports xmlrpc_xportparms, xmlrpc_wininet_xportparms added
   *potential breaking change* - now by default we fail on invalid
   SSL certs, use the xmlrpc_wininet_xportparms option to enable old
   behavior.

4) Added project file for xmlrpc_sample_auth_client

5) Added project and src for a http.sys based xmlrpc-c server.  See comments
   in the source files.  This supports Windows XP SP2 and Windows Server
   2003 and allows other http.sys based applications to bind to the same
   port.  In Server 2003, IIS uses http.sys and thus the XML-RPC server
   can be run on the standard port 80 along with IIS.  The sample also
   supports https and basic authentication.  It tested OK with
   http://validator.xmlrpc.com/  Note that the Platform SDK headers and
   link libraries for Windows XP SP2 or newer are required to compile
   xmlrpc-c for this module.  If you are not using this server, it is
   safe to exclude the xmlrpc_server_w32httpsys.c file from the xmlrpc
   project and these dependencies will not be required.  You can get the
   latest platform SDK at
   http://www.microsoft.com/msdownload/platformsdk/sdkupdate/
   Be sure after installation to choose the program to "register the PSDK
   directories with Visual Studio" so the newer headers are found.

6) Better support for libcurl.  Updated project files,
   transport_config_win32.h, added documentation UsingCURLinWin32.txt.

Changes from the 1.00 -> 1.01 release for Win32:

1) Project files now reflect static linking for the expat XML library.

2) Example projects were created/updated to keep them in sync with the
   distribution.  The project files were moved into the Windows
   directory

3) Projects for the rpc and cpp tests were created.  The
   xmlrpc_win32_config.h defines the directory for the test files relative
   to the output directory

4) Major refactoring of the Wininet Transport.

