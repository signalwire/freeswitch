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
#include "fspr_arch_file_io.h"
#include "fspr_file_io.h"
#include "fspr_general.h"
#include "fspr_strings.h"
#include "fspr_portable.h"
#include "fspr_thread_mutex.h"
#if APR_HAVE_ERRNO_H
#include <errno.h>
#endif
#include <winbase.h>
#include <string.h>
#if APR_HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#include "fspr_arch_misc.h"
#include "fspr_arch_inherit.h"

#if APR_HAS_UNICODE_FS
fspr_status_t utf8_to_unicode_path(fspr_wchar_t* retstr, fspr_size_t retlen, 
                                  const char* srcstr)
{
    /* TODO: The computations could preconvert the string to determine
     * the true size of the retstr, but that's a memory over speed
     * tradeoff that isn't appropriate this early in development.
     *
     * Allocate the maximum string length based on leading 4 
     * characters of \\?\ (allowing nearly unlimited path lengths) 
     * plus the trailing null, then transform /'s into \\'s since
     * the \\?\ form doesn't allow '/' path seperators.
     *
     * Note that the \\?\ form only works for local drive paths, and
     * \\?\UNC\ is needed UNC paths.
     */
    fspr_size_t srcremains = strlen(srcstr) + 1;
    fspr_wchar_t *t = retstr;
    fspr_status_t rv;

    /* This is correct, we don't twist the filename if it is will
     * definately be shorter than MAX_PATH.  It merits some 
     * performance testing to see if this has any effect, but there
     * seem to be applications that get confused by the resulting
     * Unicode \\?\ style file names, especially if they use argv[0]
     * or call the Win32 API functions such as GetModuleName, etc.
     * Not every application is prepared to handle such names.
     *
     * Note that a utf-8 name can never result in more wide chars
     * than the original number of utf-8 narrow chars.
     */
    if (srcremains > MAX_PATH) {
        if (srcstr[1] == ':' && (srcstr[2] == '/' || srcstr[2] == '\\')) {
            wcscpy (retstr, L"\\\\?\\");
            retlen -= 4;
            t += 4;
        }
        else if ((srcstr[0] == '/' || srcstr[0] == '\\')
              && (srcstr[1] == '/' || srcstr[1] == '\\')
              && (srcstr[2] != '?')) {
            /* Skip the slashes */
            srcstr += 2;
            srcremains -= 2;
            wcscpy (retstr, L"\\\\?\\UNC\\");
            retlen -= 8;
            t += 8;
        }
    }

    if (rv = fspr_conv_utf8_to_ucs2(srcstr, &srcremains, t, &retlen)) {
        return (rv == APR_INCOMPLETE) ? APR_EINVAL : rv;
    }
    if (srcremains) {
        return APR_ENAMETOOLONG;
    }
    for (; *t; ++t)
        if (*t == L'/')
            *t = L'\\';
    return APR_SUCCESS;
}

fspr_status_t unicode_to_utf8_path(char* retstr, fspr_size_t retlen,
                                  const fspr_wchar_t* srcstr)
{
    /* Skip the leading 4 characters if the path begins \\?\, or substitute
     * // for the \\?\UNC\ path prefix, allocating the maximum string
     * length based on the remaining string, plus the trailing null.
     * then transform \\'s back into /'s since the \\?\ form never
     * allows '/' path seperators, and APR always uses '/'s.
     */
    fspr_size_t srcremains = wcslen(srcstr) + 1;
    fspr_status_t rv;
    char *t = retstr;
    if (srcstr[0] == L'\\' && srcstr[1] == L'\\' && 
        srcstr[2] == L'?'  && srcstr[3] == L'\\') {
        if (srcstr[4] == L'U' && srcstr[5] == L'N' && 
            srcstr[6] == L'C' && srcstr[7] == L'\\') {
            srcremains -= 8;
            srcstr += 8;
            retstr[0] = '\\';
            retstr[1] = '\\';
            retlen -= 2;
            t += 2;
        }
        else {
            srcremains -= 4;
            srcstr += 4;
        }
    }
        
    if (rv = fspr_conv_ucs2_to_utf8(srcstr, &srcremains, t, &retlen)) {
        return rv;
    }
    if (srcremains) {
        return APR_ENAMETOOLONG;
    }
    return APR_SUCCESS;
}
#endif

