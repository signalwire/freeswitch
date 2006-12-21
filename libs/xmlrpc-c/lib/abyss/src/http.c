/*******************************************************************************
**
** http.c
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
*******************************************************************************/

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include "xmlrpc_config.h"
#include "xmlrpc-c/abyss.h"
#include "token.h"

/*********************************************************************
** Request Parser
*********************************************************************/

/*********************************************************************
** Request
*********************************************************************/

void RequestInit(TSession *r,TConn *c)
{
    time_t t;

    time(&t);
    r->date=*gmtime(&t);

    r->keepalive=r->cankeepalive=FALSE;
    r->query=NULL;
    r->host=NULL;
    r->from=NULL;
    r->useragent=NULL;
    r->referer=NULL;
    r->user=NULL;
    r->port=80;
    r->versionmajor=0;
    r->versionminor=9;
    r->server=c->server;
    r->conn=c;

    r->done=FALSE;

    r->chunkedwrite=r->chunkedwritemode=FALSE;

    r->requestline=NULL;

    ListInit(&r->cookies);
    ListInit(&r->ranges);
    TableInit(&r->request_headers);
    TableInit(&r->response_headers);

    r->status=0;

    StringAlloc(&(r->header));
}

void RequestFree(TSession *r)
{
    if (r->requestline)
        free(r->requestline);

    if (r->user)
        free(r->user);

    ListFree(&r->cookies);
    ListFree(&r->ranges);
    TableFree(&r->request_headers);
    TableFree(&r->response_headers);
    StringFree(&(r->header));
}



static void
readFirstLineOfRequest(TSession *   const r,
                       char **      const lineP,
                       abyss_bool * const errorP) {

    *errorP = FALSE;

    /* Ignore CRLFs in the beginning of the request (RFC2068-P30) */
    do {
        abyss_bool success;
        success = ConnReadLine(r->conn, lineP, r->server->timeout);
        if (!success) {
            /* Request Timeout */
            ResponseStatus(r, 408);
            *errorP = TRUE;
        }
    } while ((*lineP)[0] == '\0' && !*errorP);
}



static void
processFirstLineOfRequest(TSession *   const r,
                          char *       const line1,
                          abyss_bool * const moreLinesP,
                          abyss_bool * const errorP) {
    
    char * p;
    char * t;

    p = line1;

    /* Jump over spaces */
    NextToken(&p);

    r->requestline = strdup(p);
    
    t = GetToken(&p);
    if (!t) {
        /* Bad request */
        ResponseStatus(r,400);
        *errorP = TRUE;
    } else {
        if (strcmp(t, "GET") == 0)
            r->method = m_get;
        else if (strcmp(p, "PUT") == 0)
            r->method = m_put;
        else if (strcmp(t, "OPTIONS") == 0)
            r->method = m_options;
        else if (strcmp(p, "DELETE") == 0)
            r->method = m_delete;
        else if (strcmp(t, "POST") == 0)
            r->method = m_post;
        else if (strcmp(p, "TRACE") == 0)
            r->method = m_trace;
        else if (strcmp(t, "HEAD") == 0)
            r->method = m_head;
        else
            r->method = m_unknown;
        
        /* URI and Query Decoding */
        NextToken(&p);
        
        t=GetToken(&p);
        if (!t)
            *errorP = TRUE;
        else {
            r->uri=t;
        
            RequestUnescapeURI(r);
        
            t=strchr(t,'?');
            if (t) {
                *t = '\0';
                r->query = t+1;
            }
        
            NextToken(&p);
        
            /* HTTP Version Decoding */
        
            t = GetToken(&p);
            if (t) {
                uint32_t vmin, vmaj;
                if (sscanf(t, "HTTP/%d.%d", &vmaj, &vmin) != 2) {
                    /* Bad request */
                    ResponseStatus(r, 400);
                    *errorP = TRUE;
                } else {
                    r->versionmajor = vmaj;
                    r->versionminor = vmin;
                    *errorP = FALSE;
                }
                *moreLinesP = TRUE;
            } else {
                /* There is not HTTP version, so this is a single
                   line request.
                */
                *errorP = FALSE;
                *moreLinesP = FALSE;
            }
        }
    }
}



