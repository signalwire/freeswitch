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
 * url_encoding.c -- url encoding/decoding
 *
 */
#include "url_encoding.h"

#ifdef DEBUG
int main(int argc, char *argv[])
{
    char *buf1;
    char *buf2;
    

    buf1 = url_encode("This is a test #$ ");
    buf2 = url_decode(buf1);

    printf("%s\n", buf2);
    
    free(buf1);
    free(buf2);
    return EXIT_FAILURE;
}

#endif

char *url_encode(char *url, size_t l)
{
    int i;
    int j;
    char *buf;
    unsigned char c;

    buf = (char *)malloc((l * 3) + 1);
    if(buf == NULL){
        perror("Could not allocate memory url encoding");
        return NULL;
    }

    for(i = 0, j = 0; i < l; i++){
        c = (unsigned char)url[i];
        if(c <= 31 || c >= 127 
            || c == '$' || c == '&' || c == '+' || c == ',' || c == '/' 
            || c == ':' || c == ';' || c == '=' || c == '?' || c == '@'
            || c == ' ' || c == '"' || c == '<' || c == '>' || c == '#'
            || c == '%' || c == '{' || c == '}' || c == '|' || c == '\\'
            || c == '^' || c == '~' || c == '[' || c == ']' || c == '`'){
            
            (void)sprintf(buf + j, "%%%X%X", c >> 4, c & 0x0F);
            j += 3;
        }else{
            buf[j] = url[i];
            j++;
        }
    }
    
    buf[j] = '\0';

    return buf;
}

char *url_decode(char *url, size_t l)
{
    int i;
    int j;
    char *buf;
    char c;
    char d0;
    char d1;

    buf = (char *)malloc((l + 1) * sizeof(char));
    if(buf == NULL){
        perror("Could not allocate memory for decoding");
        return NULL;
    }

    for(i = 0, j = 0; i < l; j++){
        c = url[i];
        if(c == '%'){
            d0 = url[i + 2];
            d1 = url[i + 1];
            d0 = toupper(d0);
            d1 = toupper(d1);

            if(d0 >= 'A' && d0 <= 'F') d0 = d0 - 'A' + 10;
            else if(d0 >= '0' && d0 <= '9') d0 = d0 - '0';
            if(d1 >= 'A' && d1 <= 'F') d1 = d1 - 'A' + 10;
            else if(d1 >= '0' && d1 <= '9') d1 = d1 - '0';

            buf[j] = (d1 << 4) + d0;
            i += 3;
        }else{
            buf[j] = url[i];
            i++;
        }
    }

    buf[j] = '\0';

    return buf;
}


