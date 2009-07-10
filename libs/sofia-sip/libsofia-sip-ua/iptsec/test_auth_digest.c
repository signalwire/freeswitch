/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2005 Nokia Corporation.
 *
 * Contact: Pekka Pessi <pekka.pessi@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

/**@CFILE test_auth_digest.c
 *
 * @brief Test authentication functions for "Digest" scheme.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Thu Feb 22 12:10:37 2001 ppessi
 */

#include "config.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if HAVE_SOFIA_SIP
#define PROTOCOL "SIP/2.0"
#include <sofia-sip/sip.h>
#include <sofia-sip/sip_header.h>
#include <sofia-sip/sip_hclasses.h>
#else
#define PROTOCOL "HTTP/1.1"
#include <sofia-sip/http.h>
#include <sofia-sip/http_header.h>
#define sip_authentication_info_class   http_authentication_info_class
#define sip_authorization               http_authorization
#define sip_authorization_class	        http_authorization_class
#define sip_authorization_make	        http_authorization_make
#define sip_authorization_t	        http_authorization_t
#define sip_default_mclass	        http_default_mclass
#define sip_object		        http_object
#define sip_payload		        http_payload
#define sip_proxy_authenticate_make     http_proxy_authenticate_make
#define sip_proxy_authenticate_t        http_proxy_authenticate_t
#define sip_proxy_authorization_make    http_proxy_authorization_make
#define sip_proxy_authorization_t       http_proxy_authorization_t
#define sip_request		        http_request
#define sip_request_t		        http_request_t
#define sip_t			        http_t
#define sip_www_authenticate	        http_www_authenticate
#define sip_www_authenticate_class      http_www_authenticate_class
#define sip_www_authenticate            http_www_authenticate
#define sip_www_authenticate_make       http_www_authenticate_make
#define sip_www_authenticate_t	        http_www_authenticate_t
#endif

#include <sofia-sip/auth_digest.h>
#include <sofia-sip/auth_client.h>
#include <sofia-sip/msg_header.h>
#include <sofia-sip/su_wait.h>
#include <sofia-sip/su_string.h>

int tstflags;
char *argv0;

#define TSTFLAGS tstflags

#include <sofia-sip/tstdef.h>

#if defined(_WIN32)
#include <fcntl.h>
#endif

char const name[] = "test_auth_digest";

/* Fake su_time() implementation */
#include <time.h>

unsigned offset;

void offset_time(su_time_t *tv)
{
  tv->tv_sec += offset;
}