abyss_bool
RequestRead(TSession * const r) {
    char *n,*t,*p;
    abyss_bool ret;
    abyss_bool error;
    char * line1;
    abyss_bool moreLines;

    readFirstLineOfRequest(r, &line1, &error);
    if (error)
        return FALSE;
    
    processFirstLineOfRequest(r, line1, &moreLines, &error);
    if (error)
        return FALSE;
    if (!moreLines)
        return TRUE;

    /* Headers decoding */
    ret = TRUE;

    for (;;)
    {
        if (!ConnReadLine(r->conn,&p,r->server->timeout))
        {
            /* Request Timeout */
            ResponseStatus(r,408);
            return FALSE;
        };

        /* We have reached the empty line so all the request was read */
        if (!*p)
            return TRUE;

        NextToken(&p);
        
        if (!(n=GetToken(&p)))
        {
            /* Bad Request */
            ResponseStatus(r,400);
            return FALSE;
        };

        /* Valid Field name ? */
        if (n[strlen(n)-1]!=':')
        {
            /* Bad Request */
            ResponseStatus(r,400);
            return FALSE;
        };

        n[strlen(n)-1]='\0';

        NextToken(&p);

        t=n;
        while (*t)
        {
            *t=tolower(*t);
            t++;
        };

        t=p;

        TableAdd(&r->request_headers,n,t);

        if (strcmp(n,"connection")==0)
        {
            /* must handle the jigsaw TE,keepalive */
            if (strcasecmp(t,"keep-alive")==0)
                r->keepalive=TRUE;
            else
                r->keepalive=FALSE;
        }
        else if (strcmp(n,"host")==0)
            r->host=t;
        else if (strcmp(n,"from")==0)
            r->from=t;
        else if (strcmp(n,"user-agent")==0)
            r->useragent=t;
        else if (strcmp(n,"referer")==0)
            r->referer=t;
        else if (strcmp(n,"range")==0)
        {
            if (strncmp(t,"bytes=",6)==0)
                if (!ListAddFromString(&(r->ranges),t+6))
                {
                    /* Bad Request */
                    ResponseStatus(r,400);
                    return FALSE;
                };
        }
        else if (strcmp(n,"cookies")==0)
        {
            if (!ListAddFromString(&(r->cookies),t))
            {
                /* Bad Request */
                ResponseStatus(r,400);
                return FALSE;
            };
        };
    };
}

char *RequestHeaderValue(TSession *r,char *name)
{
    return (TableFind(&r->request_headers,name));
}

abyss_bool RequestUnescapeURI(TSession *r)
{
    char *x,*y,c,d;

    x=y=r->uri;

    while (1)
        switch (*x)
        {
        case '\0':
            *y='\0';
            return TRUE;

        case '%':
            x++;
            c=tolower(*x++);
            if ((c>='0') && (c<='9'))
                c-='0';
            else if ((c>='a') && (c<='f'))
                c-='a'-10;
            else
                return FALSE;

            d=tolower(*x++);
            if ((d>='0') && (d<='9'))
                d-='0';
            else if ((d>='a') && (d<='f'))
                d-='a'-10;
            else
                return FALSE;

            *y++=((c << 4) | d);
            break;

        default:
            *y++=*x++;
            break;
        };
};



abyss_bool
RequestValidURI(TSession *r) {

    char *p;

    if (!r->uri)
        return FALSE;

    if (*(r->uri)!='/') {
        if (strncmp(r->uri,"http://",7)!=0)
            return FALSE;
        else {
            r->uri+=7;
            r->host=r->uri;
            p=strchr(r->uri,'/');

            if (!p) {
                r->uri="*";
                return TRUE;
            };

            r->uri=p;
            p=r->host;

            while (*p!='/') {
                *(p-1)=*p;
                p++;
            };

            --p;
            *p='\0';

            --r->host;
        };
    }

    /* Host and Port Decoding */
    if (r->host) {
        p=strchr(r->host,':');
        if (p) {
            uint32_t port=0;

            *p='\0';
            p++;
            while (isdigit(*p) && (port<65535)) {
                port=port*10+(*p)-'0';
                ++p;
            };
            
            r->port=port;

            if (*p || port==0)
                return FALSE;
        };
    }
    if (strcmp(r->uri,"*")==0)
        return (r->method!=m_options);

    if (strchr(r->uri,'*'))
        return FALSE;

    return TRUE;
}


