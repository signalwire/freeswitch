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

#ifndef FILE_IO_H
#define FILE_IO_H

#include "fspr_private.h"
#include "fspr_general.h"
#include "fspr_thread_mutex.h"
#include "fspr_file_io.h"
#include "fspr_file_info.h"
#include "fspr_errno.h"
#include "fspr_poll.h"

/* We have an implementation of mkstemp but it's not very multi-threading 
 * friendly & is part of the POSIX emulation rather than native so don't
 * use it.
 */
#undef HAVE_MKSTEMP

#define APR_FILE_BUFSIZE 4096

struct fspr_file_t {
    fspr_pool_t *pool;
    HFILE filedes;
    char * fname;
    int isopen;
    int buffered;
    int eof_hit;
    fspr_int32_t flags;
    int timeout;
    int pipe;
    HEV pipeSem;
    enum { BLK_UNKNOWN, BLK_OFF, BLK_ON } blocking;

    /* Stuff for buffered mode */
    char *buffer;
    int bufpos;               // Read/Write position in buffer
    unsigned long dataRead;   // amount of valid data read into buffer
    int direction;            // buffer being used for 0 = read, 1 = write
    unsigned long filePtr;    // position in file of handle
    fspr_thread_mutex_t *mutex;// mutex semaphore, must be owned to access the above fields
};

struct fspr_dir_t {
    fspr_pool_t *pool;
    char *dirname;
    ULONG handle;
    FILEFINDBUF3 entry;
    int validentry;
};

fspr_status_t fspr_file_cleanup(void *);
fspr_status_t fspr_os2_time_to_fspr_time(fspr_time_t *result, FDATE os2date, 
                                      FTIME os2time);
fspr_status_t fspr_fspr_time_to_os2_time(FDATE *os2date, FTIME *os2time,
                                      fspr_time_t aprtime);

/* see win32/fileio.h for description of these */
extern const char c_is_fnchar[256];

#define IS_FNCHAR(c) c_is_fnchar[(unsigned char)c]

fspr_status_t filepath_root_test(char *path, fspr_pool_t *p);
fspr_status_t filepath_drive_get(char **rootpath, char drive, 
                                fspr_int32_t flags, fspr_pool_t *p);
fspr_status_t filepath_root_case(char **rootpath, char *root, fspr_pool_t *p);

#endif  /* ! FILE_IO_H */