int test_digest(void)
{
  char challenge[] = "Digest "
    "realm=\"garage.sr.ntc.nokia.com\", "
    "nonce=\"MjAwMS0wMS0yMSAxNTowODo1OA==\", "
    "algorithm=MD5, "
    "qop=\"auth\"";

  char response[] =
    "DIGEST USERNAME=\"digest\", "
    "REALM=\"garage.sr.ntc.nokia.com\", "
    "NONCE=\"MjAwMS0wMS0yMSAxNTowODo1OA==\", "
    "RESPONSE=\"d9d7f1ae99a013cb05f319f0f678251d\", "
    "URI=\"sip:garage.sr.ntc.nokia.com\"";

  char rfc2617[] =
    "Digest username=\"Mufasa\", "
    "realm=\"testrealm@host.com\", "
    "nonce=\"dcd98b7102dd2f0e8b11d0f600bfb0c093\", "
    "cnonce=\"0a4f113b\", "
    "nc=\"00000001\", "
    "qop=\"auth\", "
    "algorithm=\"md5\", "
    "uri=\"/dir/index.html\"";

  char indigo[] =
    "Digest username=\"user1\", "
    "realm=\"nokia-proxy\", "
    "nonce=\"0YXwH29PCT4lEz8+YJipQg==\", "
    "uri=\"sip:nokia@62.254.248.33\", "
    "response=\"dd22a698b1a9510c4237c52e0e2cbfac\", "
    "algorithm=MD5, "
    "cnonce=\"V2VkIEF1ZyAxNSAxNzozNDowNyBHTVQrMDE6MDAgMjAwMVtCQDI0YmZhYQ==\", "
    "opaque=\"WiMlvw==\", "
    "qop=auth, "
    "nc=000000002";

  char proxy_authenticate[] =
    "Digest realm=\"IndigoSw\", "
    "domain=\"indigosw.com aol.com\", "
    "nonce=\"V2VkIEF1ZyAxNSAxNzoxNzozNyBCU1QgMjAwMVtCQDE3OWU4Yg==\", "
    "opaque=\"Nzg5MWU3YjZiNDQ0YzI2Zg==\", "
    "stale=false, "
    "algorithm=md5, "
    "qop=\"auth, auth-int\"";

  sip_www_authenticate_t *wa;
  sip_authorization_t *au;
  sip_proxy_authenticate_t *pa;
  sip_proxy_authorization_t *pz;

  auth_challenge_t ac[1] = {{ sizeof(ac) }};
  auth_response_t  ar[1] = {{ sizeof(ar) }};
  su_home_t home[1] = {{ sizeof(home) }};

  auth_hexmd5_t sessionkey, hresponse;

  BEGIN();

  TEST0(wa = sip_www_authenticate_make(home, challenge));
  TEST_SIZE(auth_digest_challenge_get(home, ac, wa->au_params), 6);
  TEST_S(ac->ac_realm, "garage.sr.ntc.nokia.com");
  TEST_S(ac->ac_nonce, "MjAwMS0wMS0yMSAxNTowODo1OA==");
  TEST_S(ac->ac_algorithm, "MD5");
  TEST_1(ac->ac_md5); TEST_1(!ac->ac_md5sess); TEST_1(!ac->ac_sha1);
  TEST_S(ac->ac_qop, "auth");
  TEST_1(ac->ac_auth); TEST_1(!ac->ac_auth_int);

  TEST0(au = sip_authorization_make(home, response));
  TEST_SIZE(auth_digest_response_get(home, ar, au->au_params), 5);

  TEST0(au = sip_authorization_make(home, rfc2617));
  TEST_SIZE(auth_digest_response_get(home, ar, au->au_params), 10);

  TEST0(auth_digest_sessionkey(ar, sessionkey, "Circle Of Life") == 0);
  if (tstflags & tst_verbatim)
    printf("%s: sessionkey=\"%s\"\n", name, sessionkey);
  TEST0(strcmp(sessionkey, "939e7578ed9e3c518a452acee763bce9") == 0);

  TEST0(auth_digest_response(ar, hresponse, sessionkey, "GET", NULL, 0) == 0);
  if (tstflags & tst_verbatim)
    printf("%s: hresponse=\"%s\"\n", name, hresponse);
  TEST0(strcmp(hresponse, "6629fae49393a05397450978507c4ef1") == 0);

  TEST0(au = sip_authorization_make(home, indigo));
  TEST_SIZE(auth_digest_response_get(home, ar, au->au_params), 12);
  TEST0(auth_digest_sessionkey(ar, sessionkey, "secret") == 0);
  TEST0(auth_digest_response(ar, hresponse, sessionkey, "BYE", NULL, 0) == 0);
  TEST0(strcmp(hresponse, "dd22a698b1a9510c4237c52e0e2cbfac") == 0);

  TEST0(pa = sip_proxy_authenticate_make(home, proxy_authenticate));
  TEST_SIZE(auth_digest_challenge_get(home, ac, pa->au_params), 9);

  TEST_S(ac->ac_realm, "IndigoSw");
  TEST_1(ac->ac_auth);
  TEST_1(ac->ac_auth_int);

  {
    char challenge[] =
      "Digest realm=\"opera.ntc.nokia.com\", "
      "nonce=\"InyiWI+qIdvDKkO2jFK7mg==\"";

    char credentials[] =
      "Digest username=\"samuel.privat.saturday@opera.ntc.nokia.com\", "
      "realm=\"opera.ntc.nokia.com\", nonce=\"InyiWI+qIdvDKkO2jFK7mg==\", "
      "algorithm=MD5, uri=\"sip:opera.ntc.nokia.com\", "
      "response=\"4b4edab897dafce8d9af4b37abcdc086\"";

    memset(ac, 0, sizeof(ac)); ac->ac_size = sizeof(ac);
    memset(ar, 0, sizeof(ar)); ar->ar_size = sizeof(ar);

    TEST0(pa = sip_www_authenticate_make(home, challenge));
    TEST_SIZE(auth_digest_challenge_get(home, ac, pa->au_params), 2);

    TEST0(pz = sip_proxy_authorization_make(home, credentials));
    TEST_SIZE(auth_digest_response_get(home, ar, pz->au_params), 7);

    ar->ar_md5 = ac->ac_md5 || ac->ac_algorithm == NULL;

    TEST0(!auth_digest_sessionkey(ar, sessionkey, "1123456789ABCDEF"));
    TEST0(!auth_digest_response(ar, hresponse, sessionkey, "REGISTER", NULL, 0));
    TEST_S(hresponse, "4b4edab897dafce8d9af4b37abcdc086");
  }

  if (0) {
    /*
      RFC 2069:
      that the username for this document is "Mufasa", and the password is
      "CircleOfLife".

      The first time the client requests the document, no Authorization
      header is sent, so the server responds with:

HTTP/1.1 401 Unauthorized
WWW-Authenticate: Digest    realm="testrealm@host.com",
                            nonce="dcd98b7102dd2f0e8b11d0f600bfb0c093",
                            opaque="5ccc069c403ebaf9f0171e9517f40e41"

  The client may prompt the user for the username and password, after
  which it will respond with a new request, including the following
  Authorization header:

Authorization: Digest       username="Mufasa",
                            realm="testrealm@host.com",
                            nonce="dcd98b7102dd2f0e8b11d0f600bfb0c093",
                            uri="/dir/index.html",
                            response="e966c932a9242554e42c8ee200cec7f6",
                            opaque="5ccc069c403ebaf9f0171e9517f40e41"
    */

    char challenge[] =
      "Digest realm=\"testrealm@host.com\", "
      "nonce=\"dcd98b7102dd2f0e8b11d0f600bfb0c093\", "
      "opaque=\"5ccc069c403ebaf9f0171e9517f40e41\"";

    char rfc2069_cred[] =
      "Digest username=\"Mufasa\", "
      "realm=\"testrealm@host.com\", "
      "nonce=\"dcd98b7102dd2f0e8b11d0f600bfb0c093\", "
      "uri=\"/dir/index.html\", "
      "response=\"e966c932a9242554e42c8ee200cec7f6\", "
      "opaque=\"5ccc069c403ebaf9f0171e9517f40e41\"";

    memset(ac, 0, sizeof(ac)); ac->ac_size = sizeof(ac);
    memset(ar, 0, sizeof(ar)); ar->ar_size = sizeof(ar);

    TEST0(pa = sip_www_authenticate_make(home, challenge));
    TEST_SIZE(auth_digest_challenge_get(home, ac, pa->au_params), 3);

    TEST0(pz = sip_proxy_authorization_make(home, rfc2069_cred));
    TEST_SIZE(auth_digest_response_get(home, ar, pz->au_params), 6);

    ar->ar_md5 = ac->ac_md5 || ac->ac_algorithm == NULL;

    TEST_S(ar->ar_username, "Mufasa");
    TEST0(!auth_digest_sessionkey(ar, sessionkey, "CircleOfLife"));
    TEST0(!auth_digest_response(ar, hresponse, sessionkey, "GET", NULL, 0));
    TEST_S(hresponse, "e966c932a9242554e42c8ee200cec7f6");
  }

  {
    char worldcom_chal[] =
      "Digest realm=\"WCOM\", nonce=\"ce2292f3f748fbe239bda9e852e8b986\"";

    char worldcom_cred[] =
      "Digest realm=\"WCOM\", username=\"jari\", "
      "nonce=\"ce2292f3f748fbe239bda9e852e8b986\", "
      "response=\"ea692d202019d41a75c70df4b2401e2f\", "
      "uri=\"sip:1234@209.132.126.82\"";

    memset(ac, 0, sizeof(ac)); ac->ac_size = sizeof(ac);
    memset(ar, 0, sizeof(ar)); ar->ar_size = sizeof(ar);

    TEST0(pa = sip_proxy_authenticate_make(home, worldcom_chal));
    TEST_SIZE(auth_digest_challenge_get(home, ac, pa->au_params), 2);

    TEST0(pz = sip_proxy_authorization_make(home, worldcom_cred));
    TEST_SIZE(auth_digest_response_get(home, ar, pz->au_params), 5);

    ar->ar_md5 = ac->ac_md5 || ac->ac_algorithm == NULL;

    TEST0(!auth_digest_sessionkey(ar, sessionkey, "pass"));
    TEST0(!auth_digest_response(ar, hresponse, sessionkey, "REGISTER", NULL, 0));
    TEST_S(hresponse, "ea692d202019d41a75c70df4b2401e2f");
  }

  {
    char etri_chal[] =
      "Digest realm=\"nokia-proxy\", domain=\"sip:194.2.188.133\", "
      "nonce=\"wB7JBwIb/XhtgfGp1VuPoQ==\", opaque=\"wkJxwA==\", "
      ", algorithm=MD5, qop=\"auth\"";

    char etri_cred[] =
      "Digest username=\"myhuh\", realm=\"nokia-proxy\", "
      "nonce=\"wB7JBwIb/XhtgfGp1VuPoQ==\", uri=\"sip:194.2.188.133\", "
      "response=\"32960a62bdc202171ca5a294dc229a6d\", "
      "opaque=\"wkJxwA==\"" /* , qop=\"auth\"" */;

    memset(ac, 0, sizeof(ac)); ac->ac_size = sizeof(ac);
    memset(ar, 0, sizeof(ar)); ar->ar_size = sizeof(ar);

    TEST0(pa = sip_proxy_authenticate_make(home, etri_chal));
    TEST_SIZE(auth_digest_challenge_get(home, ac, pa->au_params), 8);

    TEST0(pz = sip_proxy_authorization_make(home, etri_cred));
    TEST_SIZE(auth_digest_response_get(home, ar, pz->au_params), 6 /* 8 */);

    ar->ar_md5 = ac->ac_md5 || ac->ac_algorithm == NULL;

    TEST(auth_digest_sessionkey(ar, sessionkey, "myhuh"), 0);
    TEST(auth_digest_response(ar, hresponse, sessionkey, "REGISTER", NULL, 0), 0);
    TEST_S(hresponse, "32960a62bdc202171ca5a294dc229a6d");
  }

  {
    char chal[] =
      "Digest realm=\"nokia-proxy\", domain=\"sip:10.21.32.63\", "
      "nonce=\"GjbLsrozHC6Lx95C57vGlw==\", opaque=\"HN22wQ==\", algorithm=MD5";

    char cred[] =
      "digest username=\"test1\",realm=\"nokia-proxy\","
      "nonce=\"GjbLsrozHC6Lx95C57vGlw==\",opaque=\"HN22wQ==\","
      "uri=\"sip:10.21.32.63\",response=\"e86db25d96713482e35378504caaba6b\","
      "algorithm=\"MD5\"";

    memset(ac, 0, sizeof(ac)); ac->ac_size = sizeof(ac);
    memset(ar, 0, sizeof(ar)); ar->ar_size = sizeof(ar);

    TEST0(pa = sip_proxy_authenticate_make(home, chal));
    TEST_SIZE(auth_digest_challenge_get(home, ac, pa->au_params), 6);

    TEST0(pz = sip_proxy_authorization_make(home, cred));

    {
    size_t n = auth_digest_response_get(home, ar, pz->au_params);
    TEST_SIZE(n, 8);
    }
    ar->ar_md5 = ac->ac_md5 || ac->ac_algorithm == NULL;

    TEST(auth_digest_sessionkey(ar, sessionkey, "test1"), 0);
    TEST(auth_digest_response(ar, hresponse, sessionkey, "REGISTER", NULL, 0), 0);
    TEST_S(hresponse, "db41913e8964dde69a1519739f35a302");
  }

  {
    char challenge[] =
      "Digest realm=\"nokia-proxy\", domain=\"sip:194.2.188.133\", "
      "nonce=\"3wWGOvaWn3n+hFv8PK2ABQ==\", opaque=\"+GNywA==\", "
      "algorithm=MD5, qop=\"auth-int\"";
    char credentials[] =
      "Digest username=\"test\", realm=\"nokia-proxy\", "
      "nonce=\"3wWGOvaWn3n+hFv8PK2ABQ==\", "
      "cnonce=\"11RkhFg9EdaIRD36w0EMVA==\", opaque=\"+GNywA==\", "
      "uri=\"sip:3000@194.2.188.133\", algorithm=MD5, "
      "response=\"26e8b9aaacfca2d68770fab1ec04e2c7\", "
      "qop=auth-int, nc=00000001";
    char data[] =
      "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n"
      "<presence>\n"
      "<presentity uri=\"sip:3000@194.2.188.133\"/>\n"
      "<atom atomid=\"BQpTalFpkMyF9hOlR8olWQ==\">\n"
      "<address uri=\"sip:3000@194.2.188.133\" priority=\" 0\">\n"
      "<status status=\"open\"/>\n"
      "<class class=\"business\"/>\n"
      "<duplex duplex=\"full\"/>\n"
      "<feature feature=\"voicemail\"/>\n"
      "<mobility mobility=\"fixed\"/>\n"
      "<note>\n"
      "</note>\n"
      "</address>\n"
      "</atom>\n"
      "</presence>\n";

    memset(ac, 0, sizeof(ac)); ac->ac_size = sizeof(ac);
    memset(ar, 0, sizeof(ar)); ar->ar_size = sizeof(ar);

    TEST0(pa = sip_proxy_authenticate_make(home, challenge));
    TEST_SIZE(auth_digest_challenge_get(home, ac, pa->au_params), 8);

    TEST0(pz = sip_proxy_authorization_make(home, credentials));
    TEST_SIZE(auth_digest_response_get(home, ar, pz->au_params), 12);

    ar->ar_md5 = ac->ac_md5 || ac->ac_algorithm == NULL;

    TEST0(!auth_digest_sessionkey(ar, sessionkey, "test"));
    TEST0(!auth_digest_response(ar, hresponse, sessionkey, "REGISTER",
				data, strlen(data)));
    TEST_S(hresponse, "26e8b9aaacfca2d68770fab1ec04e2c7");
  }

  su_home_deinit(home);

  END();
}