abyss_bool RequestValidURIPath(TSession *r)
{
    uint32_t i=0;
    char *p=r->uri;

    if (*p=='/')
    {
        i=1;
        while (*p)
            if (*(p++)=='/')
            {
                if (*p=='/')
                    break;
                else if ((strncmp(p,"./",2)==0) || (strcmp(p,".")==0))
                    p++;
                else if ((strncmp(p,"../",2)==0) || (strcmp(p,"..")==0))
                {
                    p+=2;
                    i--;
                    if (i==0)
                        break;
                }
                /* Prevent accessing hidden files (starting with .) */
                else if (*p=='.')
                    return FALSE;
                else
                    if (*p)
                        i++;
            };
    };

    return ((*p==0) && (i>0));
}



abyss_bool
RequestAuth(TSession *r,char *credential,char *user,char *pass) {

    char *p,*x;
    char z[80],t[80];

    p=RequestHeaderValue(r,"authorization");
    if (p) {
        NextToken(&p);
        x=GetToken(&p);
        if (x) {
            if (strcasecmp(x,"basic")==0) {
                NextToken(&p);
                sprintf(z,"%s:%s",user,pass);
                Base64Encode(z,t);

                if (strcmp(p,t)==0) {
                    r->user=strdup(user);
                    return TRUE;
                };
            };
        }
    };

    sprintf(z,"Basic realm=\"%s\"",credential);
    ResponseAddField(r,"WWW-Authenticate",z);
    ResponseStatus(r,401);
    return FALSE;
}



/*********************************************************************
** Range
*********************************************************************/

abyss_bool RangeDecode(char *str,uint64_t filesize,uint64_t *start,uint64_t *end)
{
    char *ss;

    *start=0;
    *end=filesize-1;

    if (*str=='-')
    {
        *start=filesize-strtol(str+1,&ss,10);
        return ((ss!=str) && (!*ss));
    };

    *start=strtol(str,&ss,10);

    if ((ss==str) || (*ss!='-'))
        return FALSE;

    str=ss+1;

    if (!*str)
        return TRUE;

    *end=strtol(str,&ss,10);

    if ((ss==str) || (*ss) || (*end<*start))
        return FALSE;

    return TRUE;
}

/*********************************************************************
** HTTP
*********************************************************************/

char *HTTPReasonByStatus(uint16_t code)
{
    static struct _HTTPReasons {
        uint16_t status;
        char *reason;
    } *r,reasons[] = 
    {
        { 100,"Continue" }, 
        { 101,"Switching Protocols" }, 
        { 200,"OK" }, 
        { 201,"Created" }, 
        { 202,"Accepted" }, 
        { 203,"Non-Authoritative Information" }, 
        { 204,"No Content" }, 
        { 205,"Reset Content" }, 
        { 206,"Partial Content" }, 
        { 300,"Multiple Choices" }, 
        { 301,"Moved Permanently" }, 
        { 302,"Moved Temporarily" }, 
        { 303,"See Other" }, 
        { 304,"Not Modified" }, 
        { 305,"Use Proxy" }, 
        { 400,"Bad Request" }, 
        { 401,"Unauthorized" }, 
        { 402,"Payment Required" }, 
        { 403,"Forbidden" }, 
        { 404,"Not Found" }, 
        { 405,"Method Not Allowed" }, 
        { 406,"Not Acceptable" }, 
        { 407,"Proxy Authentication Required" }, 
        { 408,"Request Timeout" }, 
        { 409,"Conflict" }, 
        { 410,"Gone" }, 
        { 411,"Length Required" }, 
        { 412,"Precondition Failed" }, 
        { 413,"Request Entity Too Large" }, 
        { 414,"Request-URI Too Long" }, 
        { 415,"Unsupported Media Type" }, 
        { 500,"Internal Server Error" }, 
        { 501,"Not Implemented" }, 
        { 502,"Bad Gateway" }, 
        { 503,"Service Unavailable" }, 
        { 504,"Gateway Timeout" }, 
        { 505,"HTTP Version Not Supported" },
        { 000, NULL }
    };

    r=reasons;

    while (r->status<=code)
        if (r->status==code)
            return (r->reason);
        else
            r++;

    return "No Reason";
}



int32_t
HTTPRead(TSession * const s ATTR_UNUSED,
         char *     const buffer ATTR_UNUSED,
         uint32_t   const len ATTR_UNUSED) {

    return 0;
}



