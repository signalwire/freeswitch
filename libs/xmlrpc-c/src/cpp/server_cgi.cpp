/*=============================================================================
                               server_cgi
===============================================================================

   This is the definition of the xmlrpc_c::server_cgi class.  An object of
   this class is the guts of a CGI-based XML-RPC server.  It runs inside
   a CGI script and gets the XML-RPC call from and delivers the XML-RPC
   response to the CGI environment.

   By Bryan Henderson 08.09.17.

   Contributed to the public domain by its author.
=============================================================================*/

#include "xmlrpc_config.h"
#if MSVCRT
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <io.h>
#include <fcntl.h>
#endif
#include <cstdlib>  // for getenv
#include <memory>
#include <stdio.h>

#include "xmlrpc-c/girerr.hpp"
using girerr::throwf;
#include "xmlrpc-c/server_cgi.hpp"

using namespace std;



namespace {

class httpInfo {

public:
    string requestMethod;
    bool contentTypePresent;
    string contentType;
    unsigned int contentLength;
    bool contentLengthPresent;
    bool authCookiePresent;
    string authCookie;

    httpInfo() {

        const char * const requestMethodC = getenv("REQUEST_METHOD");
        const char * const contentTypeC   = getenv("CONTENT_TYPE");
        const char * const contentLengthC = getenv("CONTENT_LENGTH");
        const char * const authCookieC    = getenv("HTTP_COOKIE_AUTH");

        if (requestMethodC)
            this->requestMethod = string(requestMethodC);
        else
            throwf("Invalid CGI environment; environment variable "
                   "REQUEST_METHOD is not set");

        if (contentTypeC) {
            this->contentTypePresent = true;
            this->contentType = string(contentTypeC);
        } else
            this->contentTypePresent = false;

        if (contentLengthC) {
            this->contentLengthPresent = true;

            int const lengthAtoi(atoi(string(contentLengthC).c_str()));

            if (lengthAtoi < 0)
                throwf("Content-length HTTP header value is negative");
            else if (lengthAtoi == 0)
                throwf("Content-length HTTP header value is zero");
            else
                this->contentLength = lengthAtoi;
        } else
            this->contentLengthPresent = false;

        if (authCookieC) {
            this->authCookie = string(authCookieC);
            this->authCookiePresent = true;
        } else
            this->authCookiePresent = false;
    }
};



class httpError {

public:

    int const code;
    string const msg;
    
    httpError(int    const code,
              string const& msg) :
        code(code), msg(msg) {}
};


} // namespace



namespace xmlrpc_c {

struct serverCgi_impl {
    // 'registryP' is what we actually use; 'registryHolder' just holds a
    // reference to 'registryP' so the registry doesn't disappear while
    // this server exists.  But note that if the creator doesn't supply
    // a registryPtr, 'registryHolder' is just a placeholder variable and
    // the creator is responsible for making sure the registry doesn't
    // go anywhere while the server exists.

    registryPtr registryHolder;
    const registry * registryP;

    serverCgi_impl(serverCgi::constrOpt const& opt);

    void
    establishRegistry(serverCgi::constrOpt const& opt);

