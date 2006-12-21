/******************************************************************************
**
** conf.c
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if defined(ABYSS_WIN32) && !defined(__BORLANDC__)
#include <direct.h>
#endif

#ifdef _UNIX
#include <pwd.h>
#endif

#include "xmlrpc-c/abyss.h"

/*********************************************************************
** Configuration Files Parsing Functions
*********************************************************************/



static abyss_bool
ConfReadLine(TFile *f,char *buffer,uint32_t len) {
    abyss_bool r=TRUE;
    char c,*p,*z=buffer;

    while ((--len)>0)
    {
        if (FileRead(f,buffer,1)<1)
        {
            if (z==buffer)
                r=FALSE;
            break;
        };

        if ((*buffer==CR) || (*buffer==LF) )
            break;

        buffer++;
    };

    if (len==0)
        while (FileRead(f,&c,1)==1)
            if ((c==CR) || (c==LF))
                break;

    *buffer='\0';

    /* Discard comments */
    p=strchr(z,'#');
    if (p)
        *p='\0';

    return r;
}

static abyss_bool
ConfNextToken(char **p) {
    while (1)
        switch (**p)
        {
        case '\t':
        case ' ':
            (*p)++;
            break;
        case '\0':
            return FALSE;
        default:
            return TRUE;
        };
}

static char *
ConfGetToken(char **p) {
    char *p0=*p;

    while (1)
        switch (**p)
        {
        case '\t':
        case ' ':
        case CR:
        case LF:
        case '\0':
            if (p0==*p)
                return NULL;

            if (**p)
            {
                **p='\0';
                (*p)++;
            };
            return p0;

        default:
            (*p)++;
        };
}

static abyss_bool
ConfReadInt(char *p,int32_t *n,int32_t min,int32_t max) {
    char *e;

    *n=strtol(p,&e,10);

    if (min!=max)
        return ((e!=p) && (*n>=min) && (*n<=max));
    else
        return (e!=p);
}

static abyss_bool
ConfReadBool(char *p, abyss_bool *b) {
    if (strcasecmp(p,"yes")==0)
    {
        *b=TRUE;
        return TRUE;
    };

    if (strcasecmp(p,"no")==0)
    {
        *b=FALSE;
        return TRUE;
    };

    return FALSE;
}

/*********************************************************************
** MIME Types File
*********************************************************************/

abyss_bool ConfReadMIMETypes(char *filename)
{
    TFile f;
    char z[512],*p;
    char *mimetype,*ext;

    if (!FileOpen(&f,filename,O_RDONLY))
        return FALSE;

    while (ConfReadLine(&f,z,512))
    {
        p=z;

        if (ConfNextToken(&p)) {
            mimetype=ConfGetToken(&p);
            if (mimetype) {
                while (ConfNextToken(&p)) {
                    ext=ConfGetToken(&p);
                    if (ext)
                        MIMETypeAdd(mimetype,ext);
                    else
                        break;
                }
            }
        }
    };

    FileClose(&f);
    return TRUE;
}

/*********************************************************************
** Server Configuration File
*********************************************************************/

abyss_bool ConfReadServerFile(const char *filename,TServer *srv)
{
    TFile f;
    char z[512],*p;
    char *option;
    int32_t n,line=0;
    TFileStat fs;

    if (!FileOpen(&f,filename,O_RDONLY))
        return FALSE;

    while (ConfReadLine(&f,z,512))
    {
        line++;
        p=z;

        if (ConfNextToken(&p)) {
            option=ConfGetToken(&p);
            if (option)
            {
                ConfNextToken(&p);

                if (strcasecmp(option,"port")==0)
                {
                    if (ConfReadInt(p,&n,1,65535))
                        srv->port=n;
                    else
                        TraceExit("Invalid port '%s'",p);
                }
                else if (strcasecmp(option,"serverroot")==0)
                {
#if defined( ABYSS_WIN32 ) && !defined( __BORLANDC__ )
                    if (_chdir(p))
#else
                    if (chdir(p))
#endif
                        TraceExit("Invalid server root '%s'",p);
                }
                else if (strcasecmp(option,"path")==0)
                {
                    if (FileStat(p,&fs))
                        if (fs.st_mode & S_IFDIR)
                        {
                            free(srv->filespath);
                            srv->filespath=strdup(p);
                            continue;
                        };
                    
                    TraceExit("Invalid path '%s'",p);
                }
                else if (strcasecmp(option,"default")==0)
                {
                    char *filename;
                    
                    while ((filename=ConfGetToken(&p)))
                    {
                        ListAdd(&srv->defaultfilenames,strdup(filename));
                        if (!ConfNextToken(&p))
                            break;
                    };
                }
                else if (strcasecmp(option,"keepalive")==0)
                {
                    if (ConfReadInt(p,&n,1,65535))
                        srv->keepalivemaxconn=n;
                    else
                        TraceExit("Invalid KeepAlive value '%s'",p);
                }
                else if (strcasecmp(option,"timeout")==0)
                {
                    if (ConfReadInt(p,&n,1,3600))
                    {
                        srv->keepalivetimeout=n;
                        /* Must see what to do with that */
                        srv->timeout=n;
                    }
                    else
                        TraceExit("Invalid TimeOut value '%s'",p);
                }
                else if (strcasecmp(option,"mimetypes")==0)
                {
                    if (!ConfReadMIMETypes(p))
                        TraceExit("Can't read MIME Types file '%s'",p);
                }
                else if (strcasecmp(option,"logfile")==0)
                {
                    LogOpen(srv,p);
                }
                else if (strcasecmp(option,"user")==0)
                {
#ifdef _UNIX
                    if (*p=='#')
                    {
                        int32_t n;

                        if (!ConfReadInt(p+1,&n,0,0))
                            TraceExit("Bad user number '%s'",p);
                        else
                            srv->uid=n;
                    }
                    else
                    {
                        struct passwd *pwd;

                        if (!(pwd=getpwnam(p)))
                            TraceExit("Unknown user '%s'",p);
        
                        srv->uid=pwd->pw_uid;
                        if ((int)srv->gid==(-1))
                            srv->gid=pwd->pw_gid;
                    };
#else
                    TraceMsg("User option ignored");
#endif  /* _UNIX */ 
                }
                else if (strcasecmp(option,"pidfile")==0)
                {
#ifdef _UNIX
                    if (!FileOpenCreate(&srv->pidfile,p,O_TRUNC | O_WRONLY))
                    {
                        srv->pidfile=-1;
                        TraceMsg("Bad PidFile value '%s'",p);
                    };
#else
                    TraceMsg("PidFile option ignored");
#endif  /* _UNIX */ 
                }
                else if (strcasecmp(option,"advertiseserver")==0)
                {
                    if (!ConfReadBool(p,&srv->advertise))
                        TraceExit("Invalid boolean value "
                                  "for AdvertiseServer option");
                }
                else
                    TraceExit("Invalid option '%s' at line %d",option,line);
            };
        }
    };

    FileClose(&f);
    return TRUE;
}