abyss_bool HTTPWrite(TSession *s,char *buffer,uint32_t len)
{
    if (s->chunkedwrite && s->chunkedwritemode)
    {
        char t[16];

        if (ConnWrite(s->conn,t,sprintf(t,"%x"CRLF,len)))
            if (ConnWrite(s->conn,buffer,len))
                return ConnWrite(s->conn,CRLF,2);

        return FALSE;
    }
    
    return ConnWrite(s->conn,buffer,len);
}

abyss_bool HTTPWriteEnd(TSession *s)
{
    if (!s->chunkedwritemode)
        return TRUE;

    if (s->chunkedwrite)
    {
        /* May be one day trailer dumping will be added */
        s->chunkedwritemode=FALSE;
        return ConnWrite(s->conn,"0"CRLF CRLF,5);
    }

    s->keepalive=FALSE;
    return TRUE;
}

/*********************************************************************
** Response
*********************************************************************/

void ResponseError(TSession *r)
{
    char *reason=HTTPReasonByStatus(r->status);
    char z[500];

    ResponseAddField(r,"Content-type","text/html");

    ResponseWrite(r);
    
    sprintf(z,"<HTML><HEAD><TITLE>Error %d</TITLE></HEAD>"
            "<BODY><H1>Error %d</H1><P>%s</P>" SERVER_HTML_INFO 
            "</BODY></HTML>",
            r->status,r->status,reason);

    ConnWrite(r->conn,z,strlen(z)); 
}

abyss_bool ResponseChunked(TSession *r)
{
    /* This is only a hope, things will be real only after a call of
       ResponseWrite
    */
    r->chunkedwrite=(r->versionmajor>=1) && (r->versionminor>=1);
    r->chunkedwritemode=TRUE;

    return TRUE;
}

void ResponseStatus(TSession *r,uint16_t code)
{
    r->status=code;
}

void ResponseStatusErrno(TSession *r)
{
    uint16_t code;

    switch (errno)
    {
    case EACCES:
        code=403;
        break;
    case ENOENT:
        code=404;
        break;
    default:
        code=500;
    };

    ResponseStatus(r,code);
}

abyss_bool ResponseAddField(TSession *r,char *name,char *value)
{
    return TableAdd(&r->response_headers,name,value);
}

void ResponseWrite(TSession *r)
{
    abyss_bool connclose=TRUE;
    char z[64];
    TTableItem *ti;
    uint16_t i;
    char *reason;

    /* if status == 0 then this is an error */
    if (r->status==0)
        r->status=500;

    /* the request was treated */
    r->done=TRUE;

    reason=HTTPReasonByStatus(r->status);
    sprintf(z,"HTTP/1.1 %d ",r->status);
    ConnWrite(r->conn,z,strlen(z));
    ConnWrite(r->conn,reason,strlen(reason));
    ConnWrite(r->conn,CRLF,2);

    /* generation of the connection field */
    if ((r->status<400) && (r->keepalive) && (r->cankeepalive))
        connclose=FALSE;

    ResponseAddField(r,"Connection",
        (connclose?"close":"Keep-Alive"));

    if (!connclose)
    {
        sprintf(z,"timeout=%u, max=%u",r->server->keepalivetimeout
                ,r->server->keepalivemaxconn);

        ResponseAddField(r,"Keep-Alive",z);

        if (r->chunkedwrite && r->chunkedwritemode)
        {
            if (!ResponseAddField(r,"Transfer-Encoding","chunked"))
            {
                    r->chunkedwrite=FALSE;
                    r->keepalive=FALSE;
            };
        };
    }
    else
    {
        r->keepalive=FALSE;
        r->chunkedwrite=FALSE;
    };
    
    /* generation of the date field */
    if ((r->status>=200) && DateToString(&r->date,z))
        ResponseAddField(r,"Date",z);

    /* Generation of the server field */
    if (r->server->advertise)
        ResponseAddField(r,"Server",SERVER_HVERSION);

    /* send all the fields */
    for (i=0;i<r->response_headers.size;i++)
    {
        ti=&r->response_headers.item[i];
        ConnWrite(r->conn,ti->name,strlen(ti->name));
        ConnWrite(r->conn,": ",2);
        ConnWrite(r->conn,ti->value,strlen(ti->value));
        ConnWrite(r->conn,CRLF,2);
    };

    ConnWrite(r->conn,CRLF,2);  
}