#include <sofia-sip/msg_addr.h>

msg_t *read_message(int flags, char const buffer[])
{
  int n, m;
  msg_t *msg;
  msg_iovec_t iovec[2];

  n = strlen(buffer);
  if (n == 0)
    return NULL;

  msg = msg_create(sip_default_mclass(), flags);
  if (msg_recv_iovec(msg, iovec, 2, n, 1) < 0) {
    perror("msg_recv_iovec");
  }
  memcpy(iovec->mv_base, buffer, n);
  msg_recv_commit(msg, n, 1);

  m = msg_extract(msg);
  if (m < 0) {
    fprintf(stderr, "test_auth_digest: parsing error\n");
    return NULL;
  }

  return msg;
}

#define AUTH_MAGIC_T su_root_t

#include <sofia-sip/auth_module.h>

static
void test_callback(su_root_t *root, auth_status_t *as)
{
  su_root_break(root);
}

static
void init_as(auth_status_t *as)
{
  memset(as, 0, sizeof *as);
  as->as_home->suh_size = (sizeof *as);
  su_home_init(as->as_home);
  as->as_method = "REGISTER";
  as->as_status = 500;
  as->as_phrase = "Infernal Error";
}

static
void deinit_as(auth_status_t *as)
{
  su_home_deinit(as->as_home);
  memset(as, 0, sizeof *as);
}

static
void reinit_as(auth_status_t *as)
{
  deinit_as(as); init_as(as);
}

