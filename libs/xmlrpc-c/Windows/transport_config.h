/* Win32 version of transport_config.h.

   For other platforms, this is generated automatically, but for Windows,
   someone generates it manually.  Nonetheless, we keep it looking as much
   as possible like the automatically generated one to make it easier to
   maintain (e.g. you can compare the two and see why something builds
   differently for Windows that for some other platform).
*/
#define MUST_BUILD_WININET_CLIENT 1
#define MUST_BUILD_CURL_CLIENT 0
#define MUST_BUILD_LIBWWW_CLIENT 0
static const char * const XMLRPC_DEFAULT_TRANSPORT =
"wininet";
