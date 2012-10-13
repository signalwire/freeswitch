#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "xmlrpc_config.h"

#include "testtool.h"
#include "cgi.h"

static const char cgiResponse1[] =
  "....Status: 200 OK\n"
  "Content-type: text/xml; charset=\"utf-8\"\n"
  "Content-length: 141\n"
  "\n"
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"
  "<methodResponse>\r\n"
  "<params>\r\n"
  "<param><value><i4>12</i4></value></param>\r\n"
  "</params>\r\n"
  "</methodResponse>\r\n";


#define TESTDATA_DIR "data"
#define DIRSEP DIRECTORY_SEPARATOR

void
test_server_cgi(void) {
/*----------------------------------------------------------------------------
  Here, we pretend to be a web server when someone has requested a POST
  to the CGI script "cgitest1".
-----------------------------------------------------------------------------*/
    FILE * cgiOutputP;

    printf("Running CGI tests...\n");

    cgiOutputP = popen("REQUEST_METHOD=POST "
                       "CONTENT_TYPE=text/xml "
                       "CONTENT_LENGTH=211 "
                       "./cgitest1 "
                       "<"
                       TESTDATA_DIR DIRSEP "sample_add_call.xml",
                       "r");

    if (cgiOutputP == NULL)
        TEST_ERROR("Unable to run 'cgitest' program.");
    else {
        unsigned char cgiResponse[4096];
        size_t bytesRead;

        bytesRead = fread(cgiResponse, 1, sizeof(cgiResponse), cgiOutputP);

        TEST(bytesRead == strlen(cgiResponse1));

        TEST(memcmp(cgiResponse, cgiResponse1, bytesRead) == 0);
    }
    fclose(cgiOutputP);
    printf("\n");
    printf("CGI tests done.\n");
}
