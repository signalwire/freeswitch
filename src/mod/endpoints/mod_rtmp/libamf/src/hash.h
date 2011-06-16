#ifndef _HASH_H_
#define _HASH_H_

/* Forward declarations of structures. */
typedef struct Hash Hash;
typedef struct HashElem HashElem;

/* A complete hash table is an instance of the following structure.
** The internals of this structure are intended to be opaque -- client
** code should not attempt to access or modify the fields of this structure
** directly.  Change this structure only by using the routines below.
** However, many of the "procedures" and "functions" for modifying and
** accessing this structure are really macros, so we can't really make
** this structure opaque.
*/
struct Hash {
  char copyKey;              /* True if copy of key made on insert */
  int  count;                /* Number of entries in this table */
  HashElem *first;           /* The first element of the array */
  void *(*xMalloc)(size_t);  /* malloc() function to use */
  void (*xFree)(void *);     /* free() function to use */
  int htsize;                /* Number of buckets in the hash table */
  struct _ht {               /* the hash table */
    int count;               /* Number of entries with this hash */
    HashElem *chain;         /* Pointer to first entry with this hash */
  } *ht;
};

/* Each element in the hash table is an instance of the following
** structure.  All elements are stored on a single doubly-linked list.
**
** Again, this structure is intended to be opaque, but it can't really
** be opaque because it is used by macros.
*/
struct HashElem {
  HashElem *next, *prev;     /* Next and previous elements in the table */
  void *data;                /* Data associated with this element */
  void *pKey; int nKey;      /* Key associated with this element */
};

/*
** Access routines.  To delete, insert a NULL pointer.
*/
Hash * HashCreate(char copyKey);
Hash * HashCreateAlloc(char copyKey, void *(*xMalloc)(size_t), void (*xFree)(void *));

void HashFree(Hash*);

void HashInit(Hash*, char copyKey, void *(*xMalloc)(size_t), void (*xFree)(void *));
void * HashInsert(Hash*, const void *pKey, int nKey, void *pData);
void * HashFind(const Hash*, const void *pKey, int nKey);
void HashClear(Hash*);

void * HashInsertSz(Hash*, const char *pKey, void *pData);
void * HashFindSz(const Hash*, const char *pKey);

/*
    Element deletion macro
*/
#define HashDelete(H, K, N) (HashInsert(H, K, N, 0))

/*
** Macros for looping over all elements of a hash table.  The idiom is
** like this:
**
**   Hash h;
**   HashElem *p;
**   ...
**   for(p=HashFirst(&h); p; p=HashNext(p)){
**     SomeStructure *pData = HashData(p);
**     // do something with pData
**   }
*/
#define HashFirst(H)  ((H)->first)
#define HashNext(E)   ((E)->next)
#define HashData(E)   ((E)->data)
#define HashKey(E)    ((E)->pKey)
#define HashKeysize(E) ((E)->nKey)

/*
** Number of entries in a hash table
*/
#define HashCount(H)  ((H)->count)


/*
    more macros
*/
typedef struct Hash*            hash_table;
typedef struct HashElem*        hash_elem;

#define hash_create()           (HashCreate(1))
#define hash_insert(H, K, V)    (HashInsertSz(H, K, V))
#define hash_find(H, K)         (HashFindSz(H, K))
#define hash_delete(H, K)       (HashInsertSz(H, K, 0))
#define hash_clear(H)           (HashClear(H))
#define hash_free(H)            (HashFree(H))

#define hash_first(H)           ((H)->first)
#define hash_next(E)            ((E)->next)
#define hash_data(E)            ((E)->data)
#define hash_key(E)             ((E)->pKey)
#define hash_keysize(E)         ((E)->nKey)

#define hash_count(H)           ((H)->count)

#endif /* _HASH_H_ */