/* Test digest authentication client and server */
int test_digest_client(void)
{
  BEGIN();

  {
    char challenge[] =
      PROTOCOL " 401 Unauthorized\r\n"
      "Call-ID:0e3dc2b2-dcc6-1226-26ac-258b5ce429ab\r\n"
      "CSeq:32439043 REGISTER\r\n"
      "From:surf3.ims3.so.noklab.net <sip:surf3@ims3.so.noklab.net>;tag=I8hFdg0H3OK\r\n"
      "To:<sip:surf3@ims3.so.noklab.net>\r\n"
      "Via:SIP/2.0/UDP 10.21.36.70:23800;branch=z9hG4bKJjKGu9vIHqf;received=10.21.36.70;rport\r\n"
      "WWW-Authenticate:DIGEST algorithm=MD5,nonce=\"h7wIpP+atU+/+Zau5UwLMA==\",realm=\"ims3.so.noklab.net\"\r\n"
      "Content-Length:0\r\n"
      "Security-Server:digest\r\n"
      "r\n";

    char request[] =
      "REGISTER sip:ims3.so.noklab.net " PROTOCOL "\r\n"
      "Via: SIP/2.0/UDP 10.21.36.70:23800;rport;branch=z9hG4bKRE18GFwa3AS\r\n"
      "Max-Forwards: 80\r\n"
      "From: surf3.ims3.so.noklab.net <sip:surf3@ims3.so.noklab.net>;tag=I8hFdg0H3OK\r\n"
      "To: <sip:surf3@ims3.so.noklab.net>\r\n"
      "Call-ID: 0e3dc2b2-dcc6-1226-26ac-258b5ce429ab\r\n"
      "CSeq: 32439044 REGISTER\r\n"
      "Contact: <sip:10.21.36.70:23800>\r\n"
      "Expires: 3600\r\n"
      "Supported: timer, 100rel\r\n"
      "Security-Client: digest\r\n"
      "Security-Verify: digest;d-ver=\"1234\"\r\n"
      "Content-Length: 0\r\n"
      "r\n";

    msg_t *m1, *m2;
    sip_t *sip;
    auth_client_t *aucs = NULL;
    sip_request_t *rq;
    su_home_t *home;
    su_root_t *root;
    char *srcdir, *s, *testpasswd;
    auth_mod_t *am;
    auth_status_t as[1];
    sip_www_authenticate_t *au;
    auth_challenger_t ach[1] =
      {{ 401, "Authorization required",
	 sip_www_authenticate_class,
	 sip_authentication_info_class
	}};
    auth_challenger_t pach[1] =
      {{ 407, "Proxy Authorization required",
	 sip_proxy_authenticate_class,
	 sip_proxy_authentication_info_class
	}};

    TEST_1(home = su_home_new(sizeof(*home)));

    TEST_1(m1 = read_message(MSG_DO_EXTRACT_COPY, challenge));
    TEST_1(sip = sip_object(m1));

    TEST_1(aucs == NULL);
    TEST(auc_challenge(&aucs, home, sip->sip_www_authenticate,
		       sip_authorization_class), 1);
    TEST_1(aucs != NULL);
    msg_destroy(m1);

    TEST(auc_all_credentials(&aucs, "DIGEST", "\"ims3.so.noklab.net\"",
			     "surf3.private@ims3.so.noklab.net", "1234"), 1);

    TEST_1(m2 = read_message(MSG_DO_EXTRACT_COPY, request));
    TEST_1(sip = sip_object(m2));
    TEST_P(sip->sip_authorization, NULL);
    TEST_1(rq = sip->sip_request);
    TEST(auc_authorization(&aucs, m2, (msg_pub_t*)sip, rq->rq_method_name,
			   rq->rq_url, sip->sip_payload), 1);
    TEST_1(sip->sip_authorization);
    TEST_S(msg_params_find(sip->sip_authorization->au_params,
			   "response="),
	   "\"860f5ecc9990772e16937750ced9594d\"");

    TEST(auc_authorization(&aucs, m2, (msg_pub_t*)sip, rq->rq_method_name,
			   (url_t *)"sip:surf3@ims3.so.noklab.net",
			   sip->sip_payload), 1);
    TEST_1(sip->sip_authorization);
    TEST_S(msg_params_find(sip->sip_authorization->au_params,
			   "response="),
	   "\"9ce0d6a5869b4e09832d5b705453cbfc\"");

    srcdir = getenv("srcdir");
    if (srcdir == NULL) {
      srcdir = su_strdup(home, argv0);
      if ((s = strrchr(srcdir, '/')))
	*s = '\0';
      else
	srcdir = ".";
    }
    TEST_1(testpasswd = su_sprintf(home, "%s/testpasswd", srcdir));

    TEST_1(root = su_root_create(NULL));

    TEST_1(am = auth_mod_create(NULL,
				AUTHTAG_METHOD("Digest"),
				AUTHTAG_REALM("ims3.so.noklab.net"),
				AUTHTAG_DB(testpasswd),
				AUTHTAG_OPAQUE("+GNywA=="),
				TAG_END()));

    init_as(as);
    auth_mod_check_client(am, as, sip->sip_authorization, ach);
    TEST(as->as_status, 401);

    TEST_1(au = sip_authorization_make(home,
				       "Digest username=\"user1\", "
				       "nonce=\"3wWGOvaWn3n+hFv8PK2ABQ==\", "
				       "opaque=\"+GNywA==\", "
				       "uri=\"sip:3000@194.2.188.133\", "
				       "response=\"26e8b9aaacfca2d6"
				       "8770fab1ec04e2c7\", "
				       "realm=\"ims3.so.noklab.net\""));

    reinit_as(as);
    auth_mod_check_client(am, as, au, ach);
    TEST(as->as_status, 401);

    {
      char const *username = au->au_params[0];
      char const *nonce = au->au_params[1];
      char const *opaque = au->au_params[2];
      char const *uri = au->au_params[3];
      char const *response = au->au_params[4];
      char const *realm = au->au_params[5];

      TEST_S(username, "username=\"user1\"");
      TEST_S(nonce, "nonce=\"3wWGOvaWn3n+hFv8PK2ABQ==\"");
      TEST_S(opaque, "opaque=\"+GNywA==\"");
      TEST_S(uri, "uri=\"sip:3000@194.2.188.133\"");
      TEST_S(response, "response=\"26e8b9aaacfca2d68770fab1ec04e2c7\"");

      TEST(msg_params_remove((msg_param_t *)au->au_params, "username"), 1);
      reinit_as(as);
      auth_mod_check_client(am, as, au, ach);
      TEST(as->as_status, 400);
      msg_params_add(home, (msg_param_t **)&au->au_params, username);

      TEST(msg_params_remove((msg_param_t *)au->au_params, "nonce"), 1);
      reinit_as(as);
      auth_mod_check_client(am, as, au, ach);
      TEST(as->as_status, 400);
      msg_params_add(home, (msg_param_t **) &au->au_params, nonce);

      TEST(msg_params_remove((msg_param_t *)au->au_params, "opaque"), 1);
      reinit_as(as);
      auth_mod_check_client(am, as, au, ach);
      TEST(as->as_status, 401);	/* We use opaque to match authorization */
      msg_params_add(home, (msg_param_t **) &au->au_params, opaque);

      TEST(msg_params_remove((msg_param_t *)au->au_params, "uri"), 1);
      reinit_as(as);
      auth_mod_check_client(am, as, au, ach);
      TEST(as->as_status, 400);
      msg_params_add(home, (msg_param_t **) &au->au_params, uri);

      TEST(msg_params_remove((msg_param_t *)au->au_params, "response"), 1);
      reinit_as(as);
      auth_mod_check_client(am, as, au, ach);
      TEST(as->as_status, 400);
      msg_params_add(home, (msg_param_t **)&au->au_params, response);

      TEST(msg_params_remove((msg_param_t *)au->au_params, "realm"), 1);
      reinit_as(as);
      auth_mod_check_client(am, as, au, ach);
      TEST(as->as_status, 401);	/* au is ignored by auth_module */
      msg_params_add(home, (msg_param_t **)&au->au_params, realm);

      reinit_as(as);
      auth_mod_check_client(am, as, au, ach);
      TEST(as->as_status, 401);
    }

    as->as_response = (msg_header_t *)
      sip_www_authenticate_make(as->as_home, "Unknown realm=\"huu haa\"");
    TEST_1(as->as_response);
    TEST(auc_challenge(&aucs, home, (msg_auth_t *)as->as_response,
		       sip_authorization_class), 1);
    aucs = NULL;

    reinit_as(as);
    auth_mod_check_client(am, as, NULL, ach);
    TEST(as->as_status, 401);
    TEST(auc_challenge(&aucs, home, (msg_auth_t *)as->as_response,
		       sip_authorization_class), 1);
    reinit_as(as);

    TEST(auc_all_credentials(&aucs, "Digest", "\"ims3.so.noklab.net\"",
			     "user1", "secret"), 1);

    msg_header_remove(m2, (void *)sip, (void *)sip->sip_authorization);

    TEST(auc_authorization(&aucs, m2, (msg_pub_t*)sip, rq->rq_method_name,
			   (url_t *)"sip:surf3@ims3.so.noklab.net",
			   sip->sip_payload), 1);
    TEST_1(sip->sip_authorization);

    TEST_1(msg_params_find(sip->sip_authorization->au_params, "cnonce=") == 0);
    TEST_1(msg_params_find(sip->sip_authorization->au_params, "nc=") == 0);

    auth_mod_check_client(am, as, sip->sip_authorization, ach);
    TEST(as->as_status, 0);
    TEST_1(as->as_info);	/* challenge for next round */
    auth_mod_destroy(am);
    aucs = NULL;

    TEST_1(am = auth_mod_create(NULL,
				AUTHTAG_METHOD("Digest"),
				AUTHTAG_REALM("ims3.so.noklab.net"),
				AUTHTAG_DB(testpasswd),
				AUTHTAG_ALGORITHM("MD5-sess"),
				AUTHTAG_QOP("auth"),
				AUTHTAG_OPAQUE("opaque=="),
				TAG_END()));

    reinit_as(as);
    auth_mod_check_client(am, as, NULL, ach); TEST(as->as_status, 401);

    {
      msg_auth_t *au = (msg_auth_t *)as->as_response;
      int i;
      char *equal;

      if (au->au_params)
	for (i = 0; au->au_params[i]; i++) {
	  if (su_casenmatch(au->au_params[i], "realm=", 6))
	    continue;
	  equal = strchr(au->au_params[i], '=');
	  if (equal)
	    msg_unquote(equal + 1, equal + 1);
	}

      TEST(auc_challenge(&aucs, home, au, sip_authorization_class), 1);
      reinit_as(as);
    }

    TEST(auc_all_credentials(&aucs, "Digest", "\"ims3.so.noklab.net\"",
			     "user1", "secret"), 1);
    msg_header_remove(m2, (void *)sip, (void *)sip->sip_authorization);
    TEST(auc_authorization(&aucs, m2, (msg_pub_t*)sip, rq->rq_method_name,
			   (url_t *)"sip:surf3@ims3.so.noklab.net",
			   sip->sip_payload), 1);
    TEST_1(sip->sip_authorization);

    auth_mod_check_client(am, as, sip->sip_authorization, ach);
    TEST(as->as_status, 0);
    TEST_1(as->as_info == NULL);	/* No challenge for next round */

    /* Test with changed payload */

    reinit_as(as);
    as->as_body = "foo"; as->as_bodylen = 3;
    auth_mod_check_client(am, as, sip->sip_authorization, ach);
    TEST(as->as_status, 0);

    reinit_as(as); aucs = NULL;

    /* Test without opaque */
    {
      msg_auth_t *au;
      char const *opaque;

      auth_mod_check_client(am, as, NULL, ach); TEST(as->as_status, 401);

      au = (void *)msg_header_dup(home, as->as_response); TEST_1(au);

      TEST_1(msg_params_find_slot((msg_param_t *)au->au_params, "opaque"));

      opaque = *msg_params_find_slot((msg_param_t *)au->au_params, "opaque");

      TEST(msg_params_remove((msg_param_t *)au->au_params, "opaque"), 1);

      TEST(auc_challenge(&aucs, home, au, sip_authorization_class), 1);
      TEST(auc_all_credentials(&aucs, "Digest", "\"ims3.so.noklab.net\"",
			       "user1", "secret"), 1);
      msg_header_remove(m2, (void *)sip, (void *)sip->sip_authorization);

      TEST(auc_authorization(&aucs, m2, (msg_pub_t*)sip, rq->rq_method_name,
			     (url_t *)"sip:surf3@ims3.so.noklab.net",
			     sip->sip_payload), 1);

      TEST_1(sip->sip_authorization);
      msg_params_add(home,
		     (msg_param_t **)&sip->sip_authorization->au_params,
		     opaque);

      reinit_as(as);
      auth_mod_check_client(am, as, sip->sip_authorization, ach);
      TEST(as->as_status, 0);
    }

    reinit_as(as); auth_mod_destroy(am); aucs = NULL;

    /* Test without realm */
    {
      msg_auth_t *au;

      TEST_1(am = auth_mod_create(NULL,
				  AUTHTAG_METHOD("Digest"),
				  AUTHTAG_DB(testpasswd),
				  AUTHTAG_ALGORITHM("MD5-sess"),
				  AUTHTAG_QOP("auth"),
				  AUTHTAG_OPAQUE("opaque=="),
				  TAG_END()));
      as->as_realm = NULL;
      auth_mod_check_client(am, as, NULL, ach); TEST(as->as_status, 500);

      as->as_realm = "ims3.so.noklab.net";
      auth_mod_check_client(am, as, NULL, ach); TEST(as->as_status, 401);

      au = (void *)msg_header_dup(home, as->as_response); TEST_1(au);

      TEST(auc_challenge(&aucs, home, au, sip_authorization_class), 1);
      TEST(auc_all_credentials(&aucs, "Digest", "\"ims3.so.noklab.net\"",
			       "user1", "secret"), 1);
      msg_header_remove(m2, (void *)sip, (void *)sip->sip_authorization);

      TEST(auc_authorization(&aucs, m2, (msg_pub_t*)sip, rq->rq_method_name,
			     (url_t *)"sip:surf3@ims3.so.noklab.net",
			     sip->sip_payload), 1);

      TEST_1(sip->sip_authorization);
      reinit_as(as);

      as->as_realm = "ims3.so.noklab.net";
      auth_mod_check_client(am, as, sip->sip_authorization, ach);
      TEST(as->as_status, 0);
    }

    reinit_as(as); auth_mod_destroy(am); aucs = NULL;

    /* Test nextnonce */
    {
      char const *nonce1, *nextnonce, *nonce2;

      TEST_1(am = auth_mod_create(NULL,
				  AUTHTAG_METHOD("Digest"),
				  AUTHTAG_REALM("ims3.so.noklab.net"),
				  AUTHTAG_DB(testpasswd),
				  AUTHTAG_ALGORITHM("MD5"),
				  AUTHTAG_QOP("auth-int"),
				  AUTHTAG_EXPIRES(90),
				  /* Generate nextnonce
				     if NEXT_EXPIRES in nonzero */
				  AUTHTAG_NEXT_EXPIRES(900),
				  TAG_END()));

      reinit_as(as);
      auth_mod_check_client(am, as, NULL, ach); TEST(as->as_status, 401);

      TEST(auc_challenge(&aucs, home, (msg_auth_t *)as->as_response,
			 sip_authorization_class), 1);
      TEST(auc_all_credentials(&aucs, "Digest", "\"ims3.so.noklab.net\"",
			       "user1", "secret"), 1);
      msg_header_remove(m2, (void *)sip, (void *)sip->sip_authorization);
      TEST(auc_authorization(&aucs, m2, (msg_pub_t*)sip, rq->rq_method_name,
			     (url_t *)"sip:surf3@ims3.so.noklab.net",
			     sip->sip_payload), 1);
      TEST_1(sip->sip_authorization);
      TEST_1(nonce1 = msg_header_find_param(sip->sip_authorization->au_common, "nonce"));

      reinit_as(as);
      auth_mod_check_client(am, as, sip->sip_authorization, ach);
      TEST(as->as_status, 0);
      /* We got authentication-info */
      TEST_1(as->as_info);
      /* It contains nextnonce */
      TEST_1(nextnonce = msg_header_find_param(as->as_info->sh_common, "nextnonce"));

      /* Store it in authenticator */
      TEST(auc_info(&aucs, (msg_auth_info_t const *)as->as_info, sip_authorization_class), 1);

      msg_header_remove(m2, (void *)sip, (void *)sip->sip_authorization);
      TEST(auc_authorization(&aucs, m2, (msg_pub_t*)sip, rq->rq_method_name,
			     (url_t *)"sip:surf3@ims3.so.noklab.net",
			     sip->sip_payload), 1);
      TEST_1(sip->sip_authorization);
      TEST_1(nonce2 = msg_header_find_param(sip->sip_authorization->au_common, "nonce"));

      /*
       * Make sure that server-side sends nextnonce in Authentication-info
       * header, nextnonce differs from nonce sent in Challenge
       */
      TEST_1(strcmp(nonce1, nextnonce));
      /* And client-side uses it */
      TEST_S(nonce2, nextnonce);

      auth_mod_destroy(am); aucs = NULL;
    }

    TEST_1(am = auth_mod_create(NULL,
				AUTHTAG_METHOD("Digest"),
				AUTHTAG_REALM("ims3.so.noklab.net"),
				AUTHTAG_DB(testpasswd),
				AUTHTAG_ALGORITHM("MD5-sess"),
				AUTHTAG_QOP("auth-int"),
				TAG_END()));

    reinit_as(as);
    auth_mod_check_client(am, as, NULL, ach); TEST(as->as_status, 401);

    TEST(auc_challenge(&aucs, home, (msg_auth_t *)as->as_response,
		       sip_authorization_class), 1);
    TEST(auc_all_credentials(&aucs, "Digest", "\"ims3.so.noklab.net\"",
			     "user1", "secret"), 1);
    msg_header_remove(m2, (void *)sip, (void *)sip->sip_authorization);
    TEST(auc_authorization(&aucs, m2, (msg_pub_t*)sip, rq->rq_method_name,
			   (url_t *)"sip:surf3@ims3.so.noklab.net",
			   sip->sip_payload), 1);
    TEST_1(sip->sip_authorization);

    reinit_as(as);
    auth_mod_check_client(am, as, sip->sip_authorization, ach);
    TEST(as->as_status, 0);
    auth_mod_destroy(am); aucs = NULL;

    TEST_1(am = auth_mod_create(NULL,
				AUTHTAG_METHOD("Digest"),
				AUTHTAG_REALM("ims3.so.noklab.net"),
				AUTHTAG_DB(testpasswd),
				AUTHTAG_ALGORITHM("MD5-sess"),
				AUTHTAG_QOP("auth,auth-int"),
				AUTHTAG_FORBIDDEN(1),
				AUTHTAG_ANONYMOUS(1),
				AUTHTAG_MAX_NCOUNT(1),
				TAG_END()));

    reinit_as(as);
    auth_mod_check_client(am, as, NULL, ach); TEST(as->as_status, 401);

    TEST(auc_challenge(&aucs, home, (msg_auth_t *)as->as_response,
		       sip_authorization_class), 1);
    TEST(auc_all_credentials(&aucs, "Digest", "\"ims3.so.noklab.net\"",
			     "user1", "secret"), 1);
    msg_header_remove(m2, (void *)sip, (void *)sip->sip_authorization);
    TEST(auc_authorization(&aucs, m2, (msg_pub_t*)sip, rq->rq_method_name,
			   (url_t *)"sip:surf3@ims3.so.noklab.net",
			   sip->sip_payload), 1);
    TEST_1(sip->sip_authorization);

    reinit_as(as);
    auth_mod_check_client(am, as, sip->sip_authorization, ach);
    TEST(as->as_status, 0);

    au = (void*)msg_header_copy(msg_home(m2), (void*)sip->sip_authorization);

    /* Test with invalid qop (bug #2329) */
    msg_params_replace(msg_home(m2), (void *)&au->au_params,
		       "qop=\"auth,auth-int\"");
    reinit_as(as);
    auth_mod_check_client(am, as, au, ach);
    TEST(as->as_status, 400);

    reinit_as(as);
    as->as_body = "foo"; as->as_bodylen = 3;
    auth_mod_check_client(am, as, sip->sip_authorization, ach);
    TEST(as->as_status, 403);

    reinit_as(as);
    as->as_body = ""; as->as_bodylen = 0;
    as->as_method = "OPTIONS";
    auth_mod_check_client(am, as, sip->sip_authorization, ach);
    TEST(as->as_status, 403);

    /* Test staleness check */
    offset = 2 * 3600;
    reinit_as(as);
    auth_mod_check_client(am, as, sip->sip_authorization, ach);
    TEST(as->as_status, 401);
    TEST_1(au = (void *)as->as_response); TEST_1(au->au_params);
    TEST_S(msg_params_find(au->au_params, "stale="), "true");

    TEST(auc_challenge(&aucs, home, (msg_auth_t *)as->as_response,
		       sip_authorization_class), 1);
    msg_header_remove(m2, (void *)sip, (void *)sip->sip_authorization);
    TEST(auc_authorization(&aucs, m2, (msg_pub_t*)sip, rq->rq_method_name,
			   (url_t *)"sip:surf3@ims3.so.noklab.net",
			   sip->sip_payload), 1);
    TEST_1(sip->sip_authorization);
    TEST_S(msg_header_find_param(sip->sip_authorization->au_common, "nc="),
	   "00000001");

    reinit_as(as);
    auth_mod_check_client(am, as, sip->sip_authorization, ach);
    TEST(as->as_status, 0);

    /* Test nonce count check */
    msg_header_remove(m2, (void *)sip, (void *)sip->sip_authorization);
    TEST(auc_authorization(&aucs, m2, (msg_pub_t*)sip, rq->rq_method_name,
			   (url_t *)"sip:surf3@ims3.so.noklab.net",
			   sip->sip_payload), 1);
    TEST_1(sip->sip_authorization);
    TEST_S(msg_header_find_param(sip->sip_authorization->au_common, "nc="),
	   "00000002");

    reinit_as(as);
    auth_mod_check_client(am, as, sip->sip_authorization, ach);
    TEST(as->as_status, 401);
    TEST_1(au = (void *)as->as_response); TEST_1(au->au_params);
    TEST_S(msg_params_find(au->au_params, "stale="), "true");

    aucs = NULL;

    /* Test anonymous operation */
    reinit_as(as);
    auth_mod_check_client(am, as, NULL, ach); TEST(as->as_status, 401);
    TEST(auc_challenge(&aucs, home, (msg_auth_t *)as->as_response,
		       sip_authorization_class), 1);
    reinit_as(as);

    TEST(auc_all_credentials(&aucs, "Digest", "\"ims3.so.noklab.net\"",
			     "anonymous", ""), 1);
    msg_header_remove(m2, (void *)sip, (void *)sip->sip_authorization);
    TEST(auc_authorization(&aucs, m2, (msg_pub_t*)sip, rq->rq_method_name,
			   (url_t *)"sip:surf3@ims3.so.noklab.net",
			   sip->sip_payload), 1);
    TEST_1(sip->sip_authorization);

    auth_mod_check_client(am, as, sip->sip_authorization, ach);
    TEST(as->as_status, 0);
    auth_mod_destroy(am); aucs = NULL;

    /* Test empty realm */
    TEST_1(am = auth_mod_create(root,
				AUTHTAG_METHOD("Digest"),
				AUTHTAG_REALM(""),
				AUTHTAG_DB(testpasswd),
				TAG_END()));
    reinit_as(as);
    auth_mod_check_client(am, as, NULL, ach); TEST(as->as_status, 401);
    TEST(auc_challenge(&aucs, home, (msg_auth_t *)as->as_response,
		       sip_authorization_class), 1);
    reinit_as(as);

    TEST(auc_all_credentials(&aucs, "Digest", "\"\"", "user1", "secret"), 1);
    msg_header_remove(m2, (void *)sip, (void *)sip->sip_authorization);
    TEST(auc_authorization(&aucs, m2, (msg_pub_t*)sip, rq->rq_method_name,
			   (url_t *)"sip:surf3@ims3.so.noklab.net",
			   sip->sip_payload), 1);
    TEST_1(sip->sip_authorization);

    auth_mod_check_client(am, as, sip->sip_authorization, ach);
    TEST(as->as_status, 0);
    aucs = NULL;
    reinit_as(as);
    auth_mod_check_client(am, as, NULL, pach); TEST(as->as_status, 407);
    TEST(auc_challenge(&aucs, home, (msg_auth_t *)as->as_response,
		       sip_proxy_authorization_class), 1);
    reinit_as(as);

    TEST(auc_credentials(&aucs, as->as_home, "Digest:\"\":user1:secret"), 1);
    msg_header_remove(m2, (void *)sip, (void *)sip->sip_proxy_authorization);
    TEST(auc_authorization(&aucs, m2, (msg_pub_t*)sip, rq->rq_method_name,
			   (url_t *)"sip:surf3@ims3.so.noklab.net",
			   sip->sip_payload), 1);
    TEST_1(sip->sip_proxy_authorization);

    auth_mod_check_client(am, as, sip->sip_proxy_authorization, pach);
    TEST(as->as_status, 0);

    auth_mod_destroy(am); aucs = NULL;

    /* Test Basic authentication scheme */
    TEST_1(am = auth_mod_create(root,
				AUTHTAG_METHOD("Basic"),
				AUTHTAG_REALM("ims3.so.noklab.net"),
				AUTHTAG_DB(testpasswd),
				TAG_END()));

    reinit_as(as);
    auth_mod_check_client(am, as, NULL, ach);
    TEST(as->as_status, 401);

    TEST(auc_challenge(&aucs, home, (msg_auth_t *)as->as_response,
		       sip_authorization_class), 1);
    reinit_as(as);

    TEST(auc_all_credentials(&aucs, "Basic", "\"ims3.so.noklab.net\"",
			     "user1", "secret"), 1);
    msg_header_remove(m2, (void *)sip, (void *)sip->sip_authorization);
    TEST(auc_authorization(&aucs, m2, (msg_pub_t*)sip, rq->rq_method_name,
			   (url_t *)"sip:surf3@ims3.so.noklab.net",
			   sip->sip_payload), 1);
    TEST_1(sip->sip_authorization);

    auth_mod_check_client(am, as, sip->sip_authorization, ach);
    TEST(as->as_status, 0);

    aucs = NULL;

    reinit_as(as);
    auth_mod_check_client(am, as, NULL, ach);
    TEST(as->as_status, 401);

    TEST(auc_challenge(&aucs, home, (msg_auth_t *)as->as_response,
		       sip_authorization_class), 1);
    reinit_as(as);

    TEST(auc_all_credentials(&aucs, "Basic", "\"ims3.so.noklab.net\"",
         "very-long-user-name-that-surely-exceeds-the-static-buffer",
         "at-least-when-used-with-the-even-longer-password"), 1);
    msg_header_remove(m2, (void *)sip, (void *)sip->sip_authorization);
    TEST(auc_authorization(&aucs, m2, (msg_pub_t*)sip, rq->rq_method_name,
			   (url_t *)"sip:surf3@ims3.so.noklab.net",
			   sip->sip_payload), 1);
    TEST_1(sip->sip_authorization);

    auth_mod_check_client(am, as, sip->sip_authorization, ach);
    TEST(as->as_status, 0);

    auth_mod_destroy(am); deinit_as(as); aucs = NULL;

    /* Test client with two challenges */
    au = sip_www_authenticate_make(
      NULL,
      "Digest realm=\"test-realm\", "
      "nonce=\"dcd98b7102dd2f0e8b11d0f600bfb0c093\", "
      "opaque=\"5ccc069c403ebaf9f0171e9517f40e41\"");
    au->au_next = sip_www_authenticate_make(
      NULL,
      "Not-Digest realm=\"test-realm\", "
      "zip=\"dcd98b7102dd2f0e8b11d0f600bfb0c093\", "
      "zap=\"5ccc069c403ebaf9f0171e9517f40e41\"");

    TEST_1(auc_challenge(&aucs, home, (msg_auth_t *)au,
			 sip_authorization_class) >= 1);
    TEST_1(auc_all_credentials(&aucs, "Digest", "\"test-realm\"",
			       "user", "pass"));
    msg_header_remove(m2, (void *)sip, (void *)sip->sip_authorization);
    TEST(auc_authorization(&aucs, m2, (msg_pub_t*)sip, rq->rq_method_name,
			   (url_t *)"sip:surf3@ims3.so.noklab.net",
			   sip->sip_payload), 1);
    TEST_1(sip->sip_authorization);
    aucs = NULL;

    /* Test asynchronous operation */
    aucs = NULL;
    TEST_1(am = auth_mod_create(root,
				AUTHTAG_METHOD("Delayed+Digest"),
				AUTHTAG_REALM("ims3.so.noklab.net"),
				AUTHTAG_DB(testpasswd),
				AUTHTAG_ALGORITHM("MD5-sess"),
				AUTHTAG_QOP("auth-int"),
				AUTHTAG_REMOTE((void *)"http://localhost:9"),
				TAG_END()));

    reinit_as(as);
    as->as_callback = test_callback;
    as->as_magic = root;
    auth_mod_check_client(am, as, NULL, ach);
    TEST(as->as_status, 100);
    su_root_run(root);
    TEST(as->as_status, 401);

    TEST(auc_challenge(&aucs, home, (msg_auth_t *)as->as_response,
		       sip_authorization_class), 1);

    reinit_as(as);
    as->as_callback = test_callback;
    as->as_magic = root;

    TEST(auc_all_credentials(&aucs, "Digest", "\"ims3.so.noklab.net\"",
			     "user1", "secret"), 1);
    msg_header_remove(m2, (void *)sip, (void *)sip->sip_authorization);
    TEST(auc_authorization(&aucs, m2, (msg_pub_t*)sip, rq->rq_method_name,
			   (url_t *)"sip:surf3@ims3.so.noklab.net",
			   sip->sip_payload), 1);
    TEST_1(sip->sip_authorization);

    auth_mod_check_client(am, as, sip->sip_authorization, ach);
    TEST(as->as_status, 100);
    su_root_run(root);
    TEST(as->as_status, 0);

    auth_mod_destroy(am); aucs = NULL;

    deinit_as(as);
    msg_destroy(m2);

    su_root_destroy(root);

    su_home_unref(home);
  }

  END();
}

