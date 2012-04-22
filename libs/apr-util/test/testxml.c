/* Copyright 2000-2005 The Apache Software Foundation or its licensors, as
 * applicable.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "apr.h"
#include "apr_general.h"
#include "apr_xml.h"

#if APR_HAVE_STDLIB_H
#include <stdlib.h>  /* for exit() */
#endif

static const char *progname;
static const char *usage = "%s [xmlfile]\nIt will create "
                           "a dummy XML file if none is supplied";
/*
 * If our platform knows about the tmpnam() external buffer size, create
 * a buffer to pass in.  This is needed in a threaded environment, or
 * one that thinks it is (like HP-UX).
 */

#ifdef L_tmpnam
static char tname_buf[L_tmpnam];
#else
static char *tname_buf = NULL;
#endif

static apr_status_t create_dummy_file_error(apr_pool_t *p, apr_file_t **fd)
{
    apr_status_t rv;
    char *tmpfile;
    int i;
    apr_off_t off = 0L;
    tmpfile = tmpnam(tname_buf);

    if ((tmpfile == NULL) || (*tmpfile == '\0')) {
        fprintf(stderr, "unable to generate temporary filename\n");
        if (errno == 0) {
            errno = ENOENT;
        }
        perror("tmpnam");
        return APR_ENOENT;
    }
    rv = apr_file_open(fd, tmpfile, APR_CREATE|APR_TRUNCATE|APR_DELONCLOSE|
                       APR_READ|APR_WRITE|APR_EXCL, APR_OS_DEFAULT, p);

    if (rv != APR_SUCCESS)
        return rv;
    rv = apr_file_puts("<?xml version=\"1.0\" ?>\n<maryx>"
                       "<had a=\"little\"/><lamb its='fleece "
                       "was white as snow' />\n", *fd);
    if (rv != APR_SUCCESS)
        return rv;

    for (i = 0; i < 5000; i++) {
        rv = apr_file_puts("<hmm roast=\"lamb\" "
                           "for=\"dinner\">yummy</hmm>\n", *fd);
        if (rv != APR_SUCCESS)
            return rv;
    }
    rv = apr_file_puts("</mary>\n", *fd);
    if (rv != APR_SUCCESS)
        return rv;

    return apr_file_seek(*fd, APR_SET, &off);
}

static apr_status_t create_dummy_file(apr_pool_t *p, apr_file_t **fd)
{
    apr_status_t rv;
    char *tmpfile;
    int i;
    apr_off_t off = 0L;
    tmpfile = tmpnam(tname_buf);

    if ((tmpfile == NULL) || (*tmpfile == '\0')) {
        fprintf(stderr, "unable to generate temporary filename\n");
        if (errno == 0) {
            errno = ENOENT;
        }
        perror("tmpnam");
        return APR_ENOENT;
    }
    rv = apr_file_open(fd, tmpfile, APR_CREATE|APR_TRUNCATE|APR_DELONCLOSE|
                       APR_READ|APR_WRITE|APR_EXCL, APR_OS_DEFAULT, p);

    if (rv != APR_SUCCESS)
        return rv;
    rv = apr_file_puts("<?xml version=\"1.0\" ?>\n<mary>"
                       "<had a=\"little\"/><lamb its='fleece "
                       "was white as snow' />\n", *fd);
    if (rv != APR_SUCCESS)
        return rv;

    for (i = 0; i < 5000; i++) {
        rv = apr_file_puts("<hmm roast=\"lamb\" "
                           "for=\"dinner\">yummy</hmm>\n", *fd);
        if (rv != APR_SUCCESS)
            return rv;
    }
    rv = apr_file_puts("</mary>\n", *fd);
    if (rv != APR_SUCCESS)
        return rv;

    rv = apr_file_seek(*fd, APR_SET, &off);
    return rv;
}

static void dump_xml(apr_xml_elem *e, int level)
{
    apr_xml_attr *a;
    apr_xml_elem *ec;

    printf("%d: element %s\n", level, e->name);
    if (e->attr) {
        a = e->attr;
        printf("%d:\tattrs\t", level);
        while (a) {
            printf("%s=%s\t", a->name, a->value);
            a = a->next;
        }
        printf("\n");
    }
    if (e->first_child) {
        ec = e->first_child;
        while (ec) {
            dump_xml(ec, level + 1);
            ec = ec->next;
        }
    }
}

static void oops(const char *s1, const char *s2, apr_status_t rv)
{
    if (progname)
        fprintf(stderr, "%s: ", progname);
    fprintf(stderr, s1, s2);
    if (rv != APR_SUCCESS) {
        char buf[120];

        fprintf(stderr, " (%s)", apr_strerror(rv, buf, sizeof buf));
    }
    fprintf(stderr, "\n");
    exit(1);
}

int main(int argc, const char *const * argv)
{
    apr_pool_t *pool;
    apr_file_t *fd;
    apr_xml_parser *parser;
    apr_xml_doc *doc;
    apr_status_t rv;
    char errbuf[2000];
    char errbufXML[2000];

    (void) apr_initialize();
    apr_pool_create(&pool, NULL);
    progname = argv[0];
    if (argc == 1) {
        rv = create_dummy_file(pool, &fd);
        if (rv != APR_SUCCESS) {
            oops("cannot create dummy file", "oops", rv);
        }
    }
    else {
        if (argc == 2) {
            rv = apr_file_open(&fd, argv[1], APR_READ, APR_OS_DEFAULT, pool);
            if (rv != APR_SUCCESS) {
                oops("cannot open: %s", argv[1], rv);
            }
        }
        else {
            oops("usage: %s", usage, 0);
        }
    }
    rv = apr_xml_parse_file(pool, &parser, &doc, fd, 2000);
    if (rv != APR_SUCCESS) {
        fprintf(stderr, "APR Error %s\nXML Error: %s\n",
                apr_strerror(rv, errbuf, sizeof(errbuf)),
             apr_xml_parser_geterror(parser, errbufXML, sizeof(errbufXML)));
        return rv;
    }
    dump_xml(doc->root, 0);
    apr_file_close(fd);
    if (argc == 1) {
        rv = create_dummy_file_error(pool, &fd);
        if (rv != APR_SUCCESS) {
            oops("cannot create error dummy file", "oops", rv);
        }
        rv = apr_xml_parse_file(pool, &parser, &doc, fd, 2000);
        if (rv != APR_SUCCESS) {
            fprintf(stdout, "APR Error %s\nXML Error: %s "
                            "(EXPECTED) This is good.\n",
                    apr_strerror(rv, errbuf, sizeof(errbuf)),
             apr_xml_parser_geterror(parser, errbufXML, sizeof(errbufXML)));
             rv = APR_SUCCESS; /* reset the return code, as the test is supposed to get this error */
        }
        else {
            fprintf(stderr, "Expected an error, but didn't get one ;( ");
            return APR_EGENERAL;
        }
    }
    apr_pool_destroy(pool);
    apr_terminate();
    return rv;
}
