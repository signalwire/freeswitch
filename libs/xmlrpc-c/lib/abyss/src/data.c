/******************************************************************************
**
** list.c
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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "mallocvar.h"

#include "xmlrpc-c/abyss.h"

#include "token.h"

#define safe_free(it) if (it) {free(it);it=NULL;}
/*********************************************************************
** List
*********************************************************************/

void ListInit(TList *sl)
{
    sl->item=NULL;
    sl->size=sl->maxsize=0;
    sl->autofree=FALSE;
}

void ListInitAutoFree(TList *sl)
{
    sl->item=NULL;
    sl->size=sl->maxsize=0;
    sl->autofree=TRUE;
}



void
ListFree(TList * const sl) {

    if (sl->item) {
        if (sl->autofree) {
            unsigned int i;
            for (i = sl->size; i > 0; --i)
                safe_free(sl->item[i-1]);
            
        }
        safe_free(sl->item);
    }
    sl->item = NULL;
    sl->size = 0;
    sl->maxsize = 0;
}



void
ListFreeItems(TList * const sl) {

    if (sl->item) {
        unsigned int i;
        for (i = sl->size; i > 0; --i)
            safe_free(sl->item[i-1]);
    }
}



abyss_bool
ListAdd(TList * const sl,
        void *  const str) {
/*----------------------------------------------------------------------------
   Add an item to the end of the list.
-----------------------------------------------------------------------------*/
    abyss_bool success;

    if (sl->size >= sl->maxsize) {
        uint16_t newSize = sl->maxsize + 16;
        void **newitem;
        
        newitem = realloc(sl->item, newSize * sizeof(void *));
        if (newitem) {
            sl->item    = newitem;
            sl->maxsize = newSize;
        }
    }

    if (sl->size >= sl->maxsize)
        success = FALSE;
    else {
        success = TRUE;
        sl->item[sl->size++] = str;
    }
    return success;
}



void
ListRemove(TList * const sl) {
/*----------------------------------------------------------------------------
   Remove the last item from the list.
-----------------------------------------------------------------------------*/

    assert(sl->size > 0);

    --sl->size;
}



abyss_bool
ListAddFromString(TList * const list,
                  char *  const stringArg) {

    char *t,*p;
    char * c;

    c = stringArg;

    if (c)
        while (1)
        {
            NextToken(&c);

            while (*c==',')
                c++;

            if (!(t=GetToken(&c)))
                break;

            p=c-2;

            while (*p==',')
                *(p--)='\0';

            if (*t)
                if (!ListAdd(list,t))
                    return FALSE;
        };

    return TRUE;
}

abyss_bool
ListFindString(TList *    const sl,
               char *     const str,
               uint16_t * const indexP)
{
    uint16_t i;

    if (sl->item && str)
        for (i=0;i<sl->size;i++)
            if (strcmp(str,(char *)(sl->item[i]))==0)
            {
                *indexP=i;
                return TRUE;
            };

    return FALSE;
}

/*********************************************************************
** Buffer
*********************************************************************/

abyss_bool BufferAlloc(TBuffer *buf,uint32_t memsize)
{
    /* ************** Implement the static buffers ***/
    buf->staticid=0;
    buf->data=(void *)malloc(memsize);
    if (buf->data)
    {
        buf->size=memsize;
        return TRUE;
    }
    else
    {
        buf->size=0;
        return FALSE;
    };
}

void BufferFree(TBuffer *buf)
{
    if (buf->staticid)
    {
        /* ************** Implement the static buffers ***/
    }
    else
        safe_free(buf->data);

    buf->size=0;
    buf->staticid=0;
}

abyss_bool BufferRealloc(TBuffer *buf,uint32_t memsize)
{
    if (buf->staticid)
    {
        TBuffer b;

        if (memsize<=buf->size)
            return TRUE;

        if (BufferAlloc(&b,memsize))
        {
            memcpy(b.data,buf->data,buf->size);
            BufferFree(buf);
            *buf=b;
            return TRUE;
        }
    }
    else
    {
        void *d;
        
        d=realloc(buf->data,memsize);
        if (d)
        {
            buf->data=d;
            buf->size=memsize;
            return TRUE;
        }
    }

    return FALSE;
}


/*********************************************************************
** String
*********************************************************************/

abyss_bool StringAlloc(TString *s)
{
    s->size=0;
    if (BufferAlloc(&(s->buffer),256))
    {
        *(char *)(s->buffer.data)='\0';
        return TRUE;
    }
    else
        return FALSE;
}

abyss_bool StringConcat(TString *s,char *s2)
{
    uint32_t len=strlen(s2);

    if (len+s->size+1>s->buffer.size)
        if (!BufferRealloc(&(s->buffer),((len+s->size+1+256)/256)*256))
            return FALSE;
    
    strcat((char *)(s->buffer.data),s2);
    s->size+=len;
    return TRUE;
}

