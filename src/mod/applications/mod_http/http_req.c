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
 * http_req.c -- HTTP client implementation
 *
 */

#include "http_req.h"

/*
extern int dprintf(int, const char *, ...);
extern int isblank(int);
*/

/* NOTE: v = version, header = h, status = s, newline = n, phrase = p */
static accept_state_t state_start(char c, state_machine_t *sm);
static accept_state_t state_shp  (char c, state_machine_t *sm);
static accept_state_t state_n    (char c, state_machine_t *sm);
static accept_state_t state_vhp_A(char c, state_machine_t *sm);
static accept_state_t state_vhp_B(char c, state_machine_t *sm);
static accept_state_t state_vhp_C(char c, state_machine_t *sm);
static accept_state_t state_vhp_D(char c, state_machine_t *sm);
static accept_state_t state_vhp_E(char c, state_machine_t *sm);
static accept_state_t state_vhp_F(char c, state_machine_t *sm);
static accept_state_t state_vhp_G(char c, state_machine_t *sm);
static accept_state_t state_vhp_H(char c, state_machine_t *sm);
static accept_state_t state_hp_A (char c, state_machine_t *sm);
static accept_state_t state_hp_B (char c, state_machine_t *sm);
static accept_state_t state_p    (char c, state_machine_t *sm);
static accept_state_t state_error(char c, state_machine_t *sm);

static int read_all(int s, char *buf, size_t len);

#ifdef DEBUG
int main(int argc, char *argv[])
{
    http_response_t response;
    http_request_t request;
    int ret;
    int i;

    if(argc != 2) return EXIT_FAILURE;

    request.method = GET;
    request.version = DEFAULT_HTTP_VERSION;
    request.url = argv[1];
    request.header_len = 0;
    request.body_len = 0;

    ret = http_req(&request, &response);
    if(ret == ERROR) return EXIT_FAILURE;
    
    printf("Version     : %s\n", response.version);
    printf("Status Code : %d\n", response.status_code);
    printf("Phrase      : %s\n", response.phrase);
    
    for(i = 0; i < response.header_len; i++){
        printf(
            "Header      : key = [%s] value = [%s]\n", 
            response.headers[i].field_name, response.headers[i].value
        );
    }

    fflush(stdout);
    write(STDOUT_FILENO, response.body, response.body_len);
    printf("\n");

    free_http_response(&response);

    return EXIT_SUCCESS;
}
#endif