int
test_auth_client(void)
{
  BEGIN();

  {
    char challenge[] =
      PROTOCOL " 401 Unauthorized\r\n"
      "Call-ID:0e3dc2b2-dcc6-1226-26ac-258b5ce429ab\r\n"
      "CSeq:32439043 REGISTER\r\n"
      "From:surf3.ims3.so.noklab.net <sip:surf3@ims3.so.noklab.net>;tag=I8hFdg0H3OK\r\n"
      "To:<sip:surf3@ims3.so.noklab.net>\r\n"
      "Via:SIP/2.0/UDP 10.21.36.70:23800;branch=z9hG4bKJjKGu9vIHqf;received=10.21.36.70;rport\r\n"
      "WWW-Authenticate:DIGEST algorithm=MD5,nonce=\"h7wIpP+atU+/+Zau5UwLMA==\",realm=\"[::1]\"\r\n"
      "Proxy-Authenticate:DIGEST algorithm=MD5,nonce=\"h7wIpP+atU+/+Zau5UwLMA==\",realm=\"\\\"realm\\\"\"\r\n"
      "Content-Length:0\r\n"
      "Security-Server:digest\r\n"
      "r\n";

    su_home_t *home;
    msg_t *msg;
    sip_t *sip;
    auth_client_t *aucs = NULL;

    TEST_1(home = su_home_new(sizeof(*home)));

    TEST_1(msg = read_message(MSG_DO_EXTRACT_COPY, challenge));
    TEST_1(sip = sip_object(msg));

    TEST_1(aucs == NULL);
    TEST(auc_challenge(&aucs, home, sip->sip_www_authenticate,
		       sip_authorization_class), 1);
    TEST_1(aucs != NULL);

    TEST(auc_credentials(&aucs, home, "Digest:\"[::1]\":user:pass"), 1);

    TEST(auc_challenge(&aucs, home, sip->sip_proxy_authenticate,
		       sip_proxy_authorization_class), 1);

    TEST(auc_credentials(&aucs, home, "Digest:\"\\\"realm\\\"\":user:pass"), 1);

    msg_destroy(msg);
    su_home_unref(home);
  }

  END();
}

