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
 * mod_http.c -- HTTP client implementation for FreeSWITCH
 * 
 * The purpose is to provide laguages like LUA with a _fast_ HTTP
 * client implementation.
 *
 * Support for SSL will be provided in future versions.
 * Initial release does not include win32 support.
 *
 */


#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/queue.h>
#include "json.h"

#include "http_req.h"
#include "url_encoding.h"

#define HTTP_SYNTAX "<http_method> <url> <urlejson_headers> [urlejson_body]"
#define HTTP_PARAMS 4
#define HTTP_BUFFER_SIZE (256 * 1024)

/* SWITCH_STANDARD_API(http_api_main); */

#define MAX_MEMLOCS 512
#define GARBAGE_TYPES_INIT() \
typedef struct memloc{\
    void *p;\
    LIST_ENTRY(memloc) memlocs;\
} memloc_t

#define GARBAGE_CLEANUP() \
do{\
    for(memloc_p = head.lh_first; memloc_p != NULL; \
        memloc_p = memloc_p->memlocs.le_next){\
        free(memloc_p->p);\
    }\
}while(0)

#define GARBAGE_ADD(a) \
do {\
    if(memloc_i >= MAX_MEMLOCS){\
	switch_safe_free(ccmd);\
	GARBAGE_CLEANUP();\
	stream->write_function(stream, "-ERR\n");\
	return SWITCH_STATUS_SUCCESS;\
    }\
    memloc_a[memloc_i].p = (void *)a;\
    LIST_INSERT_HEAD(&head, memloc_a + memloc_i, memlocs);\
    memloc_i++;\
}while(0)

#define GARBAGE_INIT() \
    LIST_HEAD(listhead, memloc) head;\
    memloc_t memloc_a[MAX_MEMLOCS];\
    memloc_t *memloc_p;\
    size_t memloc_i;\
    LIST_INIT(&head);\
    memloc_i = 0

GARBAGE_TYPES_INIT();

