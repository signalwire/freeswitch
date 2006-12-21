/******************************************************************************
**
** main.c
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

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>

#ifdef ABYSS_WIN32
#include <io.h>
#else
/* Must check this
#include <sys/io.h>
*/
#endif  /* ABYSS_WIN32 */

#ifdef _UNIX
#include <sys/signal.h>
#include <sys/wait.h>
#endif

#include "xmlrpc-c/abyss.h"

void Answer(TSession *r, uint16_t statuscode, char *buffer)
{
    ResponseChunked(r);

    ResponseStatus(r,statuscode);

    ResponseContentType(r,"text/html");

    ResponseWrite(r);
    
    HTTPWrite(r,"<HTML><BODY>",12);
    
    HTTPWrite(r,buffer,strlen(buffer));

    HTTPWrite(r,"</BODY></HTML>",14);

    HTTPWriteEnd(r);
}

abyss_bool HandleTime(TSession *r)
{
    char z[50];
    time_t ltime;
    TDate date;

    if (strcmp(r->uri,"/time")!=0)
        return FALSE;

    if (!RequestAuth(r,"Mot de passe","moez","hello"))
        return TRUE;

    time( &ltime );
    DateFromGMT(&date,ltime);
    

    strcpy(z,"The time is ");
    DateToString(&date,z+strlen(z));

    Answer(r,200,z);

    return TRUE;
}

abyss_bool HandleDump(TSession *r)
{
    char z[50];

    if (strcmp(r->uri,"/name")!=0)
        return FALSE;

    sprintf(z,"Server name is %s", (r->server)->name );
    Answer(r,200,z);

    return TRUE;
}

abyss_bool HandleStatus(TSession *r)
{
    uint32_t status;

    if (sscanf(r->uri,"/status/%d",&status)<=0)
        return FALSE;

    ResponseStatus(r,(uint16_t)status);

    return TRUE;
}

abyss_bool HandleMIMEType(TSession *r)
{
    char *m;

    if (strncmp(r->uri,"/mime/",6)!=0)
        return FALSE;

    m=MIMETypeFromExt(r->uri+6);
    if (!m)
        m="(none)";

    Answer(r,200,m);

    return TRUE;
}

#ifdef _UNIX
static void sigterm(int sig)
{
    TraceExit("Signal %d received. Exiting...\n",sig);
}

static void sigchld(int sig)
{
    pid_t pid;
    int status;

    /* Reap defunct children until there aren't any more. */
    for (;;)
    {
        pid = waitpid( (pid_t) -1, &status, WNOHANG );

        /* none left */
        if (pid==0)
            break;

        if (pid<0)
        {
            /* because of ptrace */
            if (errno==EINTR)   
                continue;

            break;
        }
    }
}
#endif _UNIX

void copyright()
{
    printf("ABYSS Web Server version "SERVER_VERSION"\n(C) Moez Mahfoudh - 2000\n\n");
}

void help(char *name)
{
    copyright();
    printf("Usage: %s [-h] [-c configuration file]\n\n",name);
}

int main(int argc,char **argv)
{
    TServer srv;
    char *p,*conffile=DEFAULT_CONF_FILE;
    abyss_bool err=FALSE;
    char *name=argv[0];

    while (p=*(++argv))
    {
        if ((*p=='-') && (*(p+1)))
            if (*(p+2)=='\0')
                switch (*(p+1))
                {
                case 'c':
                    argv++;
                    if (*argv)
                        conffile=*argv;
                    else
                        err=TRUE;
                    break;
                case 'h':
                    help(name);
                    exit(0);
                default:
                    err=TRUE;
                }
            else
                err=TRUE;
        else
            err=TRUE;
    };

    if (err)
    {
        help(name);
        exit(1);
    };

#ifdef ABYSS_WIN32
    copyright();
    printf("\nPress Ctrl+C to stop the server\n");
#endif

    DateInit();

    MIMETypeInit();

    ServerCreate(&srv,"HTTPServer",80,DEFAULT_DOCS,NULL);

    ConfReadServerFile(conffile,&srv);

    ServerAddHandler(&srv,HandleTime);
    ServerAddHandler(&srv,HandleDump);
    ServerAddHandler(&srv,HandleStatus);
    ServerAddHandler(&srv,HandleMIMEType);

    ServerInit(&srv);

#ifdef _UNIX
    /* Catch various termination signals. */
    signal(SIGTERM,sigterm);
    signal(SIGINT,sigterm);
    signal(SIGHUP,sigterm);
    signal(SIGUSR1,sigterm);

    /* Catch defunct children. */
    signal(SIGCHLD,sigchld);
    /* Become a daemon */
    switch (fork())
    {
    case 0:
        break;
    case -1:
        TraceExit("Unable to become a daemon");
    default:
        exit(0);
    };

#if !defined( _NO_USERS ) && !defined( __CYGWIN32__ )
    setsid();
    /* Change the current user if we are root */
    if (getuid()==0)
    {
        if (srv.uid==(-1))
            TraceExit("Can't run under root privileges. Please add a User option in your configuration file.");

        if (setgroups(0,NULL)==(-1))
            TraceExit("Failed to setup the group.");

        if (srv.gid!=(-1))
            if (setgid(srv.gid)==(-1))
                TraceExit("Failed to change the group.");
        
        if (setuid(srv.uid)==(-1))
            TraceExit("Failed to change the user.");
    };
#endif

    if (srv.pidfile!=(-1))
    {
        char z[16];

        sprintf(z,"%d",getpid());
        FileWrite(&srv.pidfile,z,strlen(z));
        FileClose(&srv.pidfile);
    };

#endif

    ServerRun(&srv);

    return 0;
}
