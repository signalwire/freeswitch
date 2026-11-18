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
#include "fspr_portable.h"
#include "fspr_user.h"
#include "fspr_private.h"
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#if APR_HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if APR_HAVE_UNISTD_H
#include <unistd.h> /* for _POSIX_THREAD_SAFE_FUNCTIONS */
#endif
#define APR_WANT_MEMFUNC
#include "fspr_want.h"

#define PWBUF_SIZE 512

static fspr_status_t getpwnam_safe(const char *username,
                                  struct passwd *pw,
                                  char pwbuf[PWBUF_SIZE])
{
    struct passwd *pwptr;
#if APR_HAS_THREADS && defined(_POSIX_THREAD_SAFE_FUNCTIONS) && defined(HAVE_GETPWNAM_R)
    fspr_status_t rv;

    /* POSIX defines getpwnam_r() et al to return the error number
     * rather than set errno, and requires pwptr to be set to NULL if
     * the entry is not found, imply that "not found" is not an error
     * condition; some implementations do return 0 with pwptr set to
     * NULL. */
    rv = getpwnam_r(username, pw, pwbuf, PWBUF_SIZE, &pwptr);
    if (rv) {
        return rv;
    }
    if (pwptr == NULL) {
        return APR_ENOENT;
    }
#else
    /* Some platforms (e.g. FreeBSD 4.x) do not set errno on NULL "not
     * found" return values for the non-threadsafe function either. */
    errno = 0;
    if ((pwptr = getpwnam(username)) != NULL) {
        memcpy(pw, pwptr, sizeof *pw);
    }
    else {
        return errno ? errno : APR_ENOENT;
    }
#endif
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_uid_homepath_get(char **dirname,
                                               const char *username,
                                               fspr_pool_t *p)
{
    struct passwd pw;
    char pwbuf[PWBUF_SIZE];
    fspr_status_t rv;

    if ((rv = getpwnam_safe(username, &pw, pwbuf)) != APR_SUCCESS)
        return rv;

#ifdef OS2
    /* Need to manually add user name for OS/2 */
    *dirname = fspr_pstrcat(p, pw.pw_dir, pw.pw_name, NULL);
#else
    *dirname = fspr_pstrdup(p, pw.pw_dir);
#endif
    return APR_SUCCESS;
}



APR_DECLARE(fspr_status_t) fspr_uid_current(fspr_uid_t *uid,
                                          fspr_gid_t *gid,
                                          fspr_pool_t *p)
{
    *uid = getuid();
    *gid = getgid();
  
    return APR_SUCCESS;
}




APR_DECLARE(fspr_status_t) fspr_uid_get(fspr_uid_t *uid, fspr_gid_t *gid,
                                      const char *username, fspr_pool_t *p)
{
    struct passwd pw;
    char pwbuf[PWBUF_SIZE];
    fspr_status_t rv;
        
    if ((rv = getpwnam_safe(username, &pw, pwbuf)) != APR_SUCCESS)
        return rv;

    *uid = pw.pw_uid;
    *gid = pw.pw_gid;

    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_uid_name_get(char **username, fspr_uid_t userid,
                                           fspr_pool_t *p)
{
    struct passwd *pw;
#if APR_HAS_THREADS && defined(_POSIX_THREAD_SAFE_FUNCTIONS) && defined(HAVE_GETPWUID_R)
    struct passwd pwd;
    char pwbuf[PWBUF_SIZE];
    fspr_status_t rv;

    rv = getpwuid_r(userid, &pwd, pwbuf, sizeof(pwbuf), &pw);
    if (rv) {
        return rv;
    }

    if (pw == NULL) {
        return APR_ENOENT;
    }

#else
    errno = 0;
    if ((pw = getpwuid(userid)) == NULL) {
        return errno ? errno : APR_ENOENT;
    }
#endif
    *username = fspr_pstrdup(p, pw->pw_name);
    return APR_SUCCESS;
}