SWITCH_STANDARD_API(http_api_main)
{
    char *ccmd;
    int argc;
    char *argv[HTTP_PARAMS];
    char *buf;
    char *method;
    char *url;
    char *headers_str;
    char *value;
    char *body;
    char *body_dec;
    char *t;
    char *json_response;
    struct json_object *json_http_headers;
    char *key;
    struct json_object *val;
    struct lh_entry *entry;
    int i;
    int j;
    int f;
    size_t l;
    size_t m;
    size_t a = 0;
    int ret;

    http_header_t *headers;
    http_request_t request;
    http_response_t response;

    GARBAGE_INIT();
    
    (void)memset(&response, 0, sizeof(response));

    if(cmd == NULL){
        stream->write_function(stream, "-USAGE: %s\n", HTTP_SYNTAX);
        return SWITCH_STATUS_SUCCESS;
    }

    ccmd = strdup(cmd);
    argc = switch_separate_string(ccmd, ' ', argv, HTTP_PARAMS);

    if(argc != HTTP_PARAMS && argc != (HTTP_PARAMS - 1)){
        switch_safe_free(ccmd);
        stream->write_function(stream, "-ERR\n");
        return SWITCH_STATUS_SUCCESS;
    }
    
    method = argv[0];
    url = argv[1];
    headers_str = argv[2];
    if(argc == HTTP_PARAMS){
        body = argv[3];
    }else{
        body = (char *)malloc(1 * sizeof(char));
	if(body == NULL){
            switch_safe_free(ccmd);
            stream->write_function(stream, "-ERR\n");
            return SWITCH_STATUS_SUCCESS;
	}
	body[0] = '\0';
        GARBAGE_ADD(body);
    }

    buf = (char *)malloc(HTTP_BUFFER_SIZE * sizeof(char));
    if(buf == NULL){
        switch_safe_free(ccmd);
        stream->write_function(stream, "-ERR\n");
        GARBAGE_CLEANUP();
        return SWITCH_STATUS_SUCCESS;
    }
    
    GARBAGE_ADD(buf);   
 
    request.version = DEFAULT_HTTP_VERSION;
    l = strlen(url);
    request.url = (char *)malloc((l + 1) * sizeof(char));
    if(request.url == NULL){
        switch_safe_free(ccmd);
        stream->write_function(stream, "-ERR\n");
        GARBAGE_CLEANUP();
        return SWITCH_STATUS_SUCCESS;
    } 
       
    GARBAGE_ADD(request.url);
    strcpy(request.url, url); 
    json_http_headers = json_tokener_parse(headers_str);
    if(is_error(json_http_headers)){
	switch_safe_free(ccmd);
	stream->write_function(stream, "-ERR\n");
	GARBAGE_CLEANUP();
	return SWITCH_STATUS_SUCCESS;
    }

    i = 0;
    json_object_object_foreach(json_http_headers, key, val){ 
        i++; 
    }
    
    request.header_len = i;
    headers = (http_header_t *)malloc(i * sizeof(http_header_t));
    GARBAGE_ADD(headers);

    i = 0;
    json_object_object_foreach(json_http_headers, key, val){
        l = strlen(key);
        request.headers[i].field_name = (char *)malloc((l + 1) * sizeof(char));
        if(request.headers[i].field_name == NULL){
            switch_safe_free(ccmd);
            stream->write_function(stream, "-ERR\n");
            GARBAGE_CLEANUP();
            return SWITCH_STATUS_SUCCESS;
        }
        GARBAGE_ADD(request.headers[i].field_name);
        strcpy(request.headers[i].field_name, key);
        a += strlen(key);

        value = json_object_to_json_string(val);
        l = strlen(value);
        request.headers[i].value = (char *)malloc((l + 1) * sizeof(char));
        if(request.headers[i].value == NULL){
            switch_safe_free(ccmd);
            stream->write_function(stream, "-ERR\n");
            GARBAGE_CLEANUP();
            return SWITCH_STATUS_SUCCESS;
        }
        GARBAGE_ADD(request.headers[i].value);
        strcpy(request.headers[i].value, value);
        a += strlen(value);
        i++;
    }

    if(argc == HTTP_PARAMS){
        l = strlen(body);
        body_dec = url_decode(body, l);
        GARBAGE_ADD(body_dec);
        l = strlen(body_dec);
        request.body_len = l;
        request.body = body_dec;
    }else request.body_len = 0;

    ret = http_req(&request, &response);
    if(response.version != NULL) GARBAGE_ADD(response.version);
    if(response.phrase  != NULL) GARBAGE_ADD(response.phrase);
    if(response.headers != NULL) GARBAGE_ADD(response.headers);
    if(response.body    != NULL) GARBAGE_ADD(response.body);
    for(i = 0; i < response.header_len; i++){
        GARBAGE_ADD(response.headers[i].field_name);
        GARBAGE_ADD(response.headers[i].value);
    }
    
    
    if(ret == ERROR){
        switch_safe_free(ccmd);
        stream->write_function(stream, "-ERR\n");
        GARBAGE_CLEANUP();
        return SWITCH_STATUS_SUCCESS;
    }

    /* This is evil and should be changed in the future */
    l = 128 + (256 * response.header_len) + (a * 2)
        + strlen("version") + strlen(response.version)
        + strlen("status_code") + 3
        + strlen("phrase") + strlen(response.phrase) 
        + strlen("body") + (response.body_len * 3) + 1
	+ strlen("headers")
        + 1;

    /* to be safe */
    l <<= 2;
    
    json_response = (char *)malloc(l * sizeof(char));
    if(json_response == NULL){
        switch_safe_free(ccmd);
        stream->write_function(stream, "-ERR\n");
        GARBAGE_CLEANUP();
        return SWITCH_STATUS_SUCCESS;
    }
    GARBAGE_ADD(json_response);
    
    if(response.body_len != 0){
        t = (char *)malloc((response.body_len + 1) * sizeof(char));
        if(t == NULL){
            switch_safe_free(ccmd);
            stream->write_function(stream, "-ERR\n");
            GARBAGE_CLEANUP();
            return SWITCH_STATUS_SUCCESS;
        }
        GARBAGE_ADD(t);
        (void)memcpy(t, response.body, response.body_len);
        t[response.body_len] = '\0';
        response.body = url_encode(t, response.body_len);
        GARBAGE_ADD(response.body);
    }
    

    m = snprintf(json_response, l,
        "{"
        "\"version\": \"%s\","
        "\"status_code\": \"%3d\","
        "\"phrase\": \"%s\","
        "\"body\": \"%s\","
        "\"headers\": [",
        response.version,
        response.status_code,
        response.phrase,
        ((response.body_len <= 0)? "":response.body)
    );


    for(f = HTTP_FALSE, j = 0; j < response.header_len; j++){
        if(f != HTTP_FALSE){
            m += snprintf(json_response + m, l - m,
                ","
            );
        }else f = HTTP_TRUE;

        m += snprintf(json_response + m, l - m, 
            "{\"key\": \"%s\",\"value\": \"%s\"}",
            response.headers[j].field_name,
            response.headers[j].value
        );
    }


    m += snprintf(json_response + m, l - m, "]}");
    json_response[m] = '\0';


    switch_log_printf(
        SWITCH_CHANNEL_LOG,
        SWITCH_LOG_NOTICE,
        "RESERVED %d BYTES, USED %d BYTES, HTTP Response as JSON: %s\n",
        l, 
        m,
        json_response
    );


    stream->write_function(stream, "%s\n", json_response);

    switch_safe_free(ccmd);
    GARBAGE_CLEANUP();
    return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_http_load);
SWITCH_MODULE_DEFINITION(mod_http, mod_http_load, NULL, NULL);

SWITCH_MODULE_LOAD_FUNCTION(mod_http_load)
{
    switch_api_interface_t *api_interface;

    *module_interface =
        switch_loadable_module_create_module_interface(pool, modname);

    switch_log_printf(
        SWITCH_CHANNEL_LOG,
        SWITCH_LOG_NOTICE,
        "HTTP request mod enabled\n"
    );

    SWITCH_ADD_API(
        api_interface, 
        "http", 
        "Make HTTP requests",
        http_api_main,
        HTTP_SYNTAX
    );

    return SWITCH_STATUS_SUCCESS;
}