void *res_name_from_filename(const char *file, int global, fspr_pool_t *pool)
{
#if APR_HAS_UNICODE_FS
    IF_WIN_OS_IS_UNICODE
    {
        fspr_wchar_t *wpre, *wfile, *ch;
        fspr_size_t n = strlen(file) + 1;
        fspr_size_t r, d;
        fspr_status_t rv;

        if (fspr_os_level >= APR_WIN_2000) {
            if (global)
                wpre = L"Global\\";
            else
                wpre = L"Local\\";
        }
        else
            wpre = L"";
        r = wcslen(wpre);

        if (n > 256 - r) {
            file += n - 256 - r;
            n = 256;
            /* skip utf8 continuation bytes */
            while ((*file & 0xC0) == 0x80) {
                ++file;
                --n;
            }
        }
        wfile = fspr_palloc(pool, (r + n) * sizeof(fspr_wchar_t));
        wcscpy(wfile, wpre);
        d = n;
        if (rv = fspr_conv_utf8_to_ucs2(file, &n, wfile + r, &d)) {
            return NULL;
        }
        for (ch = wfile + r; *ch; ++ch) {
            if (*ch == ':' || *ch == '/' || *ch == '\\')
                *ch = '_';
        }
        return wfile;
    }
#endif
#if APR_HAS_ANSI_FS
    ELSE_WIN_OS_IS_ANSI
    {
        char *nfile, *ch;
        fspr_size_t n = strlen(file) + 1;

#if !APR_HAS_UNICODE_FS
        fspr_status_t rv;
        fspr_size_t r, d;
        char *pre;

        if (fspr_os_level >= APR_WIN_2000) {
            if (global)
                pre = "Global\\";
            else
                pre = "Local\\";
        }
        else
            pre = "";
        r = strlen(pre);

        if (n > 256 - r) {
            file += n - 256 - r;
            n = 256;
        }
        nfile = fspr_palloc(pool, (r + n) * sizeof(fspr_wchar_t));
        memcpy(nfile, pre, r);
        memcpy(nfile + r, file, n);
#else
        const fspr_size_t r = 0;
        if (n > 256) {
            file += n - 256;
            n = 256;
        }
        nfile = fspr_pmemdup(pool, file, n);
#endif
        for (ch = nfile + r; *ch; ++ch) {
            if (*ch == ':' || *ch == '/' || *ch == '\\')
                *ch = '_';
        }
        return nfile;
    }
#endif
}


fspr_status_t file_cleanup(void *thefile)
{
    fspr_file_t *file = thefile;
    fspr_status_t flush_rv = APR_SUCCESS;

    if (file->filehand != INVALID_HANDLE_VALUE) {

        /* In order to avoid later segfaults with handle 'reuse',
         * we must protect against the case that a dup2'ed handle
         * is being closed, and invalidate the corresponding StdHandle 
         */
        if (file->filehand == GetStdHandle(STD_ERROR_HANDLE)) {
            SetStdHandle(STD_ERROR_HANDLE, INVALID_HANDLE_VALUE);
        }
        if (file->filehand == GetStdHandle(STD_OUTPUT_HANDLE)) {
            SetStdHandle(STD_OUTPUT_HANDLE, INVALID_HANDLE_VALUE);
        }
        if (file->filehand == GetStdHandle(STD_INPUT_HANDLE)) {
            SetStdHandle(STD_INPUT_HANDLE, INVALID_HANDLE_VALUE);
        }

        if (file->buffered) {
            /* XXX: flush here is not mutex protected */
            flush_rv = fspr_file_flush((fspr_file_t *)thefile);
        }
        CloseHandle(file->filehand);
        file->filehand = INVALID_HANDLE_VALUE;
    }
    if (file->pOverlapped && file->pOverlapped->hEvent) {
        CloseHandle(file->pOverlapped->hEvent);
        file->pOverlapped = NULL;
    }
    return flush_rv;
}

