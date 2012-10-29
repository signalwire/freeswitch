#ifndef CURLTRANSACTION_H_INCLUDED
#define CURLTRANSACTION_H_INCLUDED

#include "bool.h"
#include "xmlrpc-c/util.h"
#include "xmlrpc-c/client.h"
#include <curl/curl.h>

typedef struct curlTransaction curlTransaction;

typedef void curlt_finishFn(xmlrpc_env * const, void * const);
typedef void curlt_progressFn(
    void * const, double const, double const, double const, double const,
    bool * const);

struct curlSetup {

    /* This is all client transport properties that are implemented as
       simple Curl session properties (i.e. the transport basically just
       passes them through to Curl without looking at them).

       People occasionally want to replace all this with something where
       the Xmlrpc-c user simply does the curl_easy_setopt() call and this
       code need not know about all these options.  Unfortunately, that's
       a significant modularity violation.  Either the Xmlrpc-c user
       controls the Curl object or he doesn't.  If he does, then he
       shouldn't use libxmlrpc_client -- he should just copy some of this
       code into his own program.  If he doesn't, then he should never see
       the Curl library.

       Speaking of modularity: the only reason this is a separate struct
       is to make the code easier to manage.  Ideally, the fact that these
       particular properties of the transport are implemented by simple
       Curl session setup would be known only at the lowest level code
       that does that setup.
    */

    const char * networkInterface;
        /* This identifies the network interface on the local side to
           use for the session.  It is an ASCIIZ string in the form
           that the Curl recognizes for setting its CURLOPT_INTERFACE
           option (also the --interface option of the Curl program).
           E.g. "9.1.72.189" or "giraffe-data.com" or "eth0".  

           It isn't necessarily valid, but it does have a terminating NUL.

           NULL means we have no preference.
        */
    bool sslVerifyPeer;
        /* In an SSL connection, we should authenticate the server's SSL
           certificate -- refuse to talk to him if it isn't authentic.
           This is equivalent to Curl's CURLOPT_SSL_VERIFY_PEER option.
        */
    bool sslVerifyHost;
        /* In an SSL connection, we should verify that the server's
           certificate (independently of whether the certificate is
           authentic) indicates the host name that is in the URL we
           are using for the server.
        */

    const char * sslCert;
    const char * sslCertType;
    const char * sslCertPasswd;
    const char * sslKey;
    const char * sslKeyType;
    const char * sslKeyPasswd;
    const char * sslEngine;
    bool         sslEngineDefault;
    unsigned int sslVersion;
    const char * caInfo;
    const char * caPath;
    const char * randomFile;
    const char * egdSocket;
    const char * sslCipherList;

    const char * proxy;
    unsigned int proxyPort;
    unsigned int proxyAuth;
        /* e.g. CURLAUTH_BASIC, CURLAUTH_NTLM, ... */
    const char * proxyUserPwd;
    unsigned int proxyType;
        /* see enum curl_proxytype: CURLPROXY_HTTP, CURLPROXY_SOCKS4, ... */

    unsigned int timeout;
        /* 0 = no Curl timeout.  This is in milliseconds. */

    bool verbose;
};


void
curlTransaction_create(xmlrpc_env *               const envP,
                       CURL *                     const curlSessionP,
                       const xmlrpc_server_info * const serverP,
                       xmlrpc_mem_block *         const callXmlP,
                       xmlrpc_mem_block *         const responseXmlP,
                       bool                       const dontAdvertise,
                       const char *               const userAgent,
                       const struct curlSetup *   const curlSetupStuffP,
                       void *                     const userContextP,
                       curlt_finishFn *           const finish,
                       curlt_progressFn *         const progress,
                       curlTransaction **         const curlTransactionPP);

void
curlTransaction_destroy(curlTransaction * const curlTransactionP);

void
curlTransaction_finish(xmlrpc_env *      const envP,
                       curlTransaction * const curlTransactionP,
                       CURLcode          const result);

void
curlTransaction_getError(curlTransaction * const curlTransactionP,
                         xmlrpc_env *      const envP);

CURL *
curlTransaction_curlSession(curlTransaction * const curlTransactionP);

#endif
