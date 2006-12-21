#define MUST_BUILD_WININET_CLIENT 1
#define MUST_BUILD_CURL_CLIENT 0
#define MUST_BUILD_LIBWWW_CLIENT 0
static const char * const XMLRPC_DEFAULT_TRANSPORT = "wininet";

/* 
Set to zero if you do not wish to build the http.sys
based XMLRPC-C Server
*/
#define MUST_BUILD_HTTP_SYS_SERVER 1

/*
We use pragma statements to tell the linker what we need to link with.
Since Curl requires Winsock, Winmm, and libcurl, and no other
project does, if we are building curl support we tell the linker
what libs we need to add.
*/
#if MUST_BUILD_CURL_CLIENT > 0
#ifdef _DEBUG
#pragma comment( lib, "../lib/libcurld.lib" )
#else
#pragma comment( lib, "../lib/libcurl.lib" )
#endif
#pragma comment( lib, "Winmm.lib" )
#pragma comment( lib, "Ws2_32.lib" )
#endif