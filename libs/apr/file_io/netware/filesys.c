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

#include "fspr.h"
#include "fspr_arch_file_io.h"
#include "fspr_strings.h"

fspr_status_t filepath_root_case(char **rootpath, char *root, fspr_pool_t *p)
{
/* See the Windows code to figure out what to do here.
    It probably checks to make sure that the root exists 
    and case it correctly according to the file system.
*/
    *rootpath = fspr_pstrdup(p, root);
    return APR_SUCCESS;
}

fspr_status_t filepath_has_drive(const char *rootpath, int only, fspr_pool_t *p)
{
    char *s;

    if (rootpath) {
        s = strchr (rootpath, ':');
        if (only)
            /* Test if the path only has a drive/volume and nothing else
            */
            return (s && (s != rootpath) && !s[1]);
        else
            /* Test if the path includes a drive/volume
            */
            return (s && (s != rootpath));
    }
    return 0;
}

fspr_status_t filepath_compare_drive(const char *path1, const char *path2, fspr_pool_t *p)
{
    char *s1, *s2;

    if (path1 && path2) {
        s1 = strchr (path1, ':');
        s2 = strchr (path2, ':');

        /* Make sure that they both have a drive/volume delimiter
            and are the same size.  Then see if they match.
        */
        if (s1 && s2 && ((s1-path1) == (s2-path2))) {
            return strnicmp (s1, s2, s1-path1);
        }
    }
    return -1;
}

APR_DECLARE(fspr_status_t) fspr_filepath_get(char **rootpath, fspr_int32_t flags,
                                           fspr_pool_t *p)
{
    char path[APR_PATH_MAX];
    char *ptr;

    /* use getcwdpath to make sure that we get the volume name*/
    if (!getcwdpath(path, NULL, 0)) {
        if (errno == ERANGE)
            return APR_ENAMETOOLONG;
        else
            return errno;
    }
    /* Strip off the server name if there is one*/
    ptr = strpbrk(path, "\\/:");
    if (!ptr) {
        return APR_ENOENT;
    }
    if (*ptr == ':') {
        ptr = path;
    }
    *rootpath = fspr_pstrdup(p, ptr);
    if (!(flags & APR_FILEPATH_NATIVE)) {
        for (ptr = *rootpath; *ptr; ++ptr) {
            if (*ptr == '\\')
                *ptr = '/';
        }
    }
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_filepath_set(const char *rootpath,
                                           fspr_pool_t *p)
{
    if (chdir2(rootpath) != 0)
        return errno;
    return APR_SUCCESS;
}