#if HAVE_FLOCK
#include <sys/file.h>
#endif

#include <sofia-sip/auth_plugin.h>

char tmppasswd[] = "/tmp/test_auth_digest.XXXXXX";

#include <unistd.h>

static void rmtmp(void)
{
  if (tmppasswd[0])
    unlink(tmppasswd);
}

char const passwd[] =
  "# Comment\n"
  "malformed line\n"
  "user1:secret:\n"
  /* user2 has password "secret", too */
  "user2:realm:4cbc2aff0b5b2b33675c0731c0db1c14\n"
  /* duplicate user. fun */
  "user1:secret:realm\n"
  /* empty line */
  "\n";

/* Test digest authentication client and server */
int test_module_io(void)
{
  auth_mod_t *am, am0[1];
  auth_passwd_t *apw, *apw2;
  int tmpfd;

  BEGIN();

#ifndef _WIN32
  tmpfd = mkstemp(tmppasswd); TEST_1(tmpfd != -1);
#else
  tmpfd = open(tmppasswd, O_WRONLY); TEST_1(tmpfd != -1);
#endif
  atexit(rmtmp);		/* Make sure temp file is unlinked */

  TEST_SIZE(write(tmpfd, passwd, strlen(passwd)), strlen(passwd));
  TEST(close(tmpfd), 0);

  /* Test file reading operation */
  am = auth_mod_create(NULL,
		       AUTHTAG_METHOD("Digest"),
		       AUTHTAG_REALM("realm"),
		       AUTHTAG_DB(tmppasswd),
		       AUTHTAG_ALGORITHM("MD5-sess"),
		       AUTHTAG_QOP("auth-int"),
		       TAG_END()); TEST_1(am);

  apw = auth_mod_getpass(am, "user1", NULL); TEST_1(apw);
  TEST_S(apw->apw_realm, "realm");

  apw = auth_mod_getpass(am, "user2", NULL); TEST_1(apw);
  TEST_S(apw->apw_hash, "4cbc2aff0b5b2b33675c0731c0db1c14");

  apw2 = apw;

  *am0 = *am;

  TEST_1(auth_readdb_if_needed(am) == 0);

  apw = auth_mod_getpass(am, "user2", NULL); TEST_1(apw);
  TEST_P(apw, apw2);

  apw = auth_mod_addpass(am, "user3", "realm"); TEST_1(apw);
  /* user3 with password fisu */
  apw->apw_hash = "056595147630692bb29d1855089bc95b";

  {
    char const user3[] = "user3:realm:7df96b4718bd933af4883c8b73c96318\n";
    tmpfd = open(tmppasswd, O_WRONLY|O_APPEND, 0); TEST_1(tmpfd != -1);
    /* user3 with password fish */
    TEST_SIZE(write(tmpfd, user3, strlen(user3)), strlen(user3));
    TEST_1(close(tmpfd) == 0);
  }

#if HAVE_FLOCK
  /* Test flock(). */
  tmpfd = open(tmppasswd, O_RDONLY);

  TEST_1(flock(tmpfd, LOCK_EX) != -1);

  TEST_1(auth_readdb_if_needed(am) == 0);

  /* there should be no changes in user table */
  apw = auth_mod_getpass(am, "user2", NULL); TEST_1(apw);
  TEST_P(apw, apw2);

  TEST_1(flock(tmpfd, LOCK_UN) != -1);
#endif

  TEST_1(auth_readdb_if_needed(am) == 0);

  apw = auth_mod_getpass(am, "user2", "realm"); TEST_1(apw);
  TEST_1(apw != apw2);

  /* Local user3 overrides non-local */
  apw = auth_mod_getpass(am, "user3", "realm"); TEST_1(apw);
  TEST_S(apw->apw_hash, "7df96b4718bd933af4883c8b73c96318");

  /* Test truncating */
  {
    char const user1[] = "user1:secret:\n";
    tmpfd = open(tmppasswd, O_WRONLY|O_TRUNC, 0); TEST_1(tmpfd != -1);
    TEST_SIZE(write(tmpfd, user1, strlen(user1)), strlen(user1));
    TEST_1(close(tmpfd) == 0);
  }

  TEST_1(auth_readdb_if_needed(am) == 0);

  apw = auth_mod_getpass(am, "user2", "realm"); TEST_1(apw == NULL);

  /* Non-local user3 is kept in database */
  apw = auth_mod_getpass(am, "user3", "realm"); TEST_1(apw);
  TEST_S(apw->apw_hash, "056595147630692bb29d1855089bc95b");

  auth_mod_destroy(am);

  if (unlink(tmppasswd) == 0)
    tmppasswd[0] = '\0';

  END();
}

