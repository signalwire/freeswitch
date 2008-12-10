/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2008, Eric des Courtis <eric.des.courtis@benbria.com>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * Eric des Courtis <eric.des.courtis@benbria.com>
 * Copyright (C) Benbria. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Eric des Courtis <eric.des.courtis@benbria.com>
 *
 *
 * http_req.h -- HTTP client implementation
 *
 */

#ifndef __HTTP_REQ_H__
#define __HTTP_REQ_H__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>

#define ERROR -1
#define SUCCESS 0
#define EMPTY_STRING(s) s[0] = '\0'

#define DEFAULT_HTTP_VERSION "1.1"
#define DEFAULT_HTTP_PORT 80

#define HTTP_GET_METHOD "GET"
#define HTTP_HEAD_METHOD "HEAD"
#define HTTP_POST_METHOD "POST"
#define HTTP_DELETE_METHOD "DELETE"
#define HTTP_PUT_METHOD "PUT"

#define MAX_PORT_LEN 6

#define RECV_BUFF_SIZE 65536

#define HTTP_TRUE 1
#define HTTP_FALSE 0

#define INIT_STATE_MACHINE(m) \
do {\
    (m)->pos = 0;\
    (m)->buf = NULL;\
    (m)->token = SYNTAX_ERROR;\
    (m)->stop = HTTP_FALSE;\
    (m)->state = state_start;\
} while(0)

#define STATUS_CODE_LEN 4

typedef enum tokens {
    VERSION,
    STATUS_CODE,
    PHRASE,
    HEADER,
    NEWLINE,
    SYNTAX_ERROR
} token_t;

typedef enum accept_states {
    ASWR,
    ASNR,
    NOAS
} accept_state_t;

typedef struct state_machines {
    accept_state_t (*state)(char, struct state_machines *);
    int pos;
    char *buf;
    ssize_t buf_len;
    int stop;
    token_t token;
} state_machine_t;

typedef enum http_methods {
    GET,
    HEAD,
    POST,
    DELETE,
    PUT
} http_method_t;

typedef struct http_headers {
    char *field_name;
    char *value;
} http_header_t;

typedef struct http_requests {
    http_method_t method;
    char *version;
    char *url;
    http_header_t *headers;
    ssize_t header_len;
    char *body;
    ssize_t body_len;
} http_request_t;

typedef struct http_responses {
    char *version;
    int status_code;
    char *phrase;
    http_header_t *headers;
    ssize_t header_len;
    char *body;
    int body_len;
} http_response_t;

token_t get_next_token(state_machine_t *sm);
int http_parse_response(char *buf, ssize_t buf_len, http_response_t *response);
int http_req(http_request_t *req, http_response_t *res);
void free_http_response(http_response_t *response);

#endif