abyss_bool ResponseContentType(TSession *r,char *type)
{
    return ResponseAddField(r,"Content-type",type);
}

abyss_bool ResponseContentLength(TSession *r,uint64_t len)
{
    char z[32];

    sprintf(z,"%llu",len);
    return ResponseAddField(r,"Content-length",z);
}


/*********************************************************************
** MIMEType
*********************************************************************/

TList _MIMETypes,_MIMEExt;
TPool _MIMEPool;

void MIMETypeInit()
{
    ListInit(&_MIMETypes);
    ListInit(&_MIMEExt);
    PoolCreate(&_MIMEPool,1024);
}

abyss_bool MIMETypeAdd(char *type,char *ext)
{
    uint16_t index;

    if (ListFindString(&_MIMETypes,type,&index))
        type=_MIMETypes.item[index];
    else
        if (!(type=PoolStrdup(&_MIMEPool,type)))
            return FALSE;
    
    if (ListFindString(&_MIMEExt,ext,&index))
        _MIMETypes.item[index]=type;
    else {
        ext=PoolStrdup(&_MIMEPool,ext);
        if (ext)
            return (ListAdd(&_MIMETypes,type) && ListAdd(&_MIMEExt,ext));
        else
            return FALSE;
    }            
    return TRUE;
}

char *MIMETypeFromExt(char *ext)
{
    uint16_t extindex;

    if (!ListFindString(&_MIMEExt,ext,&extindex))
        return NULL;
    else
        return _MIMETypes.item[extindex];
}

char *MIMETypeFromFileName(char *filename)
{
    char *p=filename+strlen(filename),*z=NULL;

    while ((*p!='.') && (p>=filename) && ((*p)!='/'))
        p--;

    if (*p=='.')
        z=MIMETypeFromExt(p+1);

    if (z)
        return z;
    else
        return "application/octet-stream";
}

char *MIMETypeGuessFromFile(char *filename)
{
    char *p=filename+strlen(filename),*z=NULL;
    TFile file;

    while ((*p!='.') && (p>=filename) && ((*p)!='/'))
        p--;

    if (*p=='.')
        z=MIMETypeFromExt(p+1);

    if (z)
        return z;
    
    if (FileOpen(&file,filename,O_BINARY | O_RDONLY))
    {
        uint8_t buffer[80],c;
        int32_t len,i,n=0;

        i=len=FileRead(&file,buffer,80);

        while (i>0)
        {
            i--;
            c=buffer[i];
            if ((c>=' ') || (isspace(c)) || (c==26))
                n++;
        };

        if (n==len)
            z="text/plain";

        FileClose(&file);
    };

    if (z)
        return z;
    else
        return "application/octet-stream";  
}

/*********************************************************************
** Date
*********************************************************************/

static char *_DateDay[7]=
{
    "Sun","Mon","Tue","Wed","Thu","Fri","Sat"
};

static char *_DateMonth[12]=
{
    "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"
};

static int32_t _DateTimeBias=0;
static char _DateTimeBiasStr[6]="";

abyss_bool DateToString(TDate *tm,char *s)
{
    if (mktime(tm)==(time_t)(-1))
    {
        *s='\0';
        return FALSE;
    };

    sprintf(s,"%s, %02d %s %04d %02d:%02d:%02d GMT",_DateDay[tm->tm_wday],tm->tm_mday,
                _DateMonth[tm->tm_mon],tm->tm_year+1900,tm->tm_hour,tm->tm_min,tm->tm_sec);

    return TRUE;
}

abyss_bool DateToLogString(TDate *tm,char *s)
{
    time_t t;
    TDate d;

    if ((t=mktime(tm))!=(time_t)(-1))
        if (DateFromLocal(&d,t))
        {
            sprintf(s,"%02d/%s/%04d:%02d:%02d:%02d %s",d.tm_mday,_DateMonth[d.tm_mon],
                d.tm_year+1900,d.tm_hour,d.tm_min,d.tm_sec,_DateTimeBiasStr);
            
            return TRUE;
        };

    *s='\0';
    return FALSE;
}

