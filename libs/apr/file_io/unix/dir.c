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

#include "fspr_arch_file_io.h"
#include "fspr_strings.h"
#include "fspr_portable.h"
#if APR_HAVE_SYS_SYSLIMITS_H
#include <sys/syslimits.h>
#endif
#if APR_HAVE_LIMITS_H
#include <limits.h>
#endif

static fspr_status_t dir_cleanup(void *thedir)
{
    fspr_dir_t *dir = thedir;
    if (closedir(dir->dirstruct) == 0) {
        return APR_SUCCESS;
    }
    else {
        return errno;
    }
} 

#define PATH_SEPARATOR '/'

/* Remove trailing separators that don't affect the meaning of PATH. */
static const char *path_canonicalize (const char *path, fspr_pool_t *pool)
{
    /* At some point this could eliminate redundant components.  For
     * now, it just makes sure there is no trailing slash. */
    fspr_size_t len = strlen (path);
    fspr_size_t orig_len = len;
    
    while ((len > 0) && (path[len - 1] == PATH_SEPARATOR))
        len--;
    
    if (len != orig_len)
        return fspr_pstrndup (pool, path, len);
    else
        return path;
}

/* Remove one component off the end of PATH. */
static char *path_remove_last_component (const char *path, fspr_pool_t *pool)
{
    const char *newpath = path_canonicalize (path, pool);
    int i;
    
    for (i = (strlen(newpath) - 1); i >= 0; i--) {
        if (path[i] == PATH_SEPARATOR)
            break;
    }

    return fspr_pstrndup (pool, path, (i < 0) ? 0 : i);
}

fspr_status_t fspr_dir_open(fspr_dir_t **new, const char *dirname, 
                          fspr_pool_t *pool)
{
    /* On some platforms (e.g., Linux+GNU libc), d_name[] in struct 
     * dirent is declared with enough storage for the name.  On other
     * platforms (e.g., Solaris 8 for Intel), d_name is declared as a
     * one-byte array.  Note: gcc evaluates this at compile time.
     */
    fspr_size_t dirent_size = 
        (sizeof((*new)->entry->d_name) > 1 ? 
         sizeof(struct dirent) : sizeof (struct dirent) + 255);
    DIR *dir = opendir(dirname);

    if (!dir) {
        return errno;
    }

    (*new) = (fspr_dir_t *)fspr_palloc(pool, sizeof(fspr_dir_t));

    (*new)->pool = pool;
    (*new)->dirname = fspr_pstrdup(pool, dirname);
    (*new)->dirstruct = dir;
    (*new)->entry = fspr_pcalloc(pool, dirent_size);

    fspr_pool_cleanup_register((*new)->pool, *new, dir_cleanup,
                              fspr_pool_cleanup_null);
    return APR_SUCCESS;
}

fspr_status_t fspr_dir_close(fspr_dir_t *thedir)
{
    return fspr_pool_cleanup_run(thedir->pool, thedir, dir_cleanup);
}

#ifdef DIRENT_TYPE
static fspr_filetype_e filetype_from_dirent_type(int type)
{
    switch (type) {
    case DT_REG:
        return APR_REG;
    case DT_DIR:
        return APR_DIR;
    case DT_LNK:
        return APR_LNK;
    case DT_CHR:
        return APR_CHR;
    case DT_BLK:
        return APR_BLK;
#if defined(DT_FIFO)
    case DT_FIFO:
        return APR_PIPE;
#endif
#if !defined(BEOS) && defined(DT_SOCK)
    case DT_SOCK:
        return APR_SOCK;
#endif
    default:
        return APR_UNKFILE;
    }
}
#endif

