#ifndef DATA_H_INCLUDED
#define DATA_H_INCLUDED

#include "bool.h"
#include "int.h"

struct abyss_mutex;

/*********************************************************************
** Buffer
*********************************************************************/

typedef struct
{
    void *data;
    xmlrpc_uint32_t size;
    xmlrpc_uint32_t staticid;
} TBuffer;

bool
BufferAlloc(TBuffer *       const buf,
            xmlrpc_uint32_t const memsize);

bool
BufferRealloc(TBuffer *       const buf,
              xmlrpc_uint32_t const memsize);

void
BufferFree(TBuffer * const buf);


/*********************************************************************
** String
*********************************************************************/

typedef struct
{
    TBuffer buffer;
    xmlrpc_uint32_t size;
} TString;

bool
StringAlloc(TString * const stringP);

bool
StringConcat(TString *    const stringP,
             const char * const string2);

bool
StringBlockConcat(TString *    const stringP,
                  const char * const string2,
                  char **      const ref);

void
StringFree(TString * const stringP);

char *
StringData(TString * const stringP);


/*********************************************************************
** List
*********************************************************************/

typedef struct {
    void **item;
    uint16_t size;
    uint16_t maxsize;
    bool autofree;
} TList;

void
ListInit(TList * const listP);

void
ListInitAutoFree(TList * const listP);

void
ListFree(TList * const listP);

void
ListFreeItems(TList * const listP);

bool
ListAdd(TList * const listP,
        void *  const str);

void
ListRemove(TList * const listP);

bool
ListAddFromString(TList *      const listP,
                  const char * const c);

bool
ListFindString(TList *      const listP,
               const char * const str,
               uint16_t *   const indexP);


typedef struct 
{
    char *name,*value;
    uint16_t hash;
} TTableItem;

typedef struct
{
    TTableItem *item;
    uint16_t size,maxsize;
} TTable;

void
TableInit(TTable * const t);

void
TableFree(TTable * const t);

bool
TableAdd(TTable *     const t,
         const char * const name,
         const char * const value);

bool
TableAddReplace(TTable *     const t,
                const char * const name,
                const char * const value);

bool
TableFindIndex(TTable *     const t,
               const char * const name,
               uint16_t *   const index);

char *
TableFind(TTable *     const t,
          const char * const name);


/*********************************************************************
** Pool
*********************************************************************/

typedef struct _TPoolZone {
    char * pos;
    char * maxpos;
    struct _TPoolZone * next;
    struct _TPoolZone * prev;
/*  char data[0]; Some compilers don't accept this */
    char data[1];
} TPoolZone;

typedef struct {
    TPoolZone * firstzone;
    TPoolZone * currentzone;
    uint32_t zonesize;
    struct abyss_mutex * mutexP;
} TPool;

bool
PoolCreate(TPool *  const poolP,
           uint32_t const zonesize);

void
PoolFree(TPool * const poolP);

void *
PoolAlloc(TPool *  const poolP,
          uint32_t const size);

void
PoolReturn(TPool *  const poolP,
           void *   const blockP);

const char *
PoolStrdup(TPool *      const poolP,
           const char * const origString);


#endif
