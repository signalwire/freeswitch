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

#include "fspr.h"
#include "fspr_private.h"
#include "fspr_general.h"
#include "fspr_tables.h"
#include "fspr_file_io.h"
#include "fspr_file_info.h"
#include "fspr_errno.h"
#include "fspr_lib.h"
#include "fspr_poll.h"

/* System headers the file I/O library needs */
#if APR_HAVE_FCNTL_H
#include <fcntl.h>
#endif
#if APR_HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if APR_HAVE_ERRNO_H
#include <errno.h>
#endif
#if APR_HAVE_STRING_H
#include <string.h>
#endif
#if APR_HAVE_STRINGS_H
#include <strings.h>
#endif
#if APR_HAVE_DIRENT_H
#include <dirent.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if APR_HAVE_UNISTD_H
#include <unistd.h>
#endif
#if APR_HAVE_STDIO_H
#include <stdio.h>
#endif
#if APR_HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if APR_HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif
#if APR_HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include <fsio.h>

/* End System headers */

#define APR_FILE_BUFSIZE 4096

#if APR_HAS_LARGE_FILES
#define lseek(f,o,w) lseek64(f,o,w)
#define ftruncate(f,l) ftruncate64(f,l)
#endif

typedef struct stat struct_stat;

struct fspr_file_t {
    fspr_pool_t *pool;
    int filedes;
    char *fname;
    fspr_int32_t flags;
    int eof_hit;
    int is_pipe;
    fspr_interval_time_t timeout;
    int buffered;
    enum {BLK_UNKNOWN, BLK_OFF, BLK_ON } blocking;
    int ungetchar;    /* Last char provided by an unget op. (-1 = no char)*/

    /* if there is a timeout set, then this pollset is used */
    fspr_pollset_t *pollset;

    /* Stuff for buffered mode */
    char *buffer;
    int bufpos;               /* Read/Write position in buffer */
    fspr_off_t dataRead;   /* amount of valid data read into buffer */
    int direction;            /* buffer being used for 0 = read, 1 = write */
    fspr_off_t filePtr;    /* position in file of handle */
#if APR_HAS_THREADS
    struct fspr_thread_mutex_t *thlock;
#endif
};

struct fspr_dir_t {
    fspr_pool_t *pool;
    char *dirname;
    DIR *dirstruct;
    struct dirent *entry;
};

typedef struct fspr_stat_entry_t fspr_stat_entry_t;

struct fspr_stat_entry_t {
    struct stat info;
    char *casedName;
    fspr_time_t expire;
    NXPathCtx_t pathCtx;
};

#define MAX_SERVER_NAME     64
#define MAX_VOLUME_NAME     64
#define MAX_PATH_NAME       256
#define MAX_FILE_NAME       256

#define DRIVE_ONLY          1

/* If the user passes d: vs. D: (or //mach/share vs. //MACH/SHARE),
 * we need to fold the case to canonical form.  This function is
 * supposed to do so.
 */
fspr_status_t filepath_root_case(char **rootpath, char *root, fspr_pool_t *p);

/* This function check to see of the given path includes a drive/volume
 * specifier.  If the _only_ parameter is set to DRIVE_ONLY then it 
 * check to see of the path only contains a drive/volume specifier and
 * nothing else.
 */
fspr_status_t filepath_has_drive(const char *rootpath, int only, fspr_pool_t *p);

/* This function compares the drive/volume specifiers for each given path.
 * It returns zero if they match or non-zero if not. 
 */
fspr_status_t filepath_compare_drive(const char *path1, const char *path2, fspr_pool_t *p);

fspr_status_t fspr_unix_file_cleanup(void *);

#endif  /* ! FILE_IO_H */