#include <sofia-sip/su_log.h>

extern su_log_t iptsec_log[];

static
void usage(int exitcode)
{
  fprintf(stderr, "usage: %s [-v] [-a] [-l n]\n", name);
  exit(exitcode);
}

int main(int argc, char *argv[])
{
  int retval = 0;
  int i;
  extern void (*_su_time)(su_time_t *tv);

  _su_time = offset_time;

  argv0 = argv[0];

  su_init();

  for (i = 1; argv[i]; i++) {
    if (strcmp(argv[i], "-v") == 0)
      tstflags |= tst_verbatim;
    else if (strcmp(argv[i], "-a") == 0)
      tstflags |= tst_abort;
    else if (strncmp(argv[i], "-l", 2) == 0) {
      int level = 3;
      char *rest = NULL;

      if (argv[i][2])
	level = strtol(argv[i] + 2, &rest, 10);
      else if (argv[i + 1])
	level = strtol(argv[i + 1], &rest, 10), i++;
      else
	level = 3, rest = "";

      if (rest == NULL || *rest)
	usage(1);

      su_log_set_level(iptsec_log, level);
    } else {
      usage(1);
    }
  }

  if ((TSTFLAGS & tst_verbatim))
    su_log_soft_set_level(iptsec_log, 9);
  else
    su_log_soft_set_level(iptsec_log, 0);

  retval |= test_digest();
  retval |= test_digest_client();
  retval |= test_auth_client();
  retval |= test_module_io();

  su_deinit();

  return retval;
}