int http_req(http_request_t *req, http_response_t *res)
{
    uint32_t addr;
    int s;
    int ret;
    uint16_t port = 0;
    size_t len;
    struct sockaddr_in sck;
    struct hostent *hst;
    char *hostname;
    char *method;
    char *buf;
    int buf_len;
    ssize_t i;
    int j;
    int l;
    int m;
    int p;
    int q = 0;
    char f = HTTP_FALSE;
    char port_s[MAX_PORT_LEN];
    sighandler_t sig;
    struct timeval tv;

    tv.tv_sec  = 1;
    tv.tv_usec = 0;


    (void)memset(&sck, 0, sizeof(sck));

    sig = signal(SIGPIPE, SIG_IGN);
    if(sig == SIG_ERR){
        fprintf(stderr, "Cannot ignore SIGPIPE signal\n");
        return ERROR;
    }
 
    s = socket(PF_INET, SOCK_STREAM, 0);
    if(s == ERROR){
        perror("Could not create socket");
        return ERROR;
    }

    (void)setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    (void)setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    len = strlen(req->url);
    hostname = (char *)malloc(len * sizeof(char) + 1);
    if(hostname == NULL){
        perror("Could not allocate memory for hostname");
        close(s);
        return ERROR;
    }
    
    EMPTY_STRING(hostname);

    l = strlen(req->url) + 1;
    m = strlen("http://");
    for(p = 0, j = m; j < l; j++){
        if(req->url[j] == ':'){
            strncpy(hostname, req->url + m, j - m);
            hostname[j - m] = '\0';
            f = HTTP_TRUE;
            p = j;
            continue;
        }

        if((req->url[j] == '/' || req->url[j] == '\0')  && f == HTTP_TRUE){
            if((j - p) < MAX_PORT_LEN){
                strncpy(port_s, req->url + p + 1, j - p);
                port_s[j - p] = '\0';
                port = (uint16_t)atoi(port_s);
            }else port = 0;
            q = j;
            break;
        }

        if((req->url[j] == '/' || req->url[j] == '\0') && f == HTTP_FALSE){
            strncpy(hostname, req->url + m, j - m);
            hostname[j - m] = '\0';
            port = DEFAULT_HTTP_PORT;
            q = j;
            break;
        }
    }

    if(port == 0 || hostname[0] == '\0'){
        fprintf(stderr, "Invalid URL\n");
        close(s);
        free(hostname);
        return ERROR;
    }

    l = strlen(hostname);
    for(j = 0; j < l; j++){
        if(hostname[j] == '/'){
            hostname[j] = '\0';
            break;
        }
    }

    hst = gethostbyname(hostname);
    if(hst == NULL){
        herror("Could not find host");
        close(s);
        free(hostname);
        return ERROR;
    }

    addr = *((uint32_t *)hst->h_addr_list[0]);
    
    sck.sin_family = AF_INET;
    sck.sin_port = htons(port);
    sck.sin_addr.s_addr = addr;

    ret = connect(s, (struct sockaddr *)&sck, sizeof(sck));
    if(ret == ERROR){
        perror("Could not connect to host");
        close(s);
        free(hostname);
        return ERROR;
    }
    
    switch(req->method){
    case GET:
        method = HTTP_GET_METHOD;
        break;
    case HEAD:
        method = HTTP_HEAD_METHOD;
        break;
    case POST:
        method = HTTP_POST_METHOD;
        break;
    case DELETE:
	method = HTTP_DELETE_METHOD;
	break;
    case PUT:
	method = HTTP_PUT_METHOD;
	break;
    default:
        method = HTTP_GET_METHOD;
    }
   
    if(req->url[q] == '/'){ 
        dprintf(s, "%s %s HTTP/%s\r\n", method, req->url + q, req->version);
    }else{
        dprintf(s, "%s /%s HTTP/%s\r\n", method, req->url + q, req->version);
    }
    
    if(port != DEFAULT_HTTP_PORT)
        dprintf(s, "Host: %s:%d\r\n", hostname, port);   
    else dprintf(s, "Host: %s\r\n", hostname);   
    dprintf(s, "Connection: close\r\n");
    dprintf(s, "Content-Length: %d\r\n", req->body_len); 

    for(i = 0; i < req->header_len; i++){
        dprintf(
            s, 
            "%s: %s\r\n", 
            req->headers[i].field_name,
            req->headers[i].value
        );
    }

    dprintf(s, "\r\n");

    
    if(req->body != NULL && req->body_len != 0){
        ret = write(s, req->body, req->body_len);
        if(ret == ERROR){
            perror("Could not write body to socket");
            free(hostname);
            return ERROR;
        }
    }

    buf = (char *)malloc(RECV_BUFF_SIZE * sizeof(char));
    if(buf == NULL){
        perror("Could not allocate memory for buffer");
        close(s);
        free(hostname);
        return ERROR;
    }

    buf_len = read_all(s, buf, RECV_BUFF_SIZE -1);
    if(buf_len == ERROR){
        perror("Could not read into buffer");
        free(hostname);
        return ERROR;
    }
    buf[buf_len] = '\0';
   
    close(s);
   
    (void)signal(SIGPIPE, sig);
    free(hostname);
    return http_parse_response(buf, buf_len, res);
}

