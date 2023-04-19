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
#include "fspr_private.h"
#include "fspr_file_io.h"
#include "fspr_strings.h"
#include "fspr_env.h"


/* Try to open a temporary file in the temporary dir, write to it,
   and then close it. */
static int test_tempdir(const char *temp_dir, fspr_pool_t *p)
{
    fspr_file_t *dummy_file;
    char *path = fspr_pstrcat(p, temp_dir, "/apr-tmp.XXXXXX", NULL);

    if (fspr_file_mktemp(&dummy_file, path, 0, p) == APR_SUCCESS) {
        if (fspr_file_putc('!', dummy_file) == APR_SUCCESS) {
            if (fspr_file_close(dummy_file) == APR_SUCCESS) {
                return 1;
            }
        }
    }
    return 0;
}


APR_DECLARE(fspr_status_t) fspr_temp_dir_get(const char **temp_dir, 
                                           fspr_pool_t *p)
{
    fspr_status_t fspr_err;
    const char *try_dirs[] = { "/tmp", "/usr/tmp", "/var/tmp" };
    const char *try_envs[] = { "TMP", "TEMP", "TMPDIR" };
    const char *dir;
    char *cwd;
    int i;

    /* Our goal is to find a temporary directory suitable for writing
       into.  We'll only pay the price once if we're successful -- we
       cache our successful find.  Here's the order in which we'll try
       various paths:

          $TMP
          $TEMP
          $TMPDIR
          "C:\TEMP"     (windows only)
          "SYS:\TMP"    (netware only)
          "/tmp"
          "/var/tmp"
          "/usr/tmp"
          P_tmpdir      (POSIX define)
          `pwd` 

       NOTE: This algorithm is basically the same one used by Python
       2.2's tempfile.py module.  */

    /* Try the environment first. */
    for (i = 0; i < (sizeof(try_envs) / sizeof(const char *)); i++) {
        char *value;
        fspr_err = fspr_env_get(&value, try_envs[i], p);
        if ((fspr_err == APR_SUCCESS) && value) {
            fspr_size_t len = strlen(value);
            if (len && (len < APR_PATH_MAX) && test_tempdir(value, p)) {
                dir = value;
                goto end;
            }
        }
    }

#ifdef WIN32
    /* Next, on Win32, try the C:\TEMP directory. */
    if (test_tempdir("C:\\TEMP", p)) {
        dir = "C:\\TEMP";
        goto end;
    }
#endif
#ifdef NETWARE
    /* Next, on NetWare, try the SYS:/TMP directory. */
    if (test_tempdir("SYS:/TMP", p)) {
        dir = "SYS:/TMP";
        goto end;
    }
#endif

    /* Next, try a set of hard-coded paths. */
    for (i = 0; i < (sizeof(try_dirs) / sizeof(const char *)); i++) {
        if (test_tempdir(try_dirs[i], p)) {
            dir = try_dirs[i];
            goto end;
        }
    }

#ifdef P_tmpdir
    /* 
     * If we have it, use the POSIX definition of where 
     * the tmpdir should be 
     */
    if (test_tempdir(P_tmpdir, p)) {
        dir = P_tmpdir;
        goto end;
    }
#endif
    
    /* Finally, try the current working directory. */
    if (APR_SUCCESS == fspr_filepath_get(&cwd, APR_FILEPATH_NATIVE, p)) {
        if (test_tempdir(cwd, p)) {
            dir = cwd;
	    goto end;
        }
    }

    /* We didn't find a suitable temp dir anywhere */
    return APR_EGENERAL;

end:
    *temp_dir = fspr_pstrdup(p, dir);
    return APR_SUCCESS;
}
