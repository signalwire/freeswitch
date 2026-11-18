/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include "fspr_strings.h"
#include "fspr_pools.h"
#include "fspr_general.h"
#include "fspr_hash.h"
#include "fspr_lib.h"
#include "fspr_time.h"
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>

int main( int argc, char** argv) {
    fspr_pool_t *context;
    regex_t regex;
    int rc;
    int i;
    int iters;
    fspr_time_t now;
    fspr_time_t end;
    fspr_hash_t *h;
    

    if (argc !=4 ) {
            fprintf(stderr, "Usage %s match string #iterations\n",argv[0]);
            return -1;
    }
    iters = atoi( argv[3]);
    
    fspr_initialize() ;
    atexit(fspr_terminate);
    if (fspr_pool_create(&context, NULL) != APR_SUCCESS) {
        fprintf(stderr, "Something went wrong\n");
        exit(-1);
    }
    rc = regcomp( &regex, argv[1], REG_EXTENDED|REG_NOSUB);


    if (rc) {
        char errbuf[2000];
        regerror(rc, &regex,errbuf,2000);
        fprintf(stderr,"Couldn't compile regex ;(\n%s\n ",errbuf);
        return -1;
    }
    if ( regexec( &regex, argv[2], 0, NULL,0) == 0 ) {
        fprintf(stderr,"Match\n");
    }
    else {
        fprintf(stderr,"No Match\n");
    }
    now = fspr_time_now();
    for (i=0;i<iters;i++) {
        regexec( &regex, argv[2], 0, NULL,0) ;
    }
    end=fspr_time_now();
    puts(fspr_psprintf( context, "Time to run %d regex's          %8lld\n",iters,end-now));
    h = fspr_hash_make( context);
    for (i=0;i<70;i++) {
            fspr_hash_set(h,fspr_psprintf(context, "%dkey",i),APR_HASH_KEY_STRING,"1");
    }
    now = fspr_time_now();
    for (i=0;i<iters;i++) {
        fspr_hash_get( h, argv[2], APR_HASH_KEY_STRING);
    }
    end=fspr_time_now();
    puts(fspr_psprintf( context, "Time to run %d hash (no find)'s %8lld\n",iters,end-now));
    fspr_hash_set(h, argv[2],APR_HASH_KEY_STRING,"1");
    now = fspr_time_now();
    for (i=0;i<iters;i++) {
        fspr_hash_get( h, argv[2], APR_HASH_KEY_STRING);
    }
    end=fspr_time_now();
    puts(fspr_psprintf( context, "Time to run %d hash (find)'s    %8lld\n",iters,end-now));
 
    return 0;
}
