/* We use pragma statements to tell the linker what we need to link
   with.  Since Curl requires Winsock, Winmm, and libcurl, and no other
   project does, we include this file into the Curl transport source code
   to tell the linker to add these libs.
   
   Alternatively, the USER can add the libraries to LINK with as
   NEEDED!
*/

#ifdef _DEBUG
#pragma comment( lib, "C:\\FG\\FGCOMXML\\curl\\build\\Debug\\Lib_curl.lib" )
#else
#pragma comment( lib, "C:\\FG\\FGCOMXML\\curl\\build\\Release\\Lib_curl.lib" )
#endif

#pragma comment( lib, "Winmm.lib" )
#pragma comment( lib, "Ws2_32.lib" )
#pragma comment( lib, "Wldap32.lib" )