int http_parse_response(char *buf, ssize_t buf_len, http_response_t *response)
{
    token_t token;
    state_machine_t sm;
    int pos;
    ssize_t size;
    char buff[STATUS_CODE_LEN];
    int old_pos;
    int nt;
    int i;
    int j;
    int f;

    INIT_STATE_MACHINE(&sm);
    
    sm.buf = buf;
    sm.buf_len = buf_len;

    fprintf(
	stderr, 
	"ERRORS\n"
	"VERSION      = %d\n"
	"STATUS_CODE  = %d\n"
	"PHRASE       = %d\n"
	"HEADER       = %d\n"
	"NEWLINE      = %d\n"
	"SYNTAX_ERROR = %d\n",
	VERSION,
	STATUS_CODE,
	PHRASE,
	HEADER,
	NEWLINE,
	SYNTAX_ERROR
    );

    fprintf(
	stderr,
	"buf = \"%s\"\n",
	buf
    );

    pos = sm.pos;
    token = get_next_token(&sm);
    if(token != VERSION){
        fprintf(stderr, "ERROR %d-%d\n", VERSION, token);
        return ERROR;
    }
    
    size = sm.pos - pos;
    response->version = (char *)malloc((size + 1) * sizeof(char));
    if(response->version == NULL){
        perror("Cannot allocate memory for version number");
        return ERROR;
    }
    strncpy(response->version, sm.buf + pos, size);
    response->version[size] = '\0';

    pos = sm.pos;
    token = get_next_token(&sm);
    if(token != STATUS_CODE){
        fprintf(stderr, "ERROR %d-%d\n", STATUS_CODE, token);
        return ERROR;
    }

    size = sm.pos - pos;

    buff[STATUS_CODE_LEN - 1] = '\0';
    strncpy(buff, sm.buf + pos, STATUS_CODE_LEN - 1);

    response->status_code = atoi(buff);

    pos = sm.pos;
    token = get_next_token(&sm);
    if(token != PHRASE){
        fprintf(stderr, "ERROR %d-%d\n", PHRASE, token);
        return ERROR;
    }

    size = sm.pos - pos - 2;
    response->phrase = (char *)malloc((size + 1) * sizeof(char));
    if(response->phrase == NULL){
        perror("Cannot allocate memory for phrase");
        return ERROR;
    }
    strncpy(response->phrase, sm.buf + pos, size);
    response->phrase[size] = '\0';

    old_pos = sm.pos;
    nt = 0;
    f = HTTP_FALSE;
    do{
        token = get_next_token(&sm);
        switch(token){
        case PHRASE:
        case STATUS_CODE:
            f = HTTP_FALSE;
        case VERSION:
        case NEWLINE:
            break;
        case HEADER:
            if(f == HTTP_FALSE){
                nt++;
                f = HTTP_TRUE;
            }
            else f = HTTP_FALSE;
            break;
        case SYNTAX_ERROR:
            return ERROR; 
        }
    
        if(token != HEADER && token != PHRASE && token != STATUS_CODE) break;
    }while(token != SYNTAX_ERROR);

    fprintf(
	stderr,
	"HEADERS = %d\n",
	nt
    ); 

    if(nt != 0){ 
        response->headers = (http_header_t *)malloc(sizeof(http_header_t)*nt);
        if(response->headers == NULL){
            perror("Could not allocate memory for headers");
            return ERROR;
        }
    }else response->headers = NULL;

    response->header_len = nt;

    sm.pos = old_pos;
    for(i = 0; i < nt; i++){
        pos = sm.pos;
        sm.state = state_start;
        sm.stop = HTTP_FALSE;
        token = get_next_token(&sm);
        size = sm.pos - pos;
        size -= 2;

        response->headers[i].field_name = 
            (char *)malloc((size + 1) * sizeof(char));

        if(response->headers[i].field_name == NULL){
            perror("Could not allocate memory for header");
            return ERROR;
        }

        strncpy(response->headers[i].field_name, sm.buf + pos, size);
        response->headers[i].field_name[size] = '\0';

        pos = sm.pos;
        token = get_next_token(&sm);
        if(token == HEADER || token == STATUS_CODE){
            for(j = 0; 
                ((sm.buf + pos)[j] == '\r' 
                    && (sm.buf + pos)[j + 1] == '\n') == 0; 
                j++
            );
            size = j;
            sm.pos = j + 2;
        }else size = sm.pos - pos - 2;

        response->headers[i].value = 
            (char *)malloc((size + 1) * sizeof(char));

        if(response->headers[i].value == NULL){
            perror("Could not allocate memory for header");
            return ERROR;
        }
        strncpy(response->headers[i].value, sm.buf + pos, size);
        response->headers[i].value[size] = '\0';
        
    }

    pos = sm.pos;
    token = get_next_token(&sm);
    if(token != NEWLINE){
        fprintf(stderr, "ERROR %d-%d\n", NEWLINE, token);
        return ERROR;
    }

    response->body = (char *)malloc((buf_len - sm.pos + 1) * sizeof(char));
    if(response->body == NULL){
        perror("Could not allocate memory for body");
        return ERROR;
    }

    response->body_len = buf_len - sm.pos;
    response->body[response->body_len] = '\0';

    (void)memcpy(response->body, buf + sm.pos, response->body_len);

    return SUCCESS;
}

