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

#include <string.h>

#ifdef ABYSS_WIN32
#include <io.h>
#else
/* Must check this
#include <sys/io.h>
*/
#endif  /* ABYSS_WIN32 */

#ifndef ABYSS_WIN32
#include <dirent.h>
#endif  /* ABYSS_WIN32 */

#include "xmlrpc-c/abyss.h"

/*********************************************************************
** File
*********************************************************************/

abyss_bool FileOpen(TFile *f, const char *name,uint32_t attrib)
{
#if defined( ABYSS_WIN32 ) && !defined( __BORLANDC__ )
    return ((*f=_open(name,attrib))!=(-1));
#else
    return ((*f=open(name,attrib))!=(-1));
#endif
}

abyss_bool FileOpenCreate(TFile *f, const char *name, uint32_t attrib)
{
#if defined( ABYSS_WIN32 ) && !defined( __BORLANDC__ )
    return ((*f=_open(name,attrib | O_CREAT,_S_IWRITE | _S_IREAD))!=(-1));
#else
    return ((*f=open(name,attrib | O_CREAT,S_IWRITE | S_IREAD))!=(-1));
#endif
}

abyss_bool FileWrite(TFile *f, void *buffer, uint32_t len)
{
#if defined( ABYSS_WIN32 ) && !defined( __BORLANDC__ )
    return (_write(*f,buffer,len)==(int32_t)len);
#else
    return (write(*f,buffer,len)==(int32_t)len);
#endif
}

int32_t FileRead(TFile *f, void *buffer, uint32_t len)
{
#if defined( ABYSS_WIN32 ) && !defined( __BORLANDC__ )
    return (_read(*f,buffer,len));
#else
    return (read(*f,buffer,len));
#endif
}

abyss_bool FileSeek(TFile *f, uint64_t pos, uint32_t attrib)
{
#if defined( ABYSS_WIN32 ) && !defined( __BORLANDC__ )
    return (_lseek(*f,pos,attrib)!=(-1));
#else
    return (lseek(*f,pos,attrib)!=(-1));
#endif
}

uint64_t FileSize(TFile *f)
{
#if defined( ABYSS_WIN32 ) && !defined( __BORLANDC__ )
    return (_filelength(*f));
#else
    struct stat fs;

    fstat(*f,&fs);
    return (fs.st_size);
#endif  
}

abyss_bool FileClose(TFile *f)
{
#if defined( ABYSS_WIN32 ) && !defined( __BORLANDC__ )
    return (_close(*f)!=(-1));
#else
    return (close(*f)!=(-1));
#endif
}

abyss_bool FileStat(char *filename,TFileStat *filestat)
{
#if defined( ABYSS_WIN32 ) && !defined( __BORLANDC__ )
    return (_stati64(filename,filestat)!=(-1));
#else
    return (stat(filename,filestat)!=(-1));
#endif
}

abyss_bool FileFindFirst(TFileFind *filefind,char *path,TFileInfo *fileinfo)
{
#ifdef ABYSS_WIN32
    abyss_bool ret;
    char *p=path+strlen(path);

    *p='\\';
    *(p+1)='*';
    *(p+2)='\0';
#ifndef __BORLANDC__
    ret=(((*filefind)=_findfirst(path,fileinfo))!=(-1));
#else
    *filefind = FindFirstFile( path, &fileinfo->data );
   ret = *filefind != NULL;
   if( ret )
   {
      LARGE_INTEGER li;
      li.LowPart = fileinfo->data.nFileSizeLow;
      li.HighPart = fileinfo->data.nFileSizeHigh;
      strcpy( fileinfo->name, fileinfo->data.cFileName );
       fileinfo->attrib = fileinfo->data.dwFileAttributes;
       fileinfo->size   = li.QuadPart;
      fileinfo->time_write = fileinfo->data.ftLastWriteTime.dwLowDateTime;
   }
#endif
    *p='\0';
    return ret;
#else
    strncpy(filefind->path,path,NAME_MAX);
    filefind->path[NAME_MAX]='\0';
    filefind->handle=opendir(path);
    if (filefind->handle)
        return FileFindNext(filefind,fileinfo);

    return FALSE;
#endif
}

abyss_bool FileFindNext(TFileFind *filefind,TFileInfo *fileinfo)
{
#ifdef ABYSS_WIN32

#ifndef __BORLANDC__
    return (_findnext(*filefind,fileinfo)!=(-1));
#else
   abyss_bool ret = FindNextFile( *filefind, &fileinfo->data );
   if( ret )
   {
      LARGE_INTEGER li;
      li.LowPart = fileinfo->data.nFileSizeLow;
      li.HighPart = fileinfo->data.nFileSizeHigh;
      strcpy( fileinfo->name, fileinfo->data.cFileName );
       fileinfo->attrib = fileinfo->data.dwFileAttributes;
       fileinfo->size   = li.QuadPart;
      fileinfo->time_write = fileinfo->data.ftLastWriteTime.dwLowDateTime;
   }
    return ret;
#endif

#else
    struct dirent *de;
    /****** Must be changed ***/
    char z[NAME_MAX+1];

    de=readdir(filefind->handle);
    if (de)
    {
        struct stat fs;

        strcpy(fileinfo->name,de->d_name);
        strcpy(z,filefind->path);
        strncat(z,"/",NAME_MAX);
        strncat(z,fileinfo->name,NAME_MAX);
        z[NAME_MAX]='\0';
        
        stat(z,&fs);

        if (fs.st_mode & S_IFDIR)
            fileinfo->attrib=A_SUBDIR;
        else
            fileinfo->attrib=0;

        fileinfo->size=fs.st_size;
        fileinfo->time_write=fs.st_mtime;
        
        return TRUE;
    };

    return FALSE;
#endif
}

void FileFindClose(TFileFind *filefind)
{
#ifdef ABYSS_WIN32

#ifndef __BORLANDC__
    _findclose(*filefind);
#else
   FindClose( *filefind );
#endif

#else
    closedir(filefind->handle);
#endif
}
