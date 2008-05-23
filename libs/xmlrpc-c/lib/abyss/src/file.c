/******************************************************************************
**
** file.c
**
** This file is part of the ABYSS Web server project.
**
** Copyright (C) 2000 by Moez Mahfoudh <mmoez@bigfoot.com>.
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
** 
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
** OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
** HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
** SUCH DAMAGE.
**
******************************************************************************/

#include "xmlrpc_config.h"
#include "mallocvar.h"

#define _CRT_SECURE_NO_WARNINGS
    /* Tell msvcrt not to warn about functions that are often misused and
       cause security exposures.
    */

#define _FILE_OFFSET_BITS 64
    /* Tell GNU libc to make off_t 64 bits and all the POSIX file functions
       the versions that handle 64 bit file offsets.
    */
#define _LARGE_FILES
    /* Same as above, but for AIX */

#include <string.h>

#if MSVCRT
  #include <io.h>
  typedef __int64 readwriterc_t;
#else
  #include <unistd.h>
  #include <fcntl.h>
  #include <dirent.h>
  #include <sys/stat.h>
  typedef ssize_t readwriterc_t;
#endif

#include "bool.h"
#include "xmlrpc-c/string_int.h"
#include "xmlrpc-c/abyss.h"
#include "file.h"

bool const win32 =
#ifdef WIN32
TRUE;
#else
FALSE;
#endif

struct TFileFind {
#ifdef WIN32
  #if MSVCRT
      intptr_t handle;
  #else
      HANDLE handle;
  #endif
#else
    char path[NAME_MAX+1];
    DIR * handle;
#endif
};

    

/*********************************************************************
** File
*********************************************************************/

static void
createFileImage(TFile **     const filePP,
                const char * const name,
                uint32_t     const attrib,
                bool         const createFile,
                bool *       const succeededP) {

    TFile * fileP;

    MALLOCVAR(fileP);
    if (fileP == NULL)
        *succeededP = FALSE;
    else {
        int rc;

        if (createFile)
            rc = open(name, attrib | O_CREAT, S_IWRITE | S_IREAD);
        else
            rc = open(name, attrib);

        if (rc < 0)
            *succeededP = FALSE;
        else {
            fileP->fd = rc;
            *succeededP = TRUE;
        }
        if (!*succeededP)
            free(fileP);
    }
    *filePP = fileP;
}



bool
FileOpen(TFile **     const filePP,
         const char * const name,
         uint32_t     const attrib) {

    bool succeeded;

    createFileImage(filePP, name, attrib, FALSE, &succeeded);

    return succeeded;
}



bool
FileOpenCreate(TFile **     const filePP,
               const char * const name,
               uint32_t     const attrib) {

    bool succeeded;

    createFileImage(filePP, name, attrib, TRUE, &succeeded);

    return succeeded;
}



bool
FileWrite(const TFile * const fileP,
          const void *  const buffer,
          uint32_t      const len) {

    readwriterc_t rc;

    rc = write(fileP->fd, buffer, len);

    return (rc > 0 && (uint32_t)rc == len);
}



int32_t
FileRead(const TFile * const fileP,
         void *        const buffer,
         uint32_t      const len) {

    return read(fileP->fd, buffer, len);
}



bool
FileSeek(const TFile * const fileP,
         uint64_t      const pos,
         uint32_t      const attrib) {

    int64_t rc;
#if MSVCRT
    rc =  _lseeki64(fileP->fd, pos, attrib);
#else
    rc =  lseek(fileP->fd, pos, attrib);
#endif
    return (rc >= 0);
}



uint64_t
FileSize(const TFile * const fileP) {

#if MSVCRT
    return (_filelength(fileP->fd));
#else
    struct stat fs;

    fstat(fileP->fd, &fs);
    return (fs.st_size);
#endif  
}



bool
FileClose(TFile * const fileP) {

    int rc;

    rc = close(fileP->fd);

    if (rc >= 0)
        free(fileP);

    return (rc >= 0);
}



bool
FileStat(const char * const filename,
         TFileStat *  const filestat) {

    int rc;

#if MSVCRT
    rc = _stati64(filename,filestat);
#else
    rc = stat(filename,filestat);
#endif
    return (rc >= 0);
}