abyss_bool DateDecode(char *s,TDate *tm)
{
    uint32_t n=0;

    /* Ignore spaces, day name and spaces */
    while ((*s==' ') || (*s=='\t'))
        s++;

    while ((*s!=' ') && (*s!='\t'))
        s++;

    while ((*s==' ') || (*s=='\t'))
        s++;

    /* try to recognize the date format */
    if (sscanf(s,"%*s %d %d:%d:%d %d%*s",&tm->tm_mday,&tm->tm_hour,&tm->tm_min,
            &tm->tm_sec,&tm->tm_year)!=5)
        if (sscanf(s,"%d %n%*s %d %d:%d:%d GMT%*s",&tm->tm_mday,&n,&tm->tm_year,
            &tm->tm_hour,&tm->tm_min,&tm->tm_sec)!=5)
            if (sscanf(s,"%d-%n%*[A-Za-z]-%d %d:%d:%d GMT%*s",&tm->tm_mday,&n,&tm->tm_year,
                    &tm->tm_hour,&tm->tm_min,&tm->tm_sec)!=5)
                return FALSE;

    /* s points now to the month string */
    s+=n;
    for (n=0;n<12;n++)
    {
        char *p=_DateMonth[n];

        if (tolower(*p++)==tolower(*s))
            if (*p++==tolower(s[1]))
                if (*p==tolower(s[2]))
                    break;
    };

    if (n==12)
        return FALSE;

    tm->tm_mon=n;

    /* finish the work */
    if (tm->tm_year>1900)
        tm->tm_year-=1900;
    else
        if (tm->tm_year<70)
            tm->tm_year+=100;

    tm->tm_isdst=0;

    return (mktime(tm)!=(time_t)(-1));
}

int32_t DateCompare(TDate *d1,TDate *d2)
{
    int32_t x;

    if ((x=d1->tm_year-d2->tm_year)==0)
        if ((x=d1->tm_mon-d2->tm_mon)==0)
            if ((x=d1->tm_mday-d2->tm_mday)==0)
                if ((x=d1->tm_hour-d2->tm_hour)==0)
                    if ((x=d1->tm_min-d2->tm_min)==0)
                        x=d1->tm_sec-d2->tm_sec;

    return x;
}



abyss_bool
DateFromGMT(TDate *d,time_t t) {
    TDate *dx;

    dx=gmtime(&t);
    if (dx) {
        *d=*dx;
        return TRUE;
    };

    return FALSE;
}

abyss_bool DateFromLocal(TDate *d,time_t t)
{
    return DateFromGMT(d,t+_DateTimeBias*2);
}



abyss_bool
DateInit() {
    time_t t;
    TDate gmt,local,*d;

    time(&t);
    if (DateFromGMT(&gmt,t)) {
        d=localtime(&t);
        if (d) {
            local=*d;
            _DateTimeBias =
                (local.tm_sec-gmt.tm_sec)+(local.tm_min-gmt.tm_min)*60
                +(local.tm_hour-gmt.tm_hour)*3600;
            sprintf(_DateTimeBiasStr, "%+03d%02d",
                    _DateTimeBias/3600,(_DateTimeBias % 3600)/60);
            return TRUE;
        };
    }
    return FALSE;
}



/*********************************************************************
** Base64
*********************************************************************/

void Base64Encode(char *s,char *d)
{
    /* Conversion table. */
    static char tbl[64] = {
        'A','B','C','D','E','F','G','H',
        'I','J','K','L','M','N','O','P',
        'Q','R','S','T','U','V','W','X',
        'Y','Z','a','b','c','d','e','f',
        'g','h','i','j','k','l','m','n',
        'o','p','q','r','s','t','u','v',
        'w','x','y','z','0','1','2','3',
        '4','5','6','7','8','9','+','/'
    };

    uint32_t i,length=strlen(s);
    char *p=d;
    
    /* Transform the 3x8 bits to 4x6 bits, as required by base64. */
    for (i = 0; i < length; i += 3)
    {
        *p++ = tbl[s[0] >> 2];
        *p++ = tbl[((s[0] & 3) << 4) + (s[1] >> 4)];
        *p++ = tbl[((s[1] & 0xf) << 2) + (s[2] >> 6)];
        *p++ = tbl[s[2] & 0x3f];
        s += 3;
    }
    
    /* Pad the result if necessary... */
    if (i == length + 1)
        *(p - 1) = '=';
    else if (i == length + 2)
        *(p - 1) = *(p - 2) = '=';
    
    /* ...and zero-terminate it. */
    *p = '\0';
}

