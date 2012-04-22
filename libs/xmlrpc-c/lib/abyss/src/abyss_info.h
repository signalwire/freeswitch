#ifndef ABYSS_INFO_H_INCLUDED
#define ABYSS_INFO_H_INCLUDED

#include "version.h"  /* defines XMLRPC_C_VERSION */

#define SERVER_HTML_INFO \
  "<p><HR><b><i><a href=\"http:\057\057xmlrpc-c.sourceforge.net\">" \
  "ABYSS Web Server for XML-RPC For C/C++</a></i></b> " \
  "version " XMLRPC_C_VERSION "<br>" \
  "</p>"

#define SERVER_PLAIN_INFO \
  CRLF "----------------------------------------" \
       "----------------------------------------" \
  CRLF "ABYSS Web Server for XML-RPC For C/C++ " \
  "version " XMLRPC_C_VERSION CRLF "See xmlrpc-c.sourceforge.net"

#endif