abyss_bool StringBlockConcat(TString *s,char *s2,char **ref)
{
    uint32_t len=strlen(s2)+1;

    if (len+s->size>s->buffer.size)
        if (!BufferRealloc(&(s->buffer),((len+s->size+1+256)/256)*256))
            return FALSE;
    
    *ref=(char *)(s->buffer.data)+s->size;
    memcpy(*ref,s2,len);
    s->size+=len;
    return TRUE;
}

void StringFree(TString *s)
{
    s->size=0;
    BufferFree(&(s->buffer));
}

char *StringData(TString *s)
{
    return (char *)(s->buffer.data);
}

/*********************************************************************
** Hash
*********************************************************************/

static uint16_t
Hash16(const char * const start) {

    const char * s;
    
    uint16_t i;
    
    s = start;
    i = 0;

    while(*s)
        i = i * 37 + *s++;

    return i;
}

/*********************************************************************
** Table
*********************************************************************/

void TableInit(TTable *t)
{
    t->item=NULL;
    t->size=t->maxsize=0;
}

void TableFree(TTable *t)
{
    uint16_t i;

    if (t->item)
    {
        if (t->size)
            for (i=t->size;i>0;i--)
            {
                safe_free(t->item[i-1].name);
                safe_free(t->item[i-1].value);
            };
            
        safe_free(t->item);
    }

    TableInit(t);
}

abyss_bool TableFindIndex(TTable *t,char *name,uint16_t *index)
{
    uint16_t i,hash=Hash16(name);

    if ((t->item) && (t->size>0) && (*index<t->size))
    {
        for (i=*index;i<t->size;i++)
            if (hash==t->item[i].hash)
                if (strcmp(t->item[i].name,name)==0)
                {
                    *index=i;
                    return TRUE;
                };
    };

    return FALSE;
}

abyss_bool TableAddReplace(TTable *t,char *name,char *value)
{
    uint16_t i=0;

    if (TableFindIndex(t,name,&i))
    {
        safe_free(t->item[i].value);
        if (value)
            t->item[i].value=strdup(value);
        else
        {
            safe_free(t->item[i].name);
            if (--t->size>0)
                t->item[i]=t->item[t->size];
        };

        return TRUE;
    }
    else
        return TableAdd(t,name,value);
}



abyss_bool
TableAdd(TTable *t,char *name,char *value) {

    if (t->size>=t->maxsize) {
        TTableItem *newitem;
        
        t->maxsize+=16;

        newitem=(TTableItem *)realloc(t->item,(t->maxsize)*sizeof(TTableItem));
        if (newitem)
            t->item=newitem;
        else {
            t->maxsize-=16;
            return FALSE;
        }
    }

    t->item[t->size].name=strdup(name);
    t->item[t->size].value=strdup(value);
    t->item[t->size].hash=Hash16(name);

    ++t->size;

    return TRUE;
}

char *TableFind(TTable *t,char *name)
{
    uint16_t i=0;

    if (TableFindIndex(t,name,&i))
        return t->item[i].value;
    else
        return NULL;
}

/*********************************************************************
** Pool
*********************************************************************/

static TPoolZone *
PoolZoneAlloc(uint32_t zonesize) {
    TPoolZone *pz;

    pz=(TPoolZone *)malloc(zonesize+sizeof(TPoolZone));
    if (pz)
    {
        pz->pos=pz->data;
        pz->maxpos=pz->pos+zonesize;
        pz->next=pz->prev=NULL;
    };

    return pz;
}

abyss_bool PoolCreate(TPool *p,uint32_t zonesize)
{
    p->zonesize=zonesize;
    if (MutexCreate(&p->mutex))
        if (!(p->firstzone=p->currentzone=PoolZoneAlloc(zonesize)))
        {
            MutexFree(&p->mutex);
            return FALSE;
        };
    
    return TRUE;
}

void *PoolAlloc(TPool *p,uint32_t size)
{
    TPoolZone *pz,*npz;
    void *x;
    uint32_t zonesize;

    if (size==0)
        return NULL;

    if (!MutexLock(&p->mutex))
        return NULL;

    pz=p->currentzone;

    if (pz->pos+size<pz->maxpos)
    {
        x=pz->pos;
        pz->pos+=size;
        MutexUnlock(&p->mutex);
        return x;
    };

    if (size>p->zonesize)
        zonesize=size;
    else
        zonesize=p->zonesize;

    npz=PoolZoneAlloc(zonesize);
    if (npz)
    {
        npz->prev=pz;
        npz->next=pz->next;
        pz->next=npz;
        p->currentzone=npz; 
        x=npz->data;
        npz->pos=npz->data+size;
    }
    else
        x=NULL;

    MutexUnlock(&p->mutex);
    return x;
}

void PoolFree(TPool *p)
{
    TPoolZone *pz,*npz;

    pz=p->firstzone;

    while (pz)
    {
        npz=pz->next;
        safe_free(pz);
        pz=npz;
    };
}



char *PoolStrdup(TPool *p,char *s) {

    char *ns;

    if (s) {
        ns = PoolAlloc(p, strlen(s) + 1);
        if (ns)
            strcpy(ns, s);
    } else
        ns = NULL;

    return ns;
}