void free_http_response(http_response_t *response)
{
    free(response->version);
    free(response->phrase);
    if(response->headers != NULL) free(response->headers);
    if(response->body != NULL)    free(response->body);
}

token_t get_next_token(state_machine_t *sm)
{
    char c;
    accept_state_t accept;

    while(sm->stop != HTTP_TRUE){
        c = sm->buf[sm->pos];
        accept = sm->state(c, sm);
        switch(accept){
        case NOAS:
        case ASNR:
            sm->pos++;
        case ASWR:
            break;
        }

        switch(accept){
        case ASNR:
        case ASWR:
            sm->state = state_start;
            return sm->token;
        case NOAS:
            break;
        }
        
        if(sm->pos >= sm->buf_len){
            sm->stop = HTTP_TRUE;
            break;
        }
    }
    
    return SYNTAX_ERROR;
}

static accept_state_t state_start(char c, state_machine_t *sm)
{
    if(toupper(c) == 'H')               sm->state = state_vhp_A;
    else if(isdigit(c))                 sm->state = state_shp;
    else if(c == '\r' || c == '\n')     sm->state = state_n;
    else if(isprint(c))                 sm->state = state_hp_A;
    else                                sm->state = state_error;

    return NOAS;
}

static accept_state_t state_shp(char c, state_machine_t *sm)
{
    if(isdigit(c))                      sm->state = state_shp;
    else if(isblank(c)){
        sm->token = STATUS_CODE;
        return ASNR;
    }else if(c == ':')                  sm->state = state_hp_B;
    else if(c == '\r' || c == '\n')     sm->state = state_p;
    else if(isprint(c))                 sm->state = state_hp_A;
    else                                sm->state = state_error;
    
    return NOAS;
}

static accept_state_t state_n(char c, state_machine_t *sm)
{
    if(c == '\r' || c == '\n'){
        sm->token = NEWLINE;
        return ASNR;
    }else if(c == ':')                  sm->state = state_hp_B;
    else if(isprint(c))                 sm->state = state_hp_A;
    else                                sm->state = state_error;

    return NOAS;
}

static accept_state_t state_vhp_A(char c, state_machine_t *sm)
{
    if(toupper(c) == 'T')               sm->state = state_vhp_B;
    else if(isprint(c))                 sm->state = state_hp_A;
    else if(c == '\n' || c == '\r')     sm->state = state_p;
    else                                sm->state = state_error;

    return NOAS;
}