APR_DECLARE(fspr_status_t) fspr_file_open(fspr_file_t **new, const char *fname,
                                   fspr_int32_t flag, fspr_fileperms_t perm,
                                   fspr_pool_t *pool)
{
    HANDLE handle = INVALID_HANDLE_VALUE;
    DWORD oflags = 0;
    DWORD createflags = 0;
    DWORD attributes = 0;
    DWORD sharemode = FILE_SHARE_READ | FILE_SHARE_WRITE;
    fspr_status_t rv;

    if (flag & APR_READ) {
        oflags |= GENERIC_READ;
    }
    if (flag & APR_WRITE) {
        oflags |= GENERIC_WRITE;
    }
    if (flag & APR_WRITEATTRS) {
        oflags |= FILE_WRITE_ATTRIBUTES;
    }

    if (fspr_os_level >= APR_WIN_NT) 
        sharemode |= FILE_SHARE_DELETE;

    if (flag & APR_CREATE) {
        if (flag & APR_EXCL) {
            /* only create new if file does not already exist */
            createflags = CREATE_NEW;
        } else if (flag & APR_TRUNCATE) {
            /* truncate existing file or create new */
            createflags = CREATE_ALWAYS;
        } else {
            /* open existing but create if necessary */
            createflags = OPEN_ALWAYS;
        }
    } else if (flag & APR_TRUNCATE) {
        /* only truncate if file already exists */
        createflags = TRUNCATE_EXISTING;
    } else {
        /* only open if file already exists */
        createflags = OPEN_EXISTING;
    }

    if ((flag & APR_EXCL) && !(flag & APR_CREATE)) {
        return APR_EACCES;
    }   
    
    if (flag & APR_DELONCLOSE) {
        attributes |= FILE_FLAG_DELETE_ON_CLOSE;
    }

    if (flag & APR_OPENLINK) {
       attributes |= FILE_FLAG_OPEN_REPARSE_POINT;
    }

    /* Without READ or WRITE, we fail unless apr called fspr_file_open
     * internally with the private APR_OPENINFO flag.
     *
     * With the APR_OPENINFO flag on NT, use the option flag
     * FILE_FLAG_BACKUP_SEMANTICS to allow us to open directories.
     * See the static resolve_ident() fn in file_io/win32/filestat.c
     */
    if (!(flag & (APR_READ | APR_WRITE))) {
        if (flag & APR_OPENINFO) {
            if (fspr_os_level >= APR_WIN_NT) {
                attributes |= FILE_FLAG_BACKUP_SEMANTICS;
            }
        }
        else {
            return APR_EACCES;
        }
        if (flag & APR_READCONTROL)
            oflags |= READ_CONTROL;
    }

    if (flag & APR_XTHREAD) {
        /* This win32 specific feature is required 
         * to allow multiple threads to work with the file.
         */
        attributes |= FILE_FLAG_OVERLAPPED;
    }

#if APR_HAS_UNICODE_FS
    IF_WIN_OS_IS_UNICODE
    {
        fspr_wchar_t wfname[APR_PATH_MAX];

        if (flag & APR_SENDFILE_ENABLED) {    
            /* This feature is required to enable sendfile operations
             * against the file on Win32. Also implies APR_XTHREAD.
             */
            flag |= APR_XTHREAD;
            attributes |= FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_OVERLAPPED;
        }

        if (rv = utf8_to_unicode_path(wfname, sizeof(wfname) 
                                               / sizeof(fspr_wchar_t), fname))
            return rv;
        handle = CreateFileW(wfname, oflags, sharemode,
                             NULL, createflags, attributes, 0);
    }
#endif
#if APR_HAS_ANSI_FS
    ELSE_WIN_OS_IS_ANSI {
        handle = CreateFileA(fname, oflags, sharemode,
                             NULL, createflags, attributes, 0);
        if (flag & APR_SENDFILE_ENABLED) {    
            /* This feature is not supported on this platform.
             */
            flag &= ~APR_SENDFILE_ENABLED;
        }

    }
#endif
    if (handle == INVALID_HANDLE_VALUE) {
        return fspr_get_os_error();
    }

    (*new) = (fspr_file_t *)fspr_pcalloc(pool, sizeof(fspr_file_t));
    (*new)->pool = pool;
    (*new)->filehand = handle;
    (*new)->fname = fspr_pstrdup(pool, fname);
    (*new)->flags = flag;
    (*new)->timeout = -1;
    (*new)->ungetchar = -1;

    if (flag & APR_APPEND) {
        (*new)->append = 1;
        SetFilePointer((*new)->filehand, 0, NULL, FILE_END);
    }
    if (flag & APR_BUFFERED) {
        (*new)->buffered = 1;
        (*new)->buffer = fspr_palloc(pool, APR_FILE_BUFSIZE);
    }
    /* Need the mutex to handled buffered and O_APPEND style file i/o */
    if ((*new)->buffered || (*new)->append) {
        rv = fspr_thread_mutex_create(&(*new)->mutex, 
                                     APR_THREAD_MUTEX_DEFAULT, pool);
        if (rv) {
            if (file_cleanup(*new) == APR_SUCCESS) {
                fspr_pool_cleanup_kill(pool, *new, file_cleanup);
            }
            return rv;
        }
    }

    /* Create a pollset with room for one descriptor. */
    /* ### check return codes */
    (void) fspr_pollset_create(&(*new)->pollset, 1, pool, 0);

    if (!(flag & APR_FILE_NOCLEANUP)) {
        fspr_pool_cleanup_register((*new)->pool, (void *)(*new), file_cleanup,
                                  fspr_pool_cleanup_null);
    }
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_file_close(fspr_file_t *file)
{
    fspr_status_t stat;
    if ((stat = file_cleanup(file)) == APR_SUCCESS) {
        fspr_pool_cleanup_kill(file->pool, file, file_cleanup);

        if (file->mutex) {
            fspr_thread_mutex_destroy(file->mutex);
        }

        return APR_SUCCESS;
    }
    return stat;
}

APR_DECLARE(fspr_status_t) fspr_file_remove(const char *path, fspr_pool_t *pool)
{
#if APR_HAS_UNICODE_FS
    IF_WIN_OS_IS_UNICODE
    {
        fspr_wchar_t wpath[APR_PATH_MAX];
        fspr_status_t rv;
        if (rv = utf8_to_unicode_path(wpath, sizeof(wpath) 
                                              / sizeof(fspr_wchar_t), path)) {
            return rv;
        }
        if (DeleteFileW(wpath))
            return APR_SUCCESS;
    }
#endif
#if APR_HAS_ANSI_FS
    ELSE_WIN_OS_IS_ANSI
        if (DeleteFile(path))
            return APR_SUCCESS;
#endif
    return fspr_get_os_error();
}

APR_DECLARE(fspr_status_t) fspr_file_rename(const char *frompath,
                                          const char *topath,
                                          fspr_pool_t *pool)
{
    IF_WIN_OS_IS_UNICODE
    {
#if APR_HAS_UNICODE_FS
        fspr_wchar_t wfrompath[APR_PATH_MAX], wtopath[APR_PATH_MAX];
        fspr_status_t rv;
        if (rv = utf8_to_unicode_path(wfrompath, sizeof(wfrompath) 
                                           / sizeof(fspr_wchar_t), frompath)) {
            return rv;
        }
        if (rv = utf8_to_unicode_path(wtopath, sizeof(wtopath) 
                                             / sizeof(fspr_wchar_t), topath)) {
            return rv;
        }
#ifndef _WIN32_WCE
        if (MoveFileExW(wfrompath, wtopath, MOVEFILE_REPLACE_EXISTING |
                                            MOVEFILE_COPY_ALLOWED))
#else
        if (MoveFileW(wfrompath, wtopath))
#endif
            return APR_SUCCESS;
#else
        if (MoveFileEx(frompath, topath, MOVEFILE_REPLACE_EXISTING |
                                         MOVEFILE_COPY_ALLOWED))
            return APR_SUCCESS;
#endif
    }
#if APR_HAS_ANSI_FS
    ELSE_WIN_OS_IS_ANSI
    {
        /* Windows 95 and 98 do not support MoveFileEx, so we'll use
         * the old MoveFile function.  However, MoveFile requires that
         * the new file not already exist...so we have to delete that
         * file if it does.  Perhaps we should back up the to-be-deleted
         * file in case something happens?
         */
        HANDLE handle = INVALID_HANDLE_VALUE;

        if ((handle = CreateFile(topath, GENERIC_WRITE, 0, 0,  
            OPEN_EXISTING, 0, 0 )) != INVALID_HANDLE_VALUE )
        {
            CloseHandle(handle);
            if (!DeleteFile(topath))
                return fspr_get_os_error();
        }
        if (MoveFile(frompath, topath))
            return APR_SUCCESS;
    }        
#endif
    return fspr_get_os_error();
}

APR_DECLARE(fspr_status_t) fspr_os_file_get(fspr_os_file_t *thefile,
                                          fspr_file_t *file)
{
    *thefile = file->filehand;
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_os_file_put(fspr_file_t **file,
                                          fspr_os_file_t *thefile,
                                          fspr_int32_t flags,
                                          fspr_pool_t *pool)
{
    (*file) = fspr_pcalloc(pool, sizeof(fspr_file_t));
    (*file)->pool = pool;
    (*file)->filehand = *thefile;
    (*file)->ungetchar = -1; /* no char avail */
    (*file)->timeout = -1;
    (*file)->flags = flags;

    if (flags & APR_APPEND) {
        (*file)->append = 1;
    }
    if (flags & APR_BUFFERED) {
        (*file)->buffered = 1;
        (*file)->buffer = fspr_palloc(pool, APR_FILE_BUFSIZE);
    }

    if ((*file)->append || (*file)->buffered) {
        fspr_status_t rv;
        rv = fspr_thread_mutex_create(&(*file)->mutex, 
                                     APR_THREAD_MUTEX_DEFAULT, pool);
        if (rv) {
            if (file_cleanup(*file) == APR_SUCCESS) {
                fspr_pool_cleanup_kill(pool, *file, file_cleanup);
            }
            return rv;
        }
    }

    /* Create a pollset with room for one descriptor. */
    /* ### check return codes */
    (void) fspr_pollset_create(&(*file)->pollset, 1, pool, 0);

    /* XXX... we pcalloc above so all others are zeroed.
     * Should we be testing if thefile is a handle to 
     * a PIPE and set up the mechanics appropriately?
     *
     *  (*file)->pipe;
     */
    return APR_SUCCESS;
}    

APR_DECLARE(fspr_status_t) fspr_file_eof(fspr_file_t *fptr)
{
    if (fptr->eof_hit == 1) {
        return APR_EOF;
    }
    return APR_SUCCESS;
}   

APR_DECLARE(fspr_status_t) fspr_file_open_stderr(fspr_file_t **thefile, fspr_pool_t *pool)
{
#ifdef _WIN32_WCE
    return APR_ENOTIMPL;
#else
    fspr_os_file_t file_handle;

    fspr_set_os_error(APR_SUCCESS);
    file_handle = GetStdHandle(STD_ERROR_HANDLE);
    if (!file_handle || (file_handle == INVALID_HANDLE_VALUE)) {
        fspr_status_t rv = fspr_get_os_error();
        if (rv == APR_SUCCESS) {
            return APR_EINVAL;
        }
        return rv;
    }

    return fspr_os_file_put(thefile, &file_handle, 0, pool);
#endif
}

APR_DECLARE(fspr_status_t) fspr_file_open_stdout(fspr_file_t **thefile, fspr_pool_t *pool)
{
#ifdef _WIN32_WCE
    return APR_ENOTIMPL;
#else
    fspr_os_file_t file_handle;

    fspr_set_os_error(APR_SUCCESS);
    file_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!file_handle || (file_handle == INVALID_HANDLE_VALUE)) {
        fspr_status_t rv = fspr_get_os_error();
        if (rv == APR_SUCCESS) {
            return APR_EINVAL;
        }
        return rv;
    }

    return fspr_os_file_put(thefile, &file_handle, 0, pool);
#endif
}

APR_DECLARE(fspr_status_t) fspr_file_open_stdin(fspr_file_t **thefile, fspr_pool_t *pool)
{
#ifdef _WIN32_WCE
    return APR_ENOTIMPL;
#else
    fspr_os_file_t file_handle;

    fspr_set_os_error(APR_SUCCESS);
    file_handle = GetStdHandle(STD_INPUT_HANDLE);
    if (!file_handle || (file_handle == INVALID_HANDLE_VALUE)) {
        fspr_status_t rv = fspr_get_os_error();
        if (rv == APR_SUCCESS) {
            return APR_EINVAL;
        }
        return rv;
    }

    return fspr_os_file_put(thefile, &file_handle, 0, pool);
#endif
}

APR_POOL_IMPLEMENT_ACCESSOR(file);

APR_IMPLEMENT_INHERIT_SET(file, flags, pool, file_cleanup)
 
APR_IMPLEMENT_INHERIT_UNSET(file, flags, pool, file_cleanup)
