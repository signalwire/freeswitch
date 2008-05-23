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

#include "bool.h"
#include "mallocvar.h"
#include "xmlrpc-c/util_int.h"
#include "xmlrpc-c/string_int.h"

#include "xmlrpc-c/abyss.h"

#include "token.h"
#include "thread.h"

#include "data.h"

/*********************************************************************
** List
*********************************************************************/

void ListInit(TList * const sl)
{
    sl->item=NULL;
    sl->size=sl->maxsize=0;
    sl->autofree=FALSE;
}

void ListInitAutoFree(TList * const sl)
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
                free(sl->item[i-1]);
            
        }
        free(sl->item);
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
            free(sl->item[i-1]);
    }
}



bool
ListAdd(TList * const sl,
        void *  const str) {
/*----------------------------------------------------------------------------
   Add an item to the end of the list.
-----------------------------------------------------------------------------*/
    bool success;

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



bool
ListAddFromString(TList *      const list,
                  const char * const stringArg) {

    bool retval;
    
    if (!stringArg)
        retval = TRUE;
    else {
        char * buffer;
        
        buffer = strdup(stringArg);
        if (!buffer)
            retval = FALSE;
        else {
            bool endOfString;
            bool error;
            char * c;

            for (c = &buffer[0], endOfString = FALSE, error = FALSE;
                 !endOfString && !error;
                ) {
                const char * t;
                NextToken((const char **)&c);
                
                while (*c == ',')
                    ++c;
                
                t = GetToken(&c);
                if (!t)
                    endOfString = TRUE;
                else {
                    char * p;

                    for (p = c - 2; *p == ','; --p)
                        *p = '\0';
                    
                    if (t[0] != '\0') {
                        bool added;
                        added = ListAdd(list, (void*)t);
                        
                        if (!added)
                            error = TRUE;
                    }
                }
            }
            retval = !error;
            xmlrpc_strfree(buffer);
        }
    }
    return retval;
}



bool
ListFindString(TList *      const sl,
               const char * const str,
               uint16_t *   const indexP)
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

bool
BufferAlloc(TBuffer *       const buf,
            xmlrpc_uint32_t const memsize) {

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



void
BufferFree(TBuffer * const buf) {

    if (buf->staticid)
    {
        /* ************** Implement the static buffers ***/
    }
    else
        free(buf->data);

    buf->size=0;
    buf->staticid=0;
}



bool
BufferRealloc(TBuffer *       const buf,
              xmlrpc_uint32_t const memsize) {

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

bool
StringAlloc(TString * const stringP) {

    bool succeeded;
    
    stringP->size = 0;

    succeeded = BufferAlloc(&stringP->buffer, 256);
    if (succeeded) {
        *(char *)(stringP->buffer.data) = '\0';
        return TRUE;
    } else
        return FALSE;
}



bool
StringConcat(TString *    const stringP,
             const char * const string2) {

    uint32_t const len = strlen(string2);

    if (len + stringP->size + 1 > stringP->buffer.size) {
        bool succeeded;
        succeeded = BufferRealloc(
            &stringP->buffer,
            ((len + stringP->size + 1 + 256) / 256) * 256);
        if (!succeeded)
            return FALSE;
    }
    strcat((char *)(stringP->buffer.data), string2);
    stringP->size += len;
    return TRUE;
}



bool
StringBlockConcat(TString *    const stringP,
                  const char * const string2,
                  char **      const ref) {

    uint32_t const len = strlen(string2) + 1;

    if (len + stringP->size > stringP->buffer.size) {
        bool succeeded;
        succeeded = BufferRealloc(
            &stringP->buffer,
            ((len + stringP->size + 1 + 256) / 256) * 256);
        if (!succeeded)
            return FALSE;
    }
    *ref = (char *)(stringP->buffer.data) + stringP->size;
    memcpy(*ref, string2, len);
    stringP->size += len;

    return TRUE;
}



void
StringFree(TString * const stringP) {
    stringP->size = 0;
    BufferFree(&stringP->buffer);
}



char *
StringData(TString * const stringP) {
    return (char *)stringP->buffer.data;
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

void TableInit(TTable * const t)
{
    t->item=NULL;
    t->size=t->maxsize=0;
}

void TableFree(TTable * const t)
{
    uint16_t i;

    if (t->item)
    {
        if (t->size)
            for (i=t->size;i>0;i--)
            {
                free(t->item[i-1].name);
                free(t->item[i-1].value);
            };
            
        free(t->item);
    }

    TableInit(t);
}



bool
TableFindIndex(TTable *     const t,
               const char * const name,
               uint16_t *   const index) {

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



bool
TableAddReplace(TTable *     const t,
                const char * const name,
                const char * const value) {

    uint16_t i=0;

    if (TableFindIndex(t,name,&i))
    {
        free(t->item[i].value);
        if (value)
            t->item[i].value=strdup(value);
        else
        {
            free(t->item[i].name);
            if (--t->size>0)
                t->item[i]=t->item[t->size];
        };

        return TRUE;
    }
    else
        return TableAdd(t,name,value);
}



bool
TableAdd(TTable *     const t,
         const char * const name,
         const char * const value) {

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



char *
TableFind(TTable *     const t,
          const char * const name) {

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
PoolZoneAlloc(uint32_t const zonesize) {

    TPoolZone * poolZoneP;
    
    MALLOCARRAY(poolZoneP, zonesize);
    if (poolZoneP) {
        poolZoneP->pos    = &poolZoneP->data[0];
        poolZoneP->maxpos = poolZoneP->pos + zonesize;
        poolZoneP->next   = NULL;
        poolZoneP->prev   = NULL;
    }
    return poolZoneP;
}



static void
PoolZoneFree(TPoolZone * const poolZoneP) {

    free(poolZoneP);
}



bool
PoolCreate(TPool *  const poolP,
           uint32_t const zonesize) {

    bool success;
    bool mutexCreated;

    poolP->zonesize = zonesize;

    mutexCreated = MutexCreate(&poolP->mutexP);
    if (mutexCreated) {
        TPoolZone * const firstZoneP = PoolZoneAlloc(zonesize);

        if (firstZoneP != NULL) {
            poolP->firstzone   = firstZoneP;
            poolP->currentzone = firstZoneP;
            success = TRUE;
        } else
            success = FALSE;
        if (!success)
            MutexDestroy(poolP->mutexP);
    } else
        success = FALSE;

    return success;
}



void *
PoolAlloc(TPool *  const poolP,
          uint32_t const size) {
/*----------------------------------------------------------------------------
   Allocate a block of size 'size' from pool 'poolP'.
-----------------------------------------------------------------------------*/
    void * retval;

    if (size == 0)
        retval = NULL;
    else {
        bool gotMutexLock;

        gotMutexLock = MutexLock(poolP->mutexP);
        if (!gotMutexLock)
            retval = NULL;
        else {
            TPoolZone * const curPoolZoneP = poolP->currentzone;

            if (curPoolZoneP->pos + size < curPoolZoneP->maxpos) {
                retval = curPoolZoneP->pos;
                curPoolZoneP->pos += size;
            } else {
                uint32_t const zonesize = MAX(size, poolP->zonesize);

                TPoolZone * const newPoolZoneP = PoolZoneAlloc(zonesize);
                if (newPoolZoneP) {
                    newPoolZoneP->prev = curPoolZoneP;
                    newPoolZoneP->next = curPoolZoneP->next;
                    curPoolZoneP->next = newPoolZoneP;
                    poolP->currentzone = newPoolZoneP;
                    retval= newPoolZoneP->data;
                    newPoolZoneP->pos = newPoolZoneP->data + size;
                } else
                    retval = NULL;
            }
            MutexUnlock(poolP->mutexP);
        }
    }
    return retval;
}



void
PoolReturn(TPool *  const poolP,
           void *   const blockP) {
/*----------------------------------------------------------------------------
   Return the block at 'blockP' to the pool 'poolP'.  WE ASSUME THAT IS
   THE MOST RECENTLY ALLOCATED AND NOT RETURNED BLOCK IN THE POOL.
-----------------------------------------------------------------------------*/
    TPoolZone * const curPoolZoneP = poolP->currentzone;

    assert((char*)curPoolZoneP->data < (char*)blockP &&
           (char*)blockP < (char*)curPoolZoneP->pos);

    curPoolZoneP->pos = blockP;

    if (curPoolZoneP->pos == curPoolZoneP->data) {
        /* That emptied out the current zone.  Free it and make the previous
           zone current.
        */

        assert(curPoolZoneP->prev);  /* entry condition */

        curPoolZoneP->prev->next = NULL;

        PoolZoneFree(curPoolZoneP);
    }
}



void
PoolFree(TPool * const poolP) {

    TPoolZone * poolZoneP;
    TPoolZone * nextPoolZoneP;
    
    for (poolZoneP = poolP->firstzone; poolZoneP; poolZoneP = nextPoolZoneP) {
        nextPoolZoneP = poolZoneP->next;
        free(poolZoneP);
    }
    MutexDestroy(poolP->mutexP);
}



const char *
PoolStrdup(TPool *      const poolP,
           const char * const origString) {

    char * newString;

    if (origString == NULL)
        newString = NULL;
    else {
        newString = PoolAlloc(poolP, strlen(origString) + 1);
        if (newString != NULL)
            strcpy(newString, origString);
    }
    return newString;
}