    void
    tryToProcessCall();
};




void
serverCgi_impl::establishRegistry(serverCgi::constrOpt const& opt) {

    if (!opt.present.registryP && !opt.present.registryPtr)
        throwf("You must specify the 'registryP' or 'registryPtr' option");
    else if (opt.present.registryP && opt.present.registryPtr)
        throwf("You may not specify both the 'registryP' and "
               "the 'registryPtr' options");
    else {
        if (opt.present.registryP)
            this->registryP      = opt.value.registryP;
        else {
            this->registryHolder = opt.value.registryPtr;
            this->registryP      = opt.value.registryPtr.get();
        }
    }
}



serverCgi_impl::serverCgi_impl(serverCgi::constrOpt const& opt) {
    this->establishRegistry(opt);
}



serverCgi::constrOpt::constrOpt() {

    present.registryP   = false;
    present.registryPtr = false;
}



#define DEFINE_OPTION_SETTER(OPTION_NAME, TYPE) \
serverCgi::constrOpt & \
serverCgi::constrOpt::OPTION_NAME(TYPE const& arg) { \
    this->value.OPTION_NAME = arg; \
    this->present.OPTION_NAME = true; \
    return *this; \
}

DEFINE_OPTION_SETTER(registryP,   const registry *);
DEFINE_OPTION_SETTER(registryPtr, xmlrpc_c::registryPtr);

#undef DEFINE_OPTION_SETTER



serverCgi::serverCgi(constrOpt const& opt) {

    this->implP = new serverCgi_impl(opt);
}



serverCgi::~serverCgi() {

    delete(this->implP);
}



#if MSVCRT
#define FILEVAR fileP
#else
#define FILEVAR
#endif

static void
setModeBinary(FILE * const FILEVAR) {

#if MSVCRT 
    /* Fix from Jeff Stewart: NT opens stdin and stdout in text mode
       by default, badly confusing our length calculations.  So we need
       to set the file handle to binary. 
    */
    _setmode(_fileno(FILEVAR), _O_BINARY); 
#endif 
}



static string
getHttpBody(FILE * const fileP,
            size_t const length) {

    setModeBinary(fileP);
    char * const buffer(new char[length]);
    auto_ptr<char> p(buffer);  // To make it go away when we leave

    size_t count;

    count = fread(buffer, sizeof(buffer[0]), length, fileP);
    if (count < length)
        throwf("Expected %lu bytes, received %lu",
               (unsigned long) length, (unsigned long) count);

    return string(buffer, length);
}



static void 
writeNormalHttpResp(FILE * const  fileP,
                    bool   const  sendCookie,
                    string const& authCookie,
                    string const& httpBody) {

    setModeBinary(fileP);

    // HTTP headers

    fprintf(fileP, "Status: 200 OK\n");

    if (sendCookie)
        fprintf(fileP, "Set-Cookie: auth=%s\n", authCookie.c_str());

    fprintf(fileP, "Content-type: text/xml; charset=\"utf-8\"\n");
    fprintf(fileP, "Content-length: %u\n", (unsigned)httpBody.size());
    fprintf(fileP, "\n");

    // HTTP body

    fwrite(httpBody.c_str(), sizeof(char), httpBody.size(), fileP);
}



void
processCall2(const registry * const  registryP,
             FILE *           const  callFileP,
             unsigned int     const  callSize,
             bool             const  sendCookie,
             string           const& authCookie,
             FILE *           const  respFileP) {

    if (callSize > xmlrpc_limit_get(XMLRPC_XML_SIZE_LIMIT_ID))
        throw(xmlrpc_c::fault(string("XML-RPC call is too large"),
                              fault::CODE_LIMIT_EXCEEDED));
    else {
        string const callXml(getHttpBody(callFileP, callSize));

        string responseXml;

        try {
            registryP->processCall(callXml, &responseXml);
        } catch (exception const& e) {
            throw(httpError(500, e.what()));
        }
        
        writeNormalHttpResp(respFileP, sendCookie, authCookie, responseXml);
    }
}




static void
sendHttpErrorResp(FILE *    const  fileP,
                  httpError const& e) {

    setModeBinary(fileP);

    // HTTP headers

    fprintf(fileP, "Status: %d %s\n", e.code, e.msg.c_str());
    fprintf(fileP, "Content-type: text/html\n");
    fprintf(fileP, "\n");
    
    // HTTP body: HTML error message

    fprintf(fileP, "<title>%d %s</title>\n", e.code, e.msg.c_str());
    fprintf(fileP, "<h1>%d %s</h1>\n", e.code, e.msg.c_str());
    fprintf(fileP, "<p>The Xmlrpc-c CGI server was unable to process "
            "your request.  It could not process it even enough to generate "
            "an XML-RPC fault response.</p>\n");
}



void
serverCgi_impl::tryToProcessCall() {

    httpInfo httpInfo;

    if (httpInfo.requestMethod != string("POST"))
        throw(httpError(405, "Method must be POST"));

    if (!httpInfo.contentTypePresent)
        throw(httpError(400, "Must have content-type header"));

    if (httpInfo.contentType != string("text/xml"))
        throw(httpError(400, string("ContentType must be 'text/xml', not '") +
                        httpInfo.contentType + string("'")));
    
    if (!httpInfo.contentLengthPresent)
        throw(httpError(411, "Content-length required"));
              
    processCall2(this->registryP, stdin, httpInfo.contentLength,
                 httpInfo.authCookiePresent, httpInfo.authCookie, stdout);
}



void
serverCgi::processCall() {
/*----------------------------------------------------------------------------
  Get the XML-RPC call from Standard Input and environment variables,
  parse it, find the right method, call it, prepare an XML-RPC
  response with the result, and write it to Standard Output.
-----------------------------------------------------------------------------*/
    try {
        this->implP->tryToProcessCall();
    } catch (httpError const e) {
        sendHttpErrorResp(stdout, e);
    }
}



} // namespace
