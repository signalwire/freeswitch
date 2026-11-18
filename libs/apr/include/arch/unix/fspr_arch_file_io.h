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
#include "fspr_thread_mutex.h"
#ifndef WAITIO_USES_POLL
#include "fspr_poll.h"
#endif

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
#ifdef BEOS
#include <kernel/OS.h>
#endif

#if BEOS_BONE
# ifndef BONE7
  /* prior to BONE/7 fd_set & select were defined in sys/socket.h */
#  include <sys/socket.h>
# else
  /* Be moved the fd_set stuff and also the FIONBIO definition... */
#  include <sys/ioctl.h>
# endif
#endif
/* End System headers */

#define APR_FILE_BUFSIZE 4096

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
#ifndef WAITIO_USES_POLL
    /* if there is a timeout set, then this pollset is used */
    fspr_pollset_t *pollset;
#endif
    /* Stuff for buffered mode */
    char *buffer;
    int bufpos;               /* Read/Write position in buffer */
    unsigned long dataRead;   /* amount of valid data read into buffer */
    int direction;            /* buffer being used for 0 = read, 1 = write */
    fspr_off_t filePtr;        /* position in file of handle */
#if APR_HAS_THREADS
    struct fspr_thread_mutex_t *thlock;
#endif
};

#if APR_HAS_LARGE_FILES && defined(_LARGEFILE64_SOURCE)
#define stat(f,b) stat64(f,b)
#define lstat(f,b) lstat64(f,b)
#define fstat(f,b) fstat64(f,b)
#define lseek(f,o,w) lseek64(f,o,w)
#define ftruncate(f,l) ftruncate64(f,l)
typedef struct stat64 struct_stat;
#else
typedef struct stat struct_stat;
#endif

struct fspr_dir_t {
    fspr_pool_t *pool;
    char *dirname;
    DIR *dirstruct;
    struct dirent *entry;
};

fspr_status_t fspr_unix_file_cleanup(void *);

mode_t fspr_unix_perms2mode(fspr_fileperms_t perms);
fspr_fileperms_t fspr_unix_mode2perms(mode_t mode);


#endif  /* ! FILE_IO_H */