static accept_state_t state_vhp_B(char c, state_machine_t *sm)
{
    if(toupper(c) == 'T')               sm->state = state_vhp_C;
    else if(isprint(c))                 sm->state = state_hp_A;
    else if(c == '\n' || c == '\r')     sm->state = state_p;
    else                                sm->state = state_error;

    return NOAS;
}

static accept_state_t state_vhp_C(char c, state_machine_t *sm)
{
    if(toupper(c) == 'P')               sm->state = state_vhp_D;
    else if(isprint(c))                 sm->state = state_hp_A;
    else if(c == '\n' || c == '\r')     sm->state = state_p;
    else                                sm->state = state_error;
    
    return NOAS;
}

static accept_state_t state_vhp_D(char c, state_machine_t *sm)
{
    if(toupper(c) == '/')               sm->state = state_vhp_E;
    else if(isprint(c))                 sm->state = state_hp_A;
    else if(c == '\n' || c == '\r')     sm->state = state_p;
    else                                sm->state = state_error;
    
    return NOAS;
}

static accept_state_t state_vhp_E(char c, state_machine_t *sm)
{
    if(isdigit(c))                      sm->state = state_vhp_F;
    else if(isprint(c))                 sm->state = state_hp_A;
    else if(c == '\n' || c == '\r')     sm->state = state_p;
    else                                sm->state = state_error;
    
    return NOAS;
}

static accept_state_t state_vhp_F(char c, state_machine_t *sm)
{
    if(isdigit(c))                      sm->state = state_vhp_F;
    else if(c == '.')                   sm->state = state_vhp_G;
    else if(isprint(c))                 sm->state = state_hp_A;
    else if(c == '\n' || c == '\r')     sm->state = state_p;
    else                                sm->state = state_error;
    
    return NOAS;
}

static accept_state_t state_vhp_G(char c, state_machine_t *sm)
{
    if(isdigit(c))                      sm->state = state_vhp_H;
    else if(isprint(c))                 sm->state = state_hp_A;
    else if(c == '\n' || c == '\r')     sm->state = state_p;
    else                                sm->state = state_error;
    
    return NOAS;
}

static accept_state_t state_vhp_H(char c, state_machine_t *sm)
{
    if(isdigit(c))                      sm->state = state_vhp_H;
    else if(isblank(c)){
        sm->token = VERSION;
        return ASNR;

    }else if(isprint(c))                sm->state = state_hp_A;

    return NOAS;
}

static accept_state_t state_hp_A(char c, state_machine_t *sm)
{
    if(c == ':')                        sm->state = state_hp_B;
    else if(c == '\r' || c == '\n')     sm->state = state_p;
    else if(isprint(c))                 sm->state = state_hp_A;
    else                                sm->state = state_error;

    return NOAS;
}

static accept_state_t state_hp_B(char c, state_machine_t *sm)
{
    if(isblank(c)){
        sm->token = HEADER;
        return ASNR;
    } else if(c == '\r' || c == '\n')   sm->state = state_p;
    else if(c == ':')                   sm->state = state_hp_B;
    else if(isprint(c))                 sm->state = state_hp_A;
    else                                sm->state = state_error;

    return NOAS;
}

static accept_state_t state_p(char c, state_machine_t *sm)
{
    if(c == '\r' || c == '\n'){
        sm->token = PHRASE;
        return ASNR;
    }else if(c == ':')          sm->state = state_hp_B; 
    else if(isprint(c))         sm->state = state_hp_A;
    else                        sm->state = state_error;

    return NOAS;
}

static accept_state_t state_error(char c, state_machine_t *sm)
{
    c = 0;
    sm->token = SYNTAX_ERROR;

    return ASNR;
}

static int read_all(int s, char *buf, size_t len)
{
    size_t t;
    size_t r = ERROR;
 
    for(t = 0; t < len && r != 0;){
        r = read(s, buf + t, len - t);
        if((int)r == ERROR){
            return ERROR;
        }else{
            t += r;
        }
    }

    return (int)t;
}