fspr_status_t fspr_dir_read(fspr_finfo_t *finfo, fspr_int32_t wanted,
                          fspr_dir_t *thedir)
{
    fspr_status_t ret = 0;
#ifdef DIRENT_TYPE
    fspr_filetype_e type;
#endif
#if APR_HAS_THREADS && defined(_POSIX_THREAD_SAFE_FUNCTIONS) \
                    && !defined(READDIR_IS_THREAD_SAFE)
    struct dirent *retent;

    ret = readdir_r(thedir->dirstruct, thedir->entry, &retent);

    /* Avoid the Linux problem where at end-of-directory thedir->entry
     * is set to NULL, but ret = APR_SUCCESS.
     */
    if(!ret && thedir->entry != retent)
        ret = APR_ENOENT;

    /* Solaris is a bit strange, if there are no more entries in the
     * directory, it returns EINVAL.  Since this is against POSIX, we
     * hack around the problem here.  EINVAL is possible from other
     * readdir implementations, but only if the result buffer is too small.
     * since we control the size of that buffer, we should never have
     * that problem.
     */
    if (ret == EINVAL) {
        ret = ENOENT;
    }
#else
    /* We're about to call a non-thread-safe readdir() that may
       possibly set `errno', and the logic below actually cares about
       errno after the call.  Therefore we need to clear errno first. */
    errno = 0;
    thedir->entry = readdir(thedir->dirstruct);
    if (thedir->entry == NULL) {
        /* If NULL was returned, this can NEVER be a success. Can it?! */
        if (errno == APR_SUCCESS) {
            ret = APR_ENOENT;
        }
        else
            ret = errno;
    }
#endif

    /* No valid bit flag to test here - do we want one? */
    finfo->fname = NULL;

    if (ret) {
        finfo->valid = 0;
        return ret;
    }

#ifdef DIRENT_TYPE
    type = filetype_from_dirent_type(thedir->entry->DIRENT_TYPE);
    if (type != APR_UNKFILE) {
        wanted &= ~APR_FINFO_TYPE;
    }
#endif
#ifdef DIRENT_INODE
    if (thedir->entry->DIRENT_INODE && thedir->entry->DIRENT_INODE != -1) {
        wanted &= ~APR_FINFO_INODE;
    }
#endif

    wanted &= ~APR_FINFO_NAME;

    if (wanted)
    {
        char fspec[APR_PATH_MAX];
        int off;
        fspr_cpystrn(fspec, thedir->dirname, sizeof(fspec));
        off = strlen(fspec);
        if ((fspec[off - 1] != '/') && (off + 1 < sizeof(fspec)))
            fspec[off++] = '/';
        fspr_cpystrn(fspec + off, thedir->entry->d_name, sizeof(fspec) - off);
        ret = fspr_stat(finfo, fspec, APR_FINFO_LINK | wanted, thedir->pool);
        /* We passed a stack name that will disappear */
        finfo->fname = NULL;
    }

    if (wanted && (ret == APR_SUCCESS || ret == APR_INCOMPLETE)) {
        wanted &= ~finfo->valid;
    }
    else {
        /* We don't bail because we fail to stat, when we are only -required-
         * to readdir... but the result will be APR_INCOMPLETE
         */
        finfo->pool = thedir->pool;
        finfo->valid = 0;
#ifdef DIRENT_TYPE
        if (type != APR_UNKFILE) {
            finfo->filetype = type;
            finfo->valid |= APR_FINFO_TYPE;
        }
#endif
#ifdef DIRENT_INODE
        if (thedir->entry->DIRENT_INODE && thedir->entry->DIRENT_INODE != -1) {
            finfo->inode = thedir->entry->DIRENT_INODE;
            finfo->valid |= APR_FINFO_INODE;
        }
#endif
    }

    finfo->name = fspr_pstrdup(thedir->pool, thedir->entry->d_name);
    finfo->valid |= APR_FINFO_NAME;

    if (wanted)
        return APR_INCOMPLETE;

    return APR_SUCCESS;
}

fspr_status_t fspr_dir_rewind(fspr_dir_t *thedir)
{
    rewinddir(thedir->dirstruct);
    return APR_SUCCESS;
}

fspr_status_t fspr_dir_make(const char *path, fspr_fileperms_t perm, 
                          fspr_pool_t *pool)
{
    mode_t mode = fspr_unix_perms2mode(perm);

    if (mkdir(path, mode) == 0) {
        return APR_SUCCESS;
    }
    else {
        return errno;
    }
}

fspr_status_t fspr_dir_make_recursive(const char *path, fspr_fileperms_t perm,
                                           fspr_pool_t *pool) 
{
    fspr_status_t fspr_err = 0;
    
    fspr_err = fspr_dir_make (path, perm, pool); /* Try to make PATH right out */
    
    if (fspr_err == EEXIST) /* It's OK if PATH exists */
        return APR_SUCCESS;
    
    if (fspr_err == ENOENT) { /* Missing an intermediate dir */
        char *dir;
        
        dir = path_remove_last_component(path, pool);
        /* If there is no path left, give up. */
        if (dir[0] == '\0') {
            return fspr_err;
        }

        fspr_err = fspr_dir_make_recursive(dir, perm, pool);
        
        if (!fspr_err) 
            fspr_err = fspr_dir_make (path, perm, pool);
    }

    return fspr_err;
}

fspr_status_t fspr_dir_remove(const char *path, fspr_pool_t *pool)
{
    if (rmdir(path) == 0) {
        return APR_SUCCESS;
    }
    else {
        return errno;
    }
}

fspr_status_t fspr_os_dir_get(fspr_os_dir_t **thedir, fspr_dir_t *dir)
{
    if (dir == NULL) {
        return APR_ENODIR;
    }
    *thedir = dir->dirstruct;
    return APR_SUCCESS;
}

fspr_status_t fspr_os_dir_put(fspr_dir_t **dir, fspr_os_dir_t *thedir,
                          fspr_pool_t *pool)
{
    if ((*dir) == NULL) {
        (*dir) = (fspr_dir_t *)fspr_pcalloc(pool, sizeof(fspr_dir_t));
        (*dir)->pool = pool;
    }
    (*dir)->dirstruct = thedir;
    return APR_SUCCESS;
}

  