static void
fileFindFirstWin(TFileFind *  const filefindP ATTR_UNUSED,
                 const char * const path,
                 TFileInfo *  const fileinfo  ATTR_UNUSED,
                 bool *       const retP      ATTR_UNUSED) {
    const char * search;

    xmlrpc_asprintf(&search, "%s\\*", path);

#if MSVCRT
    filefindP->handle = _findfirsti64(search, fileinfo);
    *retP = filefindP->handle != -1;
#else
#ifdef WIN32
    filefindP->handle = FindFirstFile(search, &fileinfo->data);
    *retP = filefindP->handle != INVALID_HANDLE_VALUE;
    if (*retP) {
        LARGE_INTEGER li;
        li.LowPart = fileinfo->data.nFileSizeLow;
        li.HighPart = fileinfo->data.nFileSizeHigh;
        strcpy( fileinfo->name, fileinfo->data.cFileName );
        fileinfo->attrib = fileinfo->data.dwFileAttributes;
        fileinfo->size   = li.QuadPart;
        fileinfo->time_write = fileinfo->data.ftLastWriteTime.dwLowDateTime;
    }
#endif
#endif
    xmlrpc_strfree(search);
}



static void
fileFindFirstPosix(TFileFind *  const filefindP,
                   const char * const path,
                   TFileInfo *  const fileinfo,
                   bool *       const retP) {
    
#if !MSVCRT
    strncpy(filefindP->path, path, NAME_MAX);
    filefindP->path[NAME_MAX] = '\0';
    filefindP->handle = opendir(path);
    if (filefindP->handle)
        *retP = FileFindNext(filefindP, fileinfo);
    else
        *retP = FALSE;
#endif
}
    


bool
FileFindFirst(TFileFind ** const filefindPP,
              const char * const path,
              TFileInfo *  const fileinfo) {

    bool succeeded;

    TFileFind * filefindP;

    MALLOCVAR(filefindP);

    if (filefindP == NULL)
        succeeded = FALSE;
    else {
        if (win32)
            fileFindFirstWin(filefindP, path, fileinfo, &succeeded);
        else
            fileFindFirstPosix(filefindP, path, fileinfo, &succeeded);
        if (!succeeded)
            free(filefindP);
    }
    *filefindPP = filefindP;

    return succeeded;
}



static void
fileFindNextWin(TFileFind * const filefindP ATTR_UNUSED,
                TFileInfo * const fileinfo  ATTR_UNUSED,
                bool *      const retvalP   ATTR_UNUSED) {

#if MSVCRT
    *retvalP = _findnexti64(filefindP->handle, fileinfo) != -1;
#else
#ifdef WIN32
    bool found;
    found = FindNextFile(filefindP->handle, &fileinfo->data);
    if (found) {
        LARGE_INTEGER li;
        li.LowPart = fileinfo->data.nFileSizeLow;
        li.HighPart = fileinfo->data.nFileSizeHigh;
        strcpy(fileinfo->name, fileinfo->data.cFileName);
        fileinfo->attrib = fileinfo->data.dwFileAttributes;
        fileinfo->size   = li.QuadPart;
        fileinfo->time_write = fileinfo->data.ftLastWriteTime.dwLowDateTime;
    }
    *retvalP = found;
#endif
#endif
}



static void
fileFindNextPosix(TFileFind * const filefindP,
                  TFileInfo * const fileinfoP,
                  bool *      const retvalP) {

#ifndef WIN32
    struct dirent * deP;

    deP = readdir(filefindP->handle);
    if (deP) {
        char z[NAME_MAX+1];
        struct stat fs;

        strcpy(fileinfoP->name, deP->d_name);
        strcpy(z, filefindP->path);
        strncat(z, "/",NAME_MAX);
        strncat(z, fileinfoP->name, NAME_MAX);
        z[NAME_MAX] = '\0';
        
        stat(z, &fs);

        if (fs.st_mode & S_IFDIR)
            fileinfoP->attrib = A_SUBDIR;
        else
            fileinfoP->attrib = 0;

        fileinfoP->size       = fs.st_size;
        fileinfoP->time_write = fs.st_mtime;
        
        *retvalP = TRUE;
    } else
        *retvalP = FALSE;
#endif
}



bool
FileFindNext(TFileFind * const filefindP,
             TFileInfo * const fileinfo) {

    bool retval;

    if (win32)
        fileFindNextWin(filefindP, fileinfo, &retval);
    else
        fileFindNextPosix(filefindP, fileinfo, &retval);

    return retval;
}



void
FileFindClose(TFileFind * const filefindP) {
#ifdef WIN32
#if MSVCRT
    _findclose(filefindP->handle);
#else
   FindClose(filefindP->handle);
#endif
#else
    closedir(filefindP->handle);
#endif
    free(filefindP);
}
