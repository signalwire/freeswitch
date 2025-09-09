/*
Copyright (c) 1998, 1999, 2000 Thai Open Source Software Center Ltd
See the file copying.txt for copying permission.
*/

/* In 2001, this was part of the Expat package.  We copied it into
   Xmlrpc-c because it's easier on the user than making him get and
   link Expat separately, and we don't expect to benefit from separate
   maintenance of Expat.

   But we changed all the external symbols that in Expat are named
   "XML_xxxx" to "xmlrpc_XML_xxxx" because people do link Xmlrpc-c
   libraries into programs that also link Expat (a good example is
   where an Apache module uses Xmlrpc-c).  We don't want our names to
   collide with Expat's.
*/

#include <stddef.h>
#include <assert.h>

#include "xmlrpc_config.h"
#include "c_util.h"
#include "girmath.h"
#include "mallocvar.h"
#include "xmlrpc-c/string_int.h"
#include "xmldef.h"
#include "xmlparse.h"

static const char *
extractXmlSample(const char * const start,
                 const char * const end,
                 size_t       const maximumLen) {

    size_t const len = MIN(maximumLen, (size_t)(end - start));
    
    return xmlrpc_makePrintable_lp(start, len);
}



#ifdef XML_UNICODE
#define XML_ENCODE_MAX XML_UTF16_ENCODE_MAX
#define XmlConvert XmlUtf16Convert
#define XmlGetInternalEncoding xmlrpc_XmlGetUtf16InternalEncoding
#define XmlGetInternalEncodingNS xmlrpc_XmlGetUtf16InternalEncodingNS
#define XmlEncode xmlrpc_XmlUtf16Encode
#define MUST_CONVERT(enc, s) (!(enc)->isUtf16 || (((unsigned long)s) & 1))
typedef unsigned short ICHAR;
#else
#define XML_ENCODE_MAX XML_UTF8_ENCODE_MAX
#define XmlConvert XmlUtf8Convert
#define XmlGetInternalEncoding xmlrpc_XmlGetUtf8InternalEncoding
#define XmlGetInternalEncodingNS xmlrpc_XmlGetUtf8InternalEncodingNS
#define XmlEncode xmlrpc_XmlUtf8Encode
#define MUST_CONVERT(enc, s) (!(enc)->isUtf8)
typedef char ICHAR;
#endif


#ifndef XML_NS

#define XmlInitEncodingNS XmlInitEncoding
#define XmlInitUnknownEncodingNS XmlInitUnknownEncoding
#undef XmlGetInternalEncodingNS
#define XmlGetInternalEncodingNS XmlGetInternalEncoding
#define XmlParseXmlDeclNS XmlParseXmlDecl

#endif

#ifdef XML_UNICODE_WCHAR_T
#define XML_T(x) L ## x
#else
#define XML_T(x) x
#endif

/* Round up n to be a multiple of sz, where sz is a power of 2. */
#define ROUND_UP(n, sz) (((n) + ((sz) - 1)) & ~((sz) - 1))

#include "xmltok.h"
#include "xmlrole.h"

typedef const XML_Char *KEY;

typedef struct {
  KEY name;
} NAMED;

typedef struct {
  NAMED **v;
  size_t size;
  size_t used;
  size_t usedLim;
} HASH_TABLE;

typedef struct {
  NAMED **p;
  NAMED **end;
} HASH_TABLE_ITER;

#define INIT_TAG_BUF_SIZE 32  /* must be a multiple of sizeof(XML_Char) */
#define INIT_DATA_BUF_SIZE 1024
#define INIT_ATTS_SIZE 16
#define INIT_BLOCK_SIZE 1024
#define INIT_BUFFER_SIZE 1024

#define EXPAND_SPARE 24

typedef struct binding {
  struct prefix *prefix;
  struct binding *nextTagBinding;
  struct binding *prevPrefixBinding;
  const struct attribute_id *attId;
  XML_Char *uri;
  int uriLen;
  int uriAlloc;
} BINDING;

typedef struct prefix {
  const XML_Char *name;
  BINDING *binding;
} PREFIX;

typedef struct {
  const XML_Char *str;
  const XML_Char *localPart;
  int uriLen;
} TAG_NAME;

typedef struct tag {
  struct tag *parent;
  const char *rawName;
  int rawNameLength;
  TAG_NAME name;
  char *buf;
  char *bufEnd;
  BINDING *bindings;
} TAG;

typedef struct {
  const XML_Char *name;
  const XML_Char *textPtr;
  size_t textLen;
  const XML_Char *systemId;
  const XML_Char *base;
  const XML_Char *publicId;
  const XML_Char *notation;
  char open;
} ENTITY;

typedef struct block {
  struct block *next;
  int size;
  XML_Char s[1];
} BLOCK;

typedef struct {
  BLOCK *blocks;
  BLOCK *freeBlocks;
  const XML_Char *end;
  XML_Char *ptr;
  XML_Char *start;
} STRING_POOL;

/* The XML_Char before the name is used to determine whether
an attribute has been specified. */
typedef struct attribute_id {
  XML_Char *name;
  PREFIX *prefix;
  char maybeTokenized;
  char xmlns;
} ATTRIBUTE_ID;

typedef struct {
  const ATTRIBUTE_ID *id;
  char isCdata;
  const XML_Char *value;
} DEFAULT_ATTRIBUTE;

typedef struct {
  const XML_Char *name;
  PREFIX *prefix;
  const ATTRIBUTE_ID *idAtt;
  int nDefaultAtts;
  int allocDefaultAtts;
  DEFAULT_ATTRIBUTE *defaultAtts;
} ELEMENT_TYPE;

typedef struct {
  HASH_TABLE generalEntities;
  HASH_TABLE elementTypes;
  HASH_TABLE attributeIds;
  HASH_TABLE prefixes;
  STRING_POOL pool;
  int complete;
  int standalone;
  HASH_TABLE paramEntities;
  PREFIX defaultPrefix;
} DTD;

typedef struct open_internal_entity {
  const char *internalEventPtr;
  const char *internalEventEndPtr;
  struct open_internal_entity *next;
  ENTITY *entity;
} OPEN_INTERNAL_ENTITY;

typedef void Processor(XML_Parser             parser,
                       const char *     const start,
                       const char *     const end,
                       const char **    const endPtr,
                       enum XML_Error * const errorCodeP,
                       const char **    const errorP);


#define poolStart(pool) ((pool)->start)
#define poolEnd(pool) ((pool)->ptr)
#define poolLength(pool) ((pool)->ptr - (pool)->start)
#define poolChop(pool) ((void)--(pool->ptr))
#define poolLastChar(pool) (((pool)->ptr)[-1])
#define poolDiscard(pool) ((pool)->ptr = (pool)->start)
#define poolFinish(pool) ((pool)->start = (pool)->ptr)
#define poolAppendChar(pool, c) \
  (((pool)->ptr == (pool)->end && !poolGrow(pool)) \
   ? 0 \
   : ((*((pool)->ptr)++ = c), 1))

typedef struct {
  /* The first member must be userData so that the XML_GetUserData macro works. */
  void *m_userData;
  void *m_handlerArg;
  char *m_buffer;
  /* first character to be parsed */
  const char *m_bufferPtr;
  /* past last character to be parsed */
  char *m_bufferEnd;
  /* allocated end of buffer */
  const char *m_bufferLim;
  long m_parseEndByteIndex;
  const char *m_parseEndPtr;
  XML_Char *m_dataBuf;
  XML_Char *m_dataBufEnd;
  XML_StartElementHandler m_startElementHandler;
  XML_EndElementHandler m_endElementHandler;
  XML_CharacterDataHandler m_characterDataHandler;
  XML_ProcessingInstructionHandler m_processingInstructionHandler;
  XML_CommentHandler m_commentHandler;
  XML_StartCdataSectionHandler m_startCdataSectionHandler;
  XML_EndCdataSectionHandler m_endCdataSectionHandler;
  XML_DefaultHandler m_defaultHandler;
  XML_StartDoctypeDeclHandler m_startDoctypeDeclHandler;
  XML_EndDoctypeDeclHandler m_endDoctypeDeclHandler;
  XML_UnparsedEntityDeclHandler m_unparsedEntityDeclHandler;
  XML_NotationDeclHandler m_notationDeclHandler;
  XML_ExternalParsedEntityDeclHandler m_externalParsedEntityDeclHandler;
  XML_InternalParsedEntityDeclHandler m_internalParsedEntityDeclHandler;
  XML_StartNamespaceDeclHandler m_startNamespaceDeclHandler;
  XML_EndNamespaceDeclHandler m_endNamespaceDeclHandler;
  XML_NotStandaloneHandler m_notStandaloneHandler;
  XML_ExternalEntityRefHandler m_externalEntityRefHandler;
  void *m_externalEntityRefHandlerArg;
  XML_UnknownEncodingHandler m_unknownEncodingHandler;
  const ENCODING *m_encoding;
  INIT_ENCODING m_initEncoding;
  const ENCODING *m_internalEncoding;
  const XML_Char *m_protocolEncodingName;
  int m_ns;
  void *m_unknownEncodingMem;
  void *m_unknownEncodingData;
  void *m_unknownEncodingHandlerData;
  void (*m_unknownEncodingRelease)(void *);
  PROLOG_STATE m_prologState;
  Processor *m_processor;
    /* The next processor to run */
  enum XML_Error m_errorCode;
    /* Explanation of the failure of the most recent call to Expat.
       XML_ERROR_NONE means it didn't fail.  This is redundant with
       m_errorString if the latter is non-null.
       The latter is newer and better.
    */
  const char * m_errorString;
    /* malloc'ed string describing the failure of the most recent call
       to Expat.  NULL means m_errorCode is the only error information
       available.
    */
  const char *m_eventPtr;
  const char *m_eventEndPtr;
  const char *m_positionPtr;
  OPEN_INTERNAL_ENTITY *m_openInternalEntities;
  int m_defaultExpandInternalEntities;
  int m_tagLevel;
  ENTITY *m_declEntity;
  const XML_Char *m_declNotationName;
  const XML_Char *m_declNotationPublicId;
  ELEMENT_TYPE *m_declElementType;
  ATTRIBUTE_ID *m_declAttributeId;
  char m_declAttributeIsCdata;
  char m_declAttributeIsId;
  DTD m_dtd;
  const XML_Char *m_curBase;
  TAG *m_tagStack;
  TAG *m_freeTagList;
  BINDING *m_inheritedBindings;
  BINDING *m_freeBindingList;
  int m_attsSize;
  int m_nSpecifiedAtts;
  int m_idAttIndex;
  ATTRIBUTE *m_atts;
  POSITION m_position;
  STRING_POOL m_tempPool;
  STRING_POOL m_temp2Pool;
  char *m_groupConnector;
  unsigned m_groupSize;
  int m_hadExternalDoctype;
  XML_Char m_namespaceSeparator;
  enum XML_ParamEntityParsing m_paramEntityParsing;
  XML_Parser m_parentParser;
} Parser;

#define userData (((Parser *)parser)->m_userData)
#define handlerArg (((Parser *)parser)->m_handlerArg)
#define startElementHandler (((Parser *)parser)->m_startElementHandler)
#define endElementHandler (((Parser *)parser)->m_endElementHandler)
#define characterDataHandler (((Parser *)parser)->m_characterDataHandler)
#define processingInstructionHandler (((Parser *)parser)->m_processingInstructionHandler)
#define commentHandler (((Parser *)parser)->m_commentHandler)
#define startCdataSectionHandler (((Parser *)parser)->m_startCdataSectionHandler)
#define endCdataSectionHandler (((Parser *)parser)->m_endCdataSectionHandler)
#define defaultHandler (((Parser *)parser)->m_defaultHandler)
#define startDoctypeDeclHandler (((Parser *)parser)->m_startDoctypeDeclHandler)
#define endDoctypeDeclHandler (((Parser *)parser)->m_endDoctypeDeclHandler)
#define unparsedEntityDeclHandler (((Parser *)parser)->m_unparsedEntityDeclHandler)
#define notationDeclHandler (((Parser *)parser)->m_notationDeclHandler)
#define externalParsedEntityDeclHandler (((Parser *)parser)->m_externalParsedEntityDeclHandler)
#define internalParsedEntityDeclHandler (((Parser *)parser)->m_internalParsedEntityDeclHandler)
#define startNamespaceDeclHandler (((Parser *)parser)->m_startNamespaceDeclHandler)
#define endNamespaceDeclHandler (((Parser *)parser)->m_endNamespaceDeclHandler)
#define notStandaloneHandler (((Parser *)parser)->m_notStandaloneHandler)
#define externalEntityRefHandler (((Parser *)parser)->m_externalEntityRefHandler)
#define externalEntityRefHandlerArg (((Parser *)parser)->m_externalEntityRefHandlerArg)
#define unknownEncodingHandler (((Parser *)parser)->m_unknownEncodingHandler)
#define initEncoding (((Parser *)parser)->m_initEncoding)
#define internalEncoding (((Parser *)parser)->m_internalEncoding)
#define unknownEncodingMem (((Parser *)parser)->m_unknownEncodingMem)
#define unknownEncodingData (((Parser *)parser)->m_unknownEncodingData)
#define unknownEncodingHandlerData \
  (((Parser *)parser)->m_unknownEncodingHandlerData)
#define unknownEncodingRelease (((Parser *)parser)->m_unknownEncodingRelease)
#define protocolEncodingName (((Parser *)parser)->m_protocolEncodingName)
#define ns (((Parser *)parser)->m_ns)
#define prologState (((Parser *)parser)->m_prologState)
#define processor (((Parser *)parser)->m_processor)
#define errorCode (((Parser *)parser)->m_errorCode)
#define errorString (((Parser *)parser)->m_errorString)
#define eventPtr (((Parser *)parser)->m_eventPtr)
#define eventEndPtr (((Parser *)parser)->m_eventEndPtr)
#define positionPtr (((Parser *)parser)->m_positionPtr)
#define position (((Parser *)parser)->m_position)
#define openInternalEntities (((Parser *)parser)->m_openInternalEntities)
#define defaultExpandInternalEntities (((Parser *)parser)->m_defaultExpandInternalEntities)
#define tagLevel (((Parser *)parser)->m_tagLevel)
#define buffer (((Parser *)parser)->m_buffer)
#define bufferPtr (((Parser *)parser)->m_bufferPtr)
#define bufferEnd (((Parser *)parser)->m_bufferEnd)
#define parseEndByteIndex (((Parser *)parser)->m_parseEndByteIndex)
#define parseEndPtr (((Parser *)parser)->m_parseEndPtr)
#define bufferLim (((Parser *)parser)->m_bufferLim)
#define dataBuf (((Parser *)parser)->m_dataBuf)
#define dataBufEnd (((Parser *)parser)->m_dataBufEnd)
#define dtd (((Parser *)parser)->m_dtd)
#define curBase (((Parser *)parser)->m_curBase)
#define declEntity (((Parser *)parser)->m_declEntity)
#define declNotationName (((Parser *)parser)->m_declNotationName)
#define declNotationPublicId (((Parser *)parser)->m_declNotationPublicId)
#define declElementType (((Parser *)parser)->m_declElementType)
#define declAttributeId (((Parser *)parser)->m_declAttributeId)
#define declAttributeIsCdata (((Parser *)parser)->m_declAttributeIsCdata)
#define declAttributeIsId (((Parser *)parser)->m_declAttributeIsId)
#define freeTagList (((Parser *)parser)->m_freeTagList)
#define freeBindingList (((Parser *)parser)->m_freeBindingList)
#define inheritedBindings (((Parser *)parser)->m_inheritedBindings)
#define tagStack (((Parser *)parser)->m_tagStack)
#define atts (((Parser *)parser)->m_atts)
#define attsSize (((Parser *)parser)->m_attsSize)
#define nSpecifiedAtts (((Parser *)parser)->m_nSpecifiedAtts)
#define idAttIndex (((Parser *)parser)->m_idAttIndex)
#define tempPool (((Parser *)parser)->m_tempPool)
#define temp2Pool (((Parser *)parser)->m_temp2Pool)
#define groupConnector (((Parser *)parser)->m_groupConnector)
#define groupSize (((Parser *)parser)->m_groupSize)
#define hadExternalDoctype (((Parser *)parser)->m_hadExternalDoctype)
#define namespaceSeparator (((Parser *)parser)->m_namespaceSeparator)
#define parentParser (((Parser *)parser)->m_parentParser)
#define paramEntityParsing (((Parser *)parser)->m_paramEntityParsing)



static
void poolInit(STRING_POOL *pool)
{
  pool->blocks = 0;
  pool->freeBlocks = 0;
  pool->start = 0;
  pool->ptr = 0;
  pool->end = 0;
}

static
void poolClear(STRING_POOL *pool)
{
  if (!pool->freeBlocks)
    pool->freeBlocks = pool->blocks;
  else {
    BLOCK *p = pool->blocks;
    while (p) {
      BLOCK *tem = p->next;
      p->next = pool->freeBlocks;
      pool->freeBlocks = p;
      p = tem;
    }
  }
  pool->blocks = 0;
  pool->start = 0;
  pool->ptr = 0;
  pool->end = 0;
}

static
void poolDestroy(STRING_POOL *pool)
{
  BLOCK *p = pool->blocks;
  while (p) {
    BLOCK *tem = p->next;
    free(p);
    p = tem;
  }
  pool->blocks = 0;
  p = pool->freeBlocks;
  while (p) {
    BLOCK *tem = p->next;
    free(p);
    p = tem;
  }
  pool->freeBlocks = 0;
  pool->ptr = 0;
  pool->start = 0;
  pool->end = 0;
}

static
int poolGrow(STRING_POOL *pool)
{
  if (pool->freeBlocks) {
    if (pool->start == 0) {
      pool->blocks = pool->freeBlocks;
      pool->freeBlocks = pool->freeBlocks->next;
      pool->blocks->next = 0;
      pool->start = pool->blocks->s;
      pool->end = pool->start + pool->blocks->size;
      pool->ptr = pool->start;
      return 1;
    }
    if (pool->end - pool->start < pool->freeBlocks->size) {
      BLOCK *tem = pool->freeBlocks->next;
      pool->freeBlocks->next = pool->blocks;
      pool->blocks = pool->freeBlocks;
      pool->freeBlocks = tem;
      memcpy(pool->blocks->s, pool->start,
             (pool->end - pool->start) * sizeof(XML_Char));
      pool->ptr = pool->blocks->s + (pool->ptr - pool->start);
      pool->start = pool->blocks->s;
      pool->end = pool->start + pool->blocks->size;
      return 1;
    }
  }
  if (pool->blocks && pool->start == pool->blocks->s) {
    size_t const blockSize = (pool->end - pool->start)*2;
    pool->blocks = realloc(pool->blocks, offsetof(BLOCK, s) +
                           blockSize * sizeof(XML_Char));
    if (!pool->blocks)
      return 0;
    pool->blocks->size = blockSize;
    pool->ptr = pool->blocks->s + (pool->ptr - pool->start);
    pool->start = pool->blocks->s;
    pool->end = pool->start + blockSize;
  }
  else {
    size_t const poolLen = pool->end - pool->start;
    size_t const blockSize =
        poolLen < INIT_BLOCK_SIZE ? INIT_BLOCK_SIZE : poolLen * 2;
    BLOCK *tem;

    tem = malloc(offsetof(BLOCK, s) + blockSize * sizeof(XML_Char));
    if (!tem)
      return 0;
    tem->size = blockSize;
    tem->next = pool->blocks;
    pool->blocks = tem;
    if (pool->ptr != pool->start)
      memcpy(tem->s, pool->start,
             (pool->ptr - pool->start) * sizeof(XML_Char));
    pool->ptr = tem->s + (pool->ptr - pool->start);
    pool->start = tem->s;
    pool->end = tem->s + blockSize;
  }
  return 1;
}



static
XML_Char *poolAppend(STRING_POOL *pool, const ENCODING *enc,
                     const char *ptr, const char *end)
{
  if (!pool->ptr && !poolGrow(pool))
    return 0;
  for (;;) {
    XmlConvert(enc, &ptr, end, (ICHAR **)&(pool->ptr), (ICHAR *)pool->end);
    if (ptr == end)
      break;
    if (!poolGrow(pool))
      return 0;
  }
  return pool->start;
}

static const XML_Char *poolCopyString(STRING_POOL *pool, const XML_Char *s)
{
  do {
    if (!poolAppendChar(pool, *s))
      return 0;
  } while (*s++);
  s = pool->start;
  poolFinish(pool);
  return s;
}

static const XML_Char *
poolCopyStringN(STRING_POOL *pool,
                const XML_Char *s,
                int n)
{
  if (!pool->ptr && !poolGrow(pool))
    return 0;
  for (; n > 0; --n, s++) {
    if (!poolAppendChar(pool, *s))
      return 0;

  }
  s = pool->start;
  poolFinish(pool);
  return s;
}

static
XML_Char *poolStoreString(STRING_POOL *pool, const ENCODING *enc,
                          const char *ptr, const char *end)
{
  if (!poolAppend(pool, enc, ptr, end))
    return 0;
  if (pool->ptr == pool->end && !poolGrow(pool))
    return 0;
  *(pool->ptr)++ = 0;
  return pool->start;
}

#define INIT_SIZE 64

static
int keyeq(KEY s1, KEY s2)
{
  for (; *s1 == *s2; s1++, s2++)
    if (*s1 == 0)
      return 1;
  return 0;
}

static
unsigned long hash(KEY s)
{
  unsigned long h = 0;
  while (*s)
    h = (h << 5) + h + (unsigned char)*s++;
  return h;
}

static
NAMED *lookup(HASH_TABLE *table, KEY name, size_t createSize)
{
  size_t i;
  if (table->size == 0) {
    if (!createSize)
      return 0;
    table->v = calloc(INIT_SIZE, sizeof(NAMED *));
    if (!table->v)
      return 0;
    table->size = INIT_SIZE;
    table->usedLim = INIT_SIZE / 2;
    i = hash(name) & (table->size - 1);
  }
  else {
    unsigned long h = hash(name);
    for (i = h & (table->size - 1);
         table->v[i];
         i == 0 ? i = table->size - 1 : --i) {
      if (keyeq(name, table->v[i]->name))
        return table->v[i];
    }
    if (!createSize)
      return 0;
    if (table->used == table->usedLim) {
      /* check for overflow */
      size_t newSize = table->size * 2;
      NAMED **newV = calloc(newSize, sizeof(NAMED *));
      if (!newV)
        return 0;
      for (i = 0; i < table->size; i++)
        if (table->v[i]) {
          size_t j;
          for (j = hash(table->v[i]->name) & (newSize - 1);
               newV[j];
               j == 0 ? j = newSize - 1 : --j)
            ;
          newV[j] = table->v[i];
        }
      free(table->v);
      table->v = newV;
      table->size = newSize;
      table->usedLim = newSize/2;
      for (i = h & (table->size - 1);
           table->v[i];
           i == 0 ? i = table->size - 1 : --i)
        ;
    }
  }
  table->v[i] = calloc(1, createSize);
  if (!table->v[i])
    return 0;
  table->v[i]->name = name;
  (table->used)++;
  return table->v[i];
}

static
void hashTableDestroy(HASH_TABLE *table)
{
  size_t i;
  for (i = 0; i < table->size; i++) {
    NAMED *p = table->v[i];
    if (p)
      free(p);
  }
  if (table->v)
    free(table->v);
}

static
void hashTableInit(HASH_TABLE *p)
{
  p->size = 0;
  p->usedLim = 0;
  p->used = 0;
  p->v = 0;
}

static
void hashTableIterInit(HASH_TABLE_ITER *iter, const HASH_TABLE *table)
{
  iter->p = table->v;
  iter->end = iter->p + table->size;
}

static
NAMED *hashTableIterNext(HASH_TABLE_ITER *iter)
{
  while (iter->p != iter->end) {
    NAMED *tem = *(iter->p)++;
    if (tem)
      return tem;
  }
  return 0;
}



static int dtdInit(DTD *p)
{
  poolInit(&(p->pool));
  hashTableInit(&(p->generalEntities));
  hashTableInit(&(p->elementTypes));
  hashTableInit(&(p->attributeIds));
  hashTableInit(&(p->prefixes));
  p->complete = 1;
  p->standalone = 0;
  hashTableInit(&(p->paramEntities));
  p->defaultPrefix.name = 0;
  p->defaultPrefix.binding = 0;
  return 1;
}



static void dtdSwap(DTD *p1, DTD *p2)
{
  DTD tem;
  memcpy(&tem, p1, sizeof(DTD));
  memcpy(p1, p2, sizeof(DTD));
  memcpy(p2, &tem, sizeof(DTD));
}




static void dtdDestroy(DTD *p)
{
  HASH_TABLE_ITER iter;
  hashTableIterInit(&iter, &(p->elementTypes));
  for (;;) {
    ELEMENT_TYPE *e = (ELEMENT_TYPE *)hashTableIterNext(&iter);
    if (!e)
      break;
    if (e->allocDefaultAtts != 0)
      free(e->defaultAtts);
  }
  hashTableDestroy(&(p->generalEntities));
  hashTableDestroy(&(p->paramEntities));
  hashTableDestroy(&(p->elementTypes));
  hashTableDestroy(&(p->attributeIds));
  hashTableDestroy(&(p->prefixes));
  poolDestroy(&(p->pool));
}

static int copyEntityTable(HASH_TABLE *newTable,
                           STRING_POOL *newPool,
                           const HASH_TABLE *oldTable)
{
  HASH_TABLE_ITER iter;
  const XML_Char *cachedOldBase = 0;
  const XML_Char *cachedNewBase = 0;

  hashTableIterInit(&iter, oldTable);

  for (;;) {
    ENTITY *newE;
    const XML_Char *name;
    const ENTITY *oldE = (ENTITY *)hashTableIterNext(&iter);
    if (!oldE)
      break;
    name = poolCopyString(newPool, oldE->name);
    if (!name)
      return 0;
    newE = (ENTITY *)lookup(newTable, name, sizeof(ENTITY));
    if (!newE)
      return 0;
    if (oldE->systemId) {
      const XML_Char *tem = poolCopyString(newPool, oldE->systemId);
      if (!tem)
        return 0;
      newE->systemId = tem;
      if (oldE->base) {
        if (oldE->base == cachedOldBase)
          newE->base = cachedNewBase;
        else {
          cachedOldBase = oldE->base;
          tem = poolCopyString(newPool, cachedOldBase);
          if (!tem)
            return 0;
          cachedNewBase = newE->base = tem;
        }
      }
    }
    else {
      const XML_Char *tem =
          poolCopyStringN(newPool, oldE->textPtr, oldE->textLen);
      if (!tem)
        return 0;
      newE->textPtr = tem;
      newE->textLen = oldE->textLen;
    }
    if (oldE->notation) {
      const XML_Char *tem = poolCopyString(newPool, oldE->notation);
      if (!tem)
        return 0;
      newE->notation = tem;
    }
  }
  return 1;
}



/* Do a deep copy of the DTD.  Return 0 for out of memory; non-zero otherwise.
The new DTD has already been initialized. */

static int dtdCopy(DTD *newDtd, const DTD *oldDtd)
{
  HASH_TABLE_ITER iter;

  /* Copy the prefix table. */

  hashTableIterInit(&iter, &(oldDtd->prefixes));
  for (;;) {
    const XML_Char *name;
    const PREFIX *oldP = (PREFIX *)hashTableIterNext(&iter);
    if (!oldP)
      break;
    name = poolCopyString(&(newDtd->pool), oldP->name);
    if (!name)
      return 0;
    if (!lookup(&(newDtd->prefixes), name, sizeof(PREFIX)))
      return 0;
  }

  hashTableIterInit(&iter, &(oldDtd->attributeIds));

  /* Copy the attribute id table. */

  for (;;) {
    ATTRIBUTE_ID *newA;
    const XML_Char *name;
    const ATTRIBUTE_ID *oldA = (ATTRIBUTE_ID *)hashTableIterNext(&iter);

    if (!oldA)
      break;
    /* Remember to allocate the scratch byte before the name. */
    if (!poolAppendChar(&(newDtd->pool), XML_T('\0')))
      return 0;
    name = poolCopyString(&(newDtd->pool), oldA->name);
    if (!name)
      return 0;
    ++name;
    newA = (ATTRIBUTE_ID *)
        lookup(&(newDtd->attributeIds), name, sizeof(ATTRIBUTE_ID));
    if (!newA)
      return 0;
    newA->maybeTokenized = oldA->maybeTokenized;
    if (oldA->prefix) {
      newA->xmlns = oldA->xmlns;
      if (oldA->prefix == &oldDtd->defaultPrefix)
        newA->prefix = &newDtd->defaultPrefix;
      else
        newA->prefix = (PREFIX *)
            lookup(&(newDtd->prefixes), oldA->prefix->name, 0);
    }
  }

  /* Copy the element type table. */

  hashTableIterInit(&iter, &(oldDtd->elementTypes));

  for (;;) {
    int i;
    ELEMENT_TYPE *newE;
    const XML_Char *name;
    const ELEMENT_TYPE *oldE = (ELEMENT_TYPE *)hashTableIterNext(&iter);
    if (!oldE)
      break;
    name = poolCopyString(&(newDtd->pool), oldE->name);
    if (!name)
      return 0;
    newE = (ELEMENT_TYPE *)
        lookup(&(newDtd->elementTypes), name, sizeof(ELEMENT_TYPE));
    if (!newE)
      return 0;
    if (oldE->nDefaultAtts) {
      newE->defaultAtts = (DEFAULT_ATTRIBUTE *)
          malloc(oldE->nDefaultAtts * sizeof(DEFAULT_ATTRIBUTE));
      if (!newE->defaultAtts)
        return 0;
    }
    if (oldE->idAtt)
      newE->idAtt = (ATTRIBUTE_ID *)
          lookup(&(newDtd->attributeIds), oldE->idAtt->name, 0);
    newE->allocDefaultAtts = newE->nDefaultAtts = oldE->nDefaultAtts;
    if (oldE->prefix)
      newE->prefix = (PREFIX *)
          lookup(&(newDtd->prefixes), oldE->prefix->name, 0);
    for (i = 0; i < newE->nDefaultAtts; i++) {
      newE->defaultAtts[i].id = (ATTRIBUTE_ID *)
          lookup(&(newDtd->attributeIds), oldE->defaultAtts[i].id->name, 0);
      newE->defaultAtts[i].isCdata = oldE->defaultAtts[i].isCdata;
      if (oldE->defaultAtts[i].value) {
        newE->defaultAtts[i].value =
            poolCopyString(&(newDtd->pool), oldE->defaultAtts[i].value);
        if (!newE->defaultAtts[i].value)
          return 0;
      }
      else
        newE->defaultAtts[i].value = 0;
    }
  }

  /* Copy the entity tables. */
  if (!copyEntityTable(&(newDtd->generalEntities),
                       &(newDtd->pool),
                       &(oldDtd->generalEntities)))
      return 0;

  if (!copyEntityTable(&(newDtd->paramEntities),
                       &(newDtd->pool),
                       &(oldDtd->paramEntities)))
      return 0;

  newDtd->complete = oldDtd->complete;
  newDtd->standalone = oldDtd->standalone;
  return 1;
}



static
int addBinding(XML_Parser parser,
               PREFIX *prefix,
               const ATTRIBUTE_ID *attId,
               const XML_Char *uri,
               BINDING **bindingsPtr)
{
  BINDING *b;
  int len;
  for (len = 0; uri[len]; len++)
    ;
  if (namespaceSeparator)
    len++;
  if (freeBindingList) {
    b = freeBindingList;
    if (len > b->uriAlloc) {
      b->uri = realloc(b->uri, sizeof(XML_Char) * (len + EXPAND_SPARE));
      if (!b->uri)
        return 0;
      b->uriAlloc = len + EXPAND_SPARE;
    }
    freeBindingList = b->nextTagBinding;
  }
  else {
    b = malloc(sizeof(BINDING));
    if (!b)
      return 0;
    b->uri = malloc(sizeof(XML_Char) * (len + EXPAND_SPARE));
    if (!b->uri) {
      free(b);
      return 0;
    }
    b->uriAlloc = len + EXPAND_SPARE;
  }
  b->uriLen = len;
  memcpy(b->uri, uri, len * sizeof(XML_Char));
  if (namespaceSeparator)
    b->uri[len - 1] = namespaceSeparator;
  b->prefix = prefix;
  b->attId = attId;
  b->prevPrefixBinding = prefix->binding;
  if (*uri == XML_T('\0') && prefix == &dtd.defaultPrefix)
    prefix->binding = 0;
  else
    prefix->binding = b;
  b->nextTagBinding = *bindingsPtr;
  *bindingsPtr = b;
  if (startNamespaceDeclHandler)
    startNamespaceDeclHandler(handlerArg, prefix->name,
                              prefix->binding ? uri : 0);
  return 1;
}



#define CONTEXT_SEP XML_T('\f')

static
const XML_Char *getContext(XML_Parser parser)
{
  HASH_TABLE_ITER iter;
  int needSep = 0;

  if (dtd.defaultPrefix.binding) {
    int i;
    int len;
    if (!poolAppendChar(&tempPool, XML_T('=')))
      return 0;
    len = dtd.defaultPrefix.binding->uriLen;
    if (namespaceSeparator != XML_T('\0'))
      len--;
    for (i = 0; i < len; i++)
      if (!poolAppendChar(&tempPool, dtd.defaultPrefix.binding->uri[i]))
        return 0;
    needSep = 1;
  }

  hashTableIterInit(&iter, &(dtd.prefixes));
  for (;;) {
    int i;
    int len;
    const XML_Char *s;
    PREFIX *prefix = (PREFIX *)hashTableIterNext(&iter);
    if (!prefix)
      break;
    if (!prefix->binding)
      continue;
    if (needSep && !poolAppendChar(&tempPool, CONTEXT_SEP))
      return 0;
    for (s = prefix->name; *s; s++)
      if (!poolAppendChar(&tempPool, *s))
        return 0;
    if (!poolAppendChar(&tempPool, XML_T('=')))
      return 0;
    len = prefix->binding->uriLen;
    if (namespaceSeparator != XML_T('\0'))
      len--;
    for (i = 0; i < len; i++)
      if (!poolAppendChar(&tempPool, prefix->binding->uri[i]))
        return 0;
    needSep = 1;
  }


  hashTableIterInit(&iter, &(dtd.generalEntities));
  for (;;) {
    const XML_Char *s;
    ENTITY *e = (ENTITY *)hashTableIterNext(&iter);
    if (!e)
      break;
    if (!e->open)
      continue;
    if (needSep && !poolAppendChar(&tempPool, CONTEXT_SEP))
      return 0;
    for (s = e->name; *s; s++)
      if (!poolAppendChar(&tempPool, *s))
        return 0;
    needSep = 1;
  }

  if (!poolAppendChar(&tempPool, XML_T('\0')))
    return 0;
  return tempPool.start;
}

static
int setContext(XML_Parser parser, const XML_Char *context)
{
  const XML_Char *s = context;

  while (*context != XML_T('\0')) {
    if (*s == CONTEXT_SEP || *s == XML_T('\0')) {
      ENTITY *e;
      if (!poolAppendChar(&tempPool, XML_T('\0')))
        return 0;
      e = (ENTITY *)lookup(&dtd.generalEntities, poolStart(&tempPool), 0);
      if (e)
        e->open = 1;
      if (*s != XML_T('\0'))
        s++;
      context = s;
      poolDiscard(&tempPool);
    }
    else if (*s == '=') {
      PREFIX *prefix;
      if (poolLength(&tempPool) == 0)
        prefix = &dtd.defaultPrefix;
      else {
        if (!poolAppendChar(&tempPool, XML_T('\0')))
          return 0;
        prefix = (PREFIX *)
            lookup(&dtd.prefixes, poolStart(&tempPool), sizeof(PREFIX));
        if (!prefix)
          return 0;
        if (prefix->name == poolStart(&tempPool)) {
          prefix->name = poolCopyString(&dtd.pool, prefix->name);
          if (!prefix->name)
            return 0;
        }
        poolDiscard(&tempPool);
      }
      for (context = s + 1;
           *context != CONTEXT_SEP && *context != XML_T('\0');
           ++context)
        if (!poolAppendChar(&tempPool, *context))
          return 0;
      if (!poolAppendChar(&tempPool, XML_T('\0')))
        return 0;
      if (!addBinding(parser, prefix, 0, poolStart(&tempPool),
                      &inheritedBindings))
        return 0;
      poolDiscard(&tempPool);
      if (*context != XML_T('\0'))
        ++context;
      s = context;
    }
    else {
      if (!poolAppendChar(&tempPool, *s))
        return 0;
      s++;
    }
  }
  return 1;
}



static void
normalizeLines(XML_Char *s)
{
  XML_Char *p;
  for (;; s++) {
    if (*s == XML_T('\0'))
      return;
    if (*s == 0xD)
      break;
  }
  p = s;
  do {
    if (*s == 0xD) {
      *p++ = 0xA;
      if (*++s == 0xA)
        s++;
    }
    else
      *p++ = *s++;
  } while (*s);
  *p = XML_T('\0');
}



static void
reportDefault(XML_Parser       const xmlParserP,
              const ENCODING * const enc,
              const char *     const start,
              const char *     const end) {

    Parser * const parser = (Parser *)xmlParserP;

    if (MUST_CONVERT(enc, start)) {
        const char * s;

        const char **eventPP;
        const char **eventEndPP;

        if (enc == parser->m_encoding) {
            eventPP = &eventPtr;
            eventEndPP = &eventEndPtr;
        }
        else {
            eventPP = &(openInternalEntities->internalEventPtr);
            eventEndPP = &(openInternalEntities->internalEventEndPtr);
        }
        s = start;
        do {
            ICHAR *dataPtr = (ICHAR *)dataBuf;
            XmlConvert(enc, &s, end, &dataPtr, (ICHAR *)dataBufEnd);
            *eventEndPP = s;
            {
                size_t const len = dataPtr - (ICHAR *)dataBuf;
                assert((size_t)(int)len == len);   /* parser requirement */
                defaultHandler(handlerArg, dataBuf, (int)len);
            }
            *eventPP = s;
        } while (s != end);
    } else {
        size_t const len = (XML_Char *)end - (XML_Char *)start;
        assert((size_t)(int)len == len);  /* parser requirement */
        defaultHandler(handlerArg, (XML_Char *)start, len);
    }
}



static int
reportProcessingInstruction(XML_Parser parser,
                            const ENCODING *enc,
                            const char *start,
                            const char *end)
{
  const XML_Char *target;
  XML_Char *data;
  const char *tem;
  if (!processingInstructionHandler) {
    if (defaultHandler)
      reportDefault(parser, enc, start, end);
    return 1;
  }
  start += enc->minBytesPerChar * 2;
  tem = start + XmlNameLength(enc, start);
  target = poolStoreString(&tempPool, enc, start, tem);
  if (!target)
    return 0;
  poolFinish(&tempPool);
  data = poolStoreString(&tempPool, enc,
                        XmlSkipS(enc, tem),
                        end - enc->minBytesPerChar*2);
  if (!data)
    return 0;
  normalizeLines(data);
  processingInstructionHandler(handlerArg, target, data);
  poolClear(&tempPool);
  return 1;
}

static int
reportComment(XML_Parser parser,
              const ENCODING *enc,
              const char *start,
              const char *end)
{
  XML_Char *data;
  if (!commentHandler) {
    if (defaultHandler)
      reportDefault(parser, enc, start, end);
    return 1;
  }
  data = poolStoreString(&tempPool,
                         enc,
                         start + enc->minBytesPerChar * 4, 
                         end - enc->minBytesPerChar * 3);
  if (!data)
    return 0;
  normalizeLines(data);
  commentHandler(handlerArg, data);
  poolClear(&tempPool);
  return 1;
}



static enum XML_Error
handleUnknownEncoding(XML_Parser const xmlParserP,
                      const XML_Char * const encodingName) {

    Parser * const parser = (Parser *) xmlParserP;

    if (unknownEncodingHandler) {
        XML_Encoding info;
        int i;
        for (i = 0; i < 256; i++)
            info.map[i] = -1;
        info.convert = 0;
        info.data = 0;
        info.release = 0;
        if (unknownEncodingHandler(unknownEncodingHandlerData,
                                   encodingName, &info)) {
            ENCODING *enc;
            unknownEncodingMem = malloc(xmlrpc_XmlSizeOfUnknownEncoding());
            if (!unknownEncodingMem) {
                if (info.release)
                    info.release(info.data);
                return XML_ERROR_NO_MEMORY;
            }
            enc = (ns
                   ? xmlrpc_XmlInitUnknownEncodingNS
                   : xmlrpc_XmlInitUnknownEncoding)(unknownEncodingMem,
                                             info.map,
                                             info.convert,
                                             info.data);
            if (enc) {
                unknownEncodingData = info.data;
                unknownEncodingRelease = info.release;
                parser->m_encoding = enc;
                return XML_ERROR_NONE;
            }
        }
        if (info.release)
            info.release(info.data);
    }
    return XML_ERROR_UNKNOWN_ENCODING;
}



static enum XML_Error
initializeEncoding(XML_Parser const xmlParserP) {

    Parser * const parser = (Parser *) xmlParserP;

    const char *s;
#ifdef XML_UNICODE
    char encodingBuf[128];
    if (!protocolEncodingName)
        s = 0;
    else {
        int i;
        for (i = 0; protocolEncodingName[i]; i++) {
            if (i == sizeof(encodingBuf) - 1
                || (protocolEncodingName[i] & ~0x7f) != 0) {
                encodingBuf[0] = '\0';
                break;
            }
            encodingBuf[i] = (char)protocolEncodingName[i];
        }
        encodingBuf[i] = '\0';
        s = encodingBuf;
    }
#else
    s = protocolEncodingName;
#endif
    if ((ns ? xmlrpc_XmlInitEncodingNS : xmlrpc_XmlInitEncoding)(
        &parser->m_initEncoding, &parser->m_encoding, s))
        return XML_ERROR_NONE;
    return handleUnknownEncoding(xmlParserP, protocolEncodingName);
}



static enum XML_Error
processXmlDecl(XML_Parser   const xmlParserP,
               int          const isGeneralTextEntity,
               const char * const s,
               const char * const next) {

    Parser * const parser = (Parser *) xmlParserP;

    const char *encodingName = 0;
    const ENCODING *newEncoding = 0;
    const char *version;
    int standalone = -1;
    if (!(ns
          ? xmlrpc_XmlParseXmlDeclNS
          : xmlrpc_XmlParseXmlDecl)(isGeneralTextEntity,
                             parser->m_encoding,
                             s,
                             next,
                             &eventPtr,
                             &version,
                             &encodingName,
                             &newEncoding,
                             &standalone))
        return XML_ERROR_SYNTAX;
    if (!isGeneralTextEntity && standalone == 1) {
        dtd.standalone = 1;
        if (paramEntityParsing == XML_PARAM_ENTITY_PARSING_UNLESS_STANDALONE)
            paramEntityParsing = XML_PARAM_ENTITY_PARSING_NEVER;
    }
    if (defaultHandler)
        reportDefault(xmlParserP, parser->m_encoding, s, next);
    if (!protocolEncodingName) {
        if (newEncoding) {
            if (newEncoding->minBytesPerChar !=
                parser->m_encoding->minBytesPerChar) {
                eventPtr = encodingName;
                return XML_ERROR_INCORRECT_ENCODING;
            }
            parser->m_encoding = newEncoding;
        }
        else if (encodingName) {
            enum XML_Error result;
            const XML_Char * s =
                poolStoreString(&tempPool,
                                parser->m_encoding,
                                encodingName,
                                encodingName
                                + XmlNameLength(parser->m_encoding,
                                                encodingName));
            if (!s)
                return XML_ERROR_NO_MEMORY;
            result = handleUnknownEncoding(xmlParserP, s);
            poolDiscard(&tempPool);
            if (result == XML_ERROR_UNKNOWN_ENCODING)
                eventPtr = encodingName;
            return result;
        }
    }
    return XML_ERROR_NONE;
}



static ATTRIBUTE_ID *
getAttributeId(XML_Parser parser,
               const ENCODING *enc,
               const char *start,
               const char *end)
{
  ATTRIBUTE_ID *id;
  const XML_Char *name;
  if (!poolAppendChar(&dtd.pool, XML_T('\0')))
    return 0;
  name = poolStoreString(&dtd.pool, enc, start, end);
  if (!name)
    return 0;
  ++name;
  id = (ATTRIBUTE_ID *)lookup(&dtd.attributeIds, name, sizeof(ATTRIBUTE_ID));
  if (!id)
    return 0;
  if (id->name != name)
    poolDiscard(&dtd.pool);
  else {
    poolFinish(&dtd.pool);
    if (!ns)
      ;
    else if (name[0] == 'x'
        && name[1] == 'm'
        && name[2] == 'l'
        && name[3] == 'n'
        && name[4] == 's'
        && (name[5] == XML_T('\0') || name[5] == XML_T(':'))) {
      if (name[5] == '\0')
        id->prefix = &dtd.defaultPrefix;
      else
        id->prefix = (PREFIX *)lookup(&dtd.prefixes, name + 6, sizeof(PREFIX));
      id->xmlns = 1;
    }
    else {
      int i;
      for (i = 0; name[i]; i++) {
        if (name[i] == XML_T(':')) {
          int j;
          for (j = 0; j < i; j++) {
            if (!poolAppendChar(&dtd.pool, name[j]))
              return 0;
          }
          if (!poolAppendChar(&dtd.pool, XML_T('\0')))
            return 0;
          id->prefix = (PREFIX *)
              lookup(&dtd.prefixes, poolStart(&dtd.pool), sizeof(PREFIX));
          if (id->prefix->name == poolStart(&dtd.pool))
            poolFinish(&dtd.pool);
          else
            poolDiscard(&dtd.pool);
          break;
        }
      }
    }
  }
  return id;
}

static
void normalizePublicId(XML_Char *publicId)
{
  XML_Char *p = publicId;
  XML_Char *s;
  for (s = publicId; *s; s++) {
    switch (*s) {
    case 0x20:
    case 0xD:
    case 0xA:
      if (p != publicId && p[-1] != 0x20)
        *p++ = 0x20;
      break;
    default:
      *p++ = *s;
    }
  }
  if (p != publicId && p[-1] == 0x20)
    --p;
  *p = XML_T('\0');
}



static int setElementTypePrefix(XML_Parser parser, ELEMENT_TYPE *elementType)
{
  const XML_Char *name;
  for (name = elementType->name; *name; name++) {
    if (*name == XML_T(':')) {
      PREFIX *prefix;
      const XML_Char *s;
      for (s = elementType->name; s != name; s++) {
        if (!poolAppendChar(&dtd.pool, *s))
          return 0;
      }
      if (!poolAppendChar(&dtd.pool, XML_T('\0')))
        return 0;
      prefix = (PREFIX *)
          lookup(&dtd.prefixes, poolStart(&dtd.pool), sizeof(PREFIX));
      if (!prefix)
        return 0;
      if (prefix->name == poolStart(&dtd.pool))
        poolFinish(&dtd.pool);
      else
        poolDiscard(&dtd.pool);
      elementType->prefix = prefix;

    }
  }
  return 1;
}



static enum XML_Error
appendAttributeValue(XML_Parser       const xmlParserP,
                     const ENCODING * const enc,
                     int              const isCdata,
                     const char *     const ptrArg,
                     const char *     const end,
                     STRING_POOL *    const pool) {

  Parser * const parser = (Parser *) xmlParserP;

  const char * ptr;

  ptr = ptrArg;

  for (;;) {
    const char *next;
    int tok = XmlAttributeValueTok(enc, ptr, end, &next);
    switch (tok) {
    case XML_TOK_NONE:
      return XML_ERROR_NONE;
    case XML_TOK_INVALID:
      if (enc == parser->m_encoding)
        eventPtr = next;
      return XML_ERROR_INVALID_TOKEN;
    case XML_TOK_PARTIAL:
      if (enc == parser->m_encoding)
        eventPtr = ptr;
      return XML_ERROR_INVALID_TOKEN;
    case XML_TOK_CHAR_REF:
      {
        XML_Char buf[XML_ENCODE_MAX];
        int i;
        int n = XmlCharRefNumber(enc, ptr);
        if (n < 0) {
          if (enc == parser->m_encoding)
            eventPtr = ptr;
          return XML_ERROR_BAD_CHAR_REF;
        }
        if (!isCdata
            && n == 0x20 /* space */
            && (poolLength(pool) == 0 || poolLastChar(pool) == 0x20))
          break;
        n = XmlEncode(n, (ICHAR *)buf);
        if (!n) {
          if (enc == parser->m_encoding)
            eventPtr = ptr;
          return XML_ERROR_BAD_CHAR_REF;
        }
        for (i = 0; i < n; i++) {
          if (!poolAppendChar(pool, buf[i]))
            return XML_ERROR_NO_MEMORY;
        }
      }
      break;
    case XML_TOK_DATA_CHARS:
      if (!poolAppend(pool, enc, ptr, next))
        return XML_ERROR_NO_MEMORY;
      break;
      break;
    case XML_TOK_TRAILING_CR:
      next = ptr + enc->minBytesPerChar;
      /* fall through */
    case XML_TOK_ATTRIBUTE_VALUE_S:
    case XML_TOK_DATA_NEWLINE:
      if (!isCdata && (poolLength(pool) == 0 || poolLastChar(pool) == 0x20))
        break;
      if (!poolAppendChar(pool, 0x20))
        return XML_ERROR_NO_MEMORY;
      break;
    case XML_TOK_ENTITY_REF:
      {
        const XML_Char *name;
        ENTITY *entity;
        XML_Char ch = XmlPredefinedEntityName(enc,
                                              ptr + enc->minBytesPerChar,
                                              next - enc->minBytesPerChar);
        if (ch) {
          if (!poolAppendChar(pool, ch))
            return XML_ERROR_NO_MEMORY;
          break;
        }
        name = poolStoreString(&temp2Pool, enc,
                               ptr + enc->minBytesPerChar,
                               next - enc->minBytesPerChar);
        if (!name)
          return XML_ERROR_NO_MEMORY;
        entity = (ENTITY *)lookup(&dtd.generalEntities, name, 0);
        poolDiscard(&temp2Pool);
        if (!entity) {
          if (dtd.complete) {
            if (enc == parser->m_encoding)
              eventPtr = ptr;
            return XML_ERROR_UNDEFINED_ENTITY;
          }
        }
        else if (entity->open) {
          if (enc == parser->m_encoding)
            eventPtr = ptr;
          return XML_ERROR_RECURSIVE_ENTITY_REF;
        }
        else if (entity->notation) {
          if (enc == parser->m_encoding)
            eventPtr = ptr;
          return XML_ERROR_BINARY_ENTITY_REF;
        }
        else if (!entity->textPtr) {
          if (enc == parser->m_encoding)
            eventPtr = ptr;
          return XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF;
        }
        else {
          enum XML_Error result;
          const XML_Char *textEnd = entity->textPtr + entity->textLen;
          entity->open = 1;
          result = appendAttributeValue(xmlParserP, internalEncoding,
                                        isCdata, (char *)entity->textPtr,
                                        (char *)textEnd, pool);
          entity->open = 0;
          if (result)
            return result;
        }
      }
      break;
    default:
      abort();
    }
    ptr = next;
  }
  /* not reached */
}



static enum XML_Error
storeAttributeValue(XML_Parser parser, const ENCODING *enc, int isCdata,
                    const char *ptr, const char *end,
                    STRING_POOL *pool)
{
  enum XML_Error result =
      appendAttributeValue(parser, enc, isCdata, ptr, end, pool);
  if (result)
    return result;
  if (!isCdata && poolLength(pool) && poolLastChar(pool) == 0x20)
    poolChop(pool);
  if (!poolAppendChar(pool, XML_T('\0')))
    return XML_ERROR_NO_MEMORY;
  return XML_ERROR_NONE;
}



static
enum XML_Error
storeEntityValue(XML_Parser       const xmlParserP,
                 const ENCODING * const enc,
                 const char *     const entityTextPtrArg,
                 const char *     const entityTextEnd) {

  Parser * const parser = (Parser *) xmlParserP;

  STRING_POOL * const pool = &(dtd.pool);
  const char * entityTextPtr;

  entityTextPtr = entityTextPtrArg;

  for (;;) {
    const char * next;
    int tok;

    tok = XmlEntityValueTok(enc, entityTextPtr, entityTextEnd, &next);
    switch (tok) {
    case XML_TOK_PARAM_ENTITY_REF:
      if (parentParser || enc != parser->m_encoding) {
        enum XML_Error result;
        const XML_Char *name;
        ENTITY *entity;
        name = poolStoreString(&tempPool, enc,
                               entityTextPtr + enc->minBytesPerChar,
                               next - enc->minBytesPerChar);
        if (!name)
          return XML_ERROR_NO_MEMORY;
        entity = (ENTITY *)lookup(&dtd.paramEntities, name, 0);
        poolDiscard(&tempPool);
        if (!entity) {
          if (enc == parser->m_encoding)
            eventPtr = entityTextPtr;
          return XML_ERROR_UNDEFINED_ENTITY;
        }
        if (entity->open) {
          if (enc == parser->m_encoding)
            eventPtr = entityTextPtr;
          return XML_ERROR_RECURSIVE_ENTITY_REF;
        }
        if (entity->systemId) {
          if (enc == parser->m_encoding)
            eventPtr = entityTextPtr;
          return XML_ERROR_PARAM_ENTITY_REF;
        }
        entity->open = 1;
        result = storeEntityValue(parser,
                                  internalEncoding,
                                  (char *)entity->textPtr,
                                  (char *)(entity->textPtr + entity->textLen));
        entity->open = 0;
        if (result)
          return result;
        break;
      }
      eventPtr = entityTextPtr;
      return XML_ERROR_SYNTAX;
    case XML_TOK_NONE:
      return XML_ERROR_NONE;
    case XML_TOK_ENTITY_REF:
    case XML_TOK_DATA_CHARS:
      if (!poolAppend(pool, enc, entityTextPtr, next))
        return XML_ERROR_NO_MEMORY;
      break;
    case XML_TOK_TRAILING_CR:
      next = entityTextPtr + enc->minBytesPerChar;
      /* fall through */
    case XML_TOK_DATA_NEWLINE:
      if (pool->end == pool->ptr && !poolGrow(pool))
        return XML_ERROR_NO_MEMORY;
      *(pool->ptr)++ = 0xA;
      break;
    case XML_TOK_CHAR_REF:
      {
        XML_Char buf[XML_ENCODE_MAX];
        int i;
        int n = XmlCharRefNumber(enc, entityTextPtr);
        if (n < 0) {
          if (enc == parser->m_encoding)
            eventPtr = entityTextPtr;
          return XML_ERROR_BAD_CHAR_REF;
        }
        n = XmlEncode(n, (ICHAR *)buf);
        if (!n) {
          if (enc == parser->m_encoding)
            eventPtr = entityTextPtr;
          return XML_ERROR_BAD_CHAR_REF;
        }
        for (i = 0; i < n; i++) {
          if (pool->end == pool->ptr && !poolGrow(pool))
            return XML_ERROR_NO_MEMORY;
          *(pool->ptr)++ = buf[i];
        }
      }
      break;
    case XML_TOK_PARTIAL:
      if (enc == parser->m_encoding)
        eventPtr = entityTextPtr;
      return XML_ERROR_INVALID_TOKEN;
    case XML_TOK_INVALID:
      if (enc == parser->m_encoding)
        eventPtr = next;
      return XML_ERROR_INVALID_TOKEN;
    default:
      abort();
    }
    entityTextPtr = next;
  }
  /* not reached */
}



static int
defineAttribute(ELEMENT_TYPE *type,
                ATTRIBUTE_ID *attId,
                int isCdata,
                int isId,
                const XML_Char *value)
{
  DEFAULT_ATTRIBUTE *att;
  if (value || isId) {
    /* The handling of default attributes gets messed up if we have
       a default which duplicates a non-default. */
    int i;
    for (i = 0; i < type->nDefaultAtts; i++)
      if (attId == type->defaultAtts[i].id)
        return 1;
    if (isId && !type->idAtt && !attId->xmlns)
      type->idAtt = attId;
  }
  if (type->nDefaultAtts == type->allocDefaultAtts) {
    if (type->allocDefaultAtts == 0) {
      type->allocDefaultAtts = 8;
      type->defaultAtts =
          malloc(type->allocDefaultAtts*sizeof(DEFAULT_ATTRIBUTE));
    }
    else {
      type->allocDefaultAtts *= 2;
      type->defaultAtts =
          realloc(type->defaultAtts,
                  type->allocDefaultAtts*sizeof(DEFAULT_ATTRIBUTE));
    }
    if (!type->defaultAtts)
      return 0;
  }
  att = type->defaultAtts + type->nDefaultAtts;
  att->id = attId;
  att->value = value;
  att->isCdata = isCdata;
  if (!isCdata)
    attId->maybeTokenized = 1;
  type->nDefaultAtts += 1;
  return 1;
}



/* If tagNamePtr is non-null, build a real list of attributes,
otherwise just check the attributes for well-formedness. */

static enum XML_Error
storeAtts(XML_Parser       const xmlParserP,
          const ENCODING * const enc,
          const char *     const attStr,
          TAG_NAME *       const tagNamePtr,
          BINDING **       const bindingsPtr) {

  Parser * const parser = (Parser *)xmlParserP;

  ELEMENT_TYPE *elementType = 0;
  int nDefaultAtts = 0;
  const XML_Char ** appAtts;
      /* the attribute list to pass to the application */
  int attIndex = 0;
  int i;
  int n;
  int nPrefixes = 0;
  BINDING *binding;
  const XML_Char *localPart;

  /* lookup the element type name */
  if (tagNamePtr) {
    elementType = (ELEMENT_TYPE *)
        lookup(&dtd.elementTypes, tagNamePtr->str, 0);
    if (!elementType) {
      tagNamePtr->str = poolCopyString(&dtd.pool, tagNamePtr->str);
      if (!tagNamePtr->str)
        return XML_ERROR_NO_MEMORY;
      elementType = (ELEMENT_TYPE *)
          lookup(&dtd.elementTypes, tagNamePtr->str, sizeof(ELEMENT_TYPE));
      if (!elementType)
        return XML_ERROR_NO_MEMORY;
      if (ns && !setElementTypePrefix(xmlParserP, elementType))
        return XML_ERROR_NO_MEMORY;
    }
    nDefaultAtts = elementType->nDefaultAtts;
  }
  /* get the attributes from the tokenizer */
  n = XmlGetAttributes(enc, attStr, attsSize, atts);
  if (n + nDefaultAtts > attsSize) {
    int oldAttsSize = attsSize;
    attsSize = n + nDefaultAtts + INIT_ATTS_SIZE;
    atts = realloc((void *)atts, attsSize * sizeof(ATTRIBUTE));
    if (!atts)
      return XML_ERROR_NO_MEMORY;
    if (n > oldAttsSize)
      XmlGetAttributes(enc, attStr, n, atts);
  }
  appAtts = (const XML_Char **)atts;
  for (i = 0; i < n; i++) {
    /* add the name and value to the attribute list */
    ATTRIBUTE_ID *attId = getAttributeId(xmlParserP, enc, atts[i].name,
                                         atts[i].name
                                         + XmlNameLength(enc, atts[i].name));
    if (!attId)
      return XML_ERROR_NO_MEMORY;
    /* detect duplicate attributes */
    if ((attId->name)[-1]) {
      if (enc == parser->m_encoding)
        eventPtr = atts[i].name;
      return XML_ERROR_DUPLICATE_ATTRIBUTE;
    }
    (attId->name)[-1] = 1;
    appAtts[attIndex++] = attId->name;
    if (!atts[i].normalized) {
      enum XML_Error result;
      int isCdata = 1;

      /* figure out whether declared as other than CDATA */
      if (attId->maybeTokenized) {
        int j;
        for (j = 0; j < nDefaultAtts; j++) {
          if (attId == elementType->defaultAtts[j].id) {
            isCdata = elementType->defaultAtts[j].isCdata;
            break;
          }
        }
      }

      /* normalize the attribute value */
      result = storeAttributeValue(xmlParserP, enc, isCdata,
                                   atts[i].valuePtr, atts[i].valueEnd,
                                   &tempPool);
      if (result)
        return result;
      if (tagNamePtr) {
        appAtts[attIndex] = poolStart(&tempPool);
        poolFinish(&tempPool);
      }
      else
        poolDiscard(&tempPool);
    }
    else if (tagNamePtr) {
      /* the value did not need normalizing */
      appAtts[attIndex] =
          poolStoreString(&tempPool, enc, atts[i].valuePtr, atts[i].valueEnd);
      if (appAtts[attIndex] == 0)
        return XML_ERROR_NO_MEMORY;
      poolFinish(&tempPool);
    }
    /* handle prefixed attribute names */
    if (attId->prefix && tagNamePtr) {
      if (attId->xmlns) {
        /* deal with namespace declarations here */
        if (!addBinding(xmlParserP, attId->prefix, attId, appAtts[attIndex],
                        bindingsPtr))
          return XML_ERROR_NO_MEMORY;
        --attIndex;
      }
      else {
        /* deal with other prefixed names later */
        attIndex++;
        nPrefixes++;
        (attId->name)[-1] = 2;
      }
    }
    else
      attIndex++;
  }
  if (tagNamePtr) {
    int j;
    nSpecifiedAtts = attIndex;
    if (elementType->idAtt && (elementType->idAtt->name)[-1]) {
      for (i = 0; i < attIndex; i += 2)
        if (appAtts[i] == elementType->idAtt->name) {
          idAttIndex = i;
          break;
        }
    }
    else
      idAttIndex = -1;
    /* do attribute defaulting */
    for (j = 0; j < nDefaultAtts; j++) {
      const DEFAULT_ATTRIBUTE *da = elementType->defaultAtts + j;
      if (!(da->id->name)[-1] && da->value) {
        if (da->id->prefix) {
          if (da->id->xmlns) {
            if (!addBinding(xmlParserP, da->id->prefix, da->id, da->value,
                            bindingsPtr))
              return XML_ERROR_NO_MEMORY;
          }
          else {
            (da->id->name)[-1] = 2;
            nPrefixes++;
            appAtts[attIndex++] = da->id->name;
            appAtts[attIndex++] = da->value;
          }
        }
        else {
          (da->id->name)[-1] = 1;
          appAtts[attIndex++] = da->id->name;
          appAtts[attIndex++] = da->value;
        }
      }
    }
    appAtts[attIndex] = 0;
  }
  i = 0;
  if (nPrefixes) {
    /* expand prefixed attribute names */
    for (; i < attIndex; i += 2) {
      if (appAtts[i][-1] == 2) {
        ATTRIBUTE_ID *id;
        ((XML_Char *)(appAtts[i]))[-1] = 0;
        id = (ATTRIBUTE_ID *)lookup(&dtd.attributeIds, appAtts[i], 0);
        if (id->prefix->binding) {
          int j;
          const BINDING *b = id->prefix->binding;
          const XML_Char *s = appAtts[i];
          for (j = 0; j < b->uriLen; j++) {
            if (!poolAppendChar(&tempPool, b->uri[j]))
              return XML_ERROR_NO_MEMORY;
          }
          while (*s++ != ':')
            ;
          do {
            if (!poolAppendChar(&tempPool, *s))
              return XML_ERROR_NO_MEMORY;
          } while (*s++);
          appAtts[i] = poolStart(&tempPool);
          poolFinish(&tempPool);
        }
        if (!--nPrefixes)
          break;
      }
      else
        ((XML_Char *)(appAtts[i]))[-1] = 0;
    }
  }
  /* clear the flags that say whether attributes were specified */
  for (; i < attIndex; i += 2)
    ((XML_Char *)(appAtts[i]))[-1] = 0;
  if (!tagNamePtr)
    return XML_ERROR_NONE;
  for (binding = *bindingsPtr; binding; binding = binding->nextTagBinding)
    binding->attId->name[-1] = 0;
  /* expand the element type name */
  if (elementType->prefix) {
    binding = elementType->prefix->binding;
    if (!binding)
      return XML_ERROR_NONE;
    localPart = tagNamePtr->str;
    while (*localPart++ != XML_T(':'))
      ;
  }
  else if (dtd.defaultPrefix.binding) {
    binding = dtd.defaultPrefix.binding;
    localPart = tagNamePtr->str;
  }
  else
    return XML_ERROR_NONE;
  tagNamePtr->localPart = localPart;
  tagNamePtr->uriLen = binding->uriLen;
  for (i = 0; localPart[i++];)
    ;
  n = i + binding->uriLen;
  if (n > binding->uriAlloc) {
    TAG *p;
    XML_Char *uri = malloc((n + EXPAND_SPARE) * sizeof(XML_Char));
    if (!uri)
      return XML_ERROR_NO_MEMORY;
    binding->uriAlloc = n + EXPAND_SPARE;
    memcpy(uri, binding->uri, binding->uriLen * sizeof(XML_Char));
    for (p = tagStack; p; p = p->parent)
      if (p->name.str == binding->uri)
        p->name.str = uri;
    free(binding->uri);
    binding->uri = uri;
  }
  memcpy(binding->uri + binding->uriLen, localPart, i * sizeof(XML_Char));
  tagNamePtr->str = binding->uri;
  return XML_ERROR_NONE;
}



static Processor epilogProcessor;

static void
epilogProcessor(XML_Parser       const xmlParserP,
                const char *     const startArg,
                const char *     const end,
                const char **    const nextPtr,
                enum XML_Error * const errorCodeP,
                const char **    const errorP) {
    
    Parser * const parser = (Parser *) xmlParserP;

    const char * s;

    *errorP = NULL;

    s = startArg;

    processor = epilogProcessor;
    eventPtr = s;
    for (;;) {
        const char *next;
        int tok = XmlPrologTok(parser->m_encoding, s, end, &next);
        eventEndPtr = next;
        switch (tok) {
        case -XML_TOK_PROLOG_S:
            if (defaultHandler) {
                eventEndPtr = end;
                reportDefault(xmlParserP, parser->m_encoding, s, end);
            }
            /* fall through */
        case XML_TOK_NONE:
            if (nextPtr)
                *nextPtr = end;
            *errorCodeP = XML_ERROR_NONE;
            return;
        case XML_TOK_PROLOG_S:
            if (defaultHandler)
                reportDefault(xmlParserP, parser->m_encoding, s, next);
            break;
        case XML_TOK_PI:
            if (!reportProcessingInstruction(xmlParserP, parser->m_encoding,
                                             s, next)) {
                *errorCodeP = XML_ERROR_NO_MEMORY;
                return;
            }
            break;
        case XML_TOK_COMMENT:
            if (!reportComment(xmlParserP, parser->m_encoding, s, next)) {
                *errorCodeP = XML_ERROR_NO_MEMORY;
                return;
            }
            break;
        case XML_TOK_INVALID:
            eventPtr = next;
            *errorCodeP = XML_ERROR_INVALID_TOKEN;
            return;
        case XML_TOK_PARTIAL:
            if (nextPtr) {
                *nextPtr = s;
                *errorCodeP = XML_ERROR_NONE;
            } else
                *errorCodeP = XML_ERROR_UNCLOSED_TOKEN;
            return;
        case XML_TOK_PARTIAL_CHAR:
            if (nextPtr) {
                *nextPtr = s;
                *errorCodeP = XML_ERROR_NONE;
            } else
                *errorCodeP = XML_ERROR_PARTIAL_CHAR;
            return;
        default:
            *errorCodeP = XML_ERROR_JUNK_AFTER_DOC_ELEMENT;
            return;
        }
        eventPtr = s = next;
    }
}



static enum XML_Error
doCdataSection(XML_Parser       const xmlParserP,
               const ENCODING * const enc,
               const char **    const startPtr,
               const char *     const end,
               const char **    const nextPtr) {

  Parser * const parser = (Parser *) xmlParserP;

  const char *s = *startPtr;
  const char **eventPP;
  const char **eventEndPP;
  if (enc == parser->m_encoding) {
    eventPP = &eventPtr;
    *eventPP = s;
    eventEndPP = &eventEndPtr;
  }
  else {
    eventPP = &(openInternalEntities->internalEventPtr);
    eventEndPP = &(openInternalEntities->internalEventEndPtr);
  }
  *eventPP = s;
  *startPtr = 0;
  for (;;) {
    const char *next;
    int tok = XmlCdataSectionTok(enc, s, end, &next);
    *eventEndPP = next;
    switch (tok) {
    case XML_TOK_CDATA_SECT_CLOSE:
      if (endCdataSectionHandler)
        endCdataSectionHandler(handlerArg);
      else if (defaultHandler)
        reportDefault(xmlParserP, enc, s, next);
      *startPtr = next;
      return XML_ERROR_NONE;
    case XML_TOK_DATA_NEWLINE:
      if (characterDataHandler) {
        XML_Char c = 0xA;
        characterDataHandler(handlerArg, &c, 1);
      }
      else if (defaultHandler)
        reportDefault(xmlParserP, enc, s, next);
      break;
    case XML_TOK_DATA_CHARS:
      if (characterDataHandler) {
        if (MUST_CONVERT(enc, s)) {
          for (;;) {
            ICHAR *dataPtr = (ICHAR *)dataBuf;
            XmlConvert(enc, &s, next, &dataPtr, (ICHAR *)dataBufEnd);
            *eventEndPP = next;
            {
                size_t const len = dataPtr - (ICHAR *)dataBuf;
                assert((size_t)(int)len == len);   /* parser requirement */
                characterDataHandler(handlerArg, dataBuf, (int)len);
            }
            if (s == next)
              break;
            *eventPP = s;
          }
        }
        else {
            size_t const len = (XML_Char *)next - (XML_Char *)s;
            assert((size_t)(int)len == len);   /* parser requirement */
            characterDataHandler(handlerArg, (XML_Char *)s, (int)len);
        }                               
      }
      else if (defaultHandler)
        reportDefault(xmlParserP, enc, s, next);
      break;
    case XML_TOK_INVALID:
      *eventPP = next;
      return XML_ERROR_INVALID_TOKEN;
    case XML_TOK_PARTIAL_CHAR:
      if (nextPtr) {
        *nextPtr = s;
        return XML_ERROR_NONE;
      }
      return XML_ERROR_PARTIAL_CHAR;
    case XML_TOK_PARTIAL:
    case XML_TOK_NONE:
      if (nextPtr) {
        *nextPtr = s;
        return XML_ERROR_NONE;
      }
      return XML_ERROR_UNCLOSED_CDATA_SECTION;
    default:
      abort();
    }
    *eventPP = s = next;
  }
  /* not reached */
}



/* Forward declaration for recursive reference: */
static void
doContent(XML_Parser       const xmlParserP,
          int              const startTagLevel,
          const ENCODING * const enc,
          const char *     const startArg,
          const char *     const end,
          const char **    const nextPtr,
          enum XML_Error * const errorCodeP,
          const char **    const errorP);


static Processor contentProcessor;

static void
contentProcessor(XML_Parser       const xmlParserP,
                 const char *     const start,
                 const char *     const end,
                 const char **    const endPtr,
                 enum XML_Error * const errorCodeP,
                 const char **    const errorP) {

    Parser * const parser = (Parser *) xmlParserP;

    const char * error;

    parser->m_errorString = NULL;

    doContent(xmlParserP, 0, parser->m_encoding, start, end, endPtr,
              errorCodeP, &error);

    if (*errorCodeP != XML_ERROR_NONE) {
        if (error) {
            xmlrpc_asprintf(errorP, "Invalid XML \"content\".  %s", error);
    
            xmlrpc_strfree(error);
        } else {
            const char * const sampleXml = extractXmlSample(start, end, 40);

            xmlrpc_asprintf(errorP, "Invalid XML \"content\" starting "
                            "with '%s'.  %s",
                            sampleXml,
                            xmlrpc_XML_ErrorString(*errorCodeP));

            xmlrpc_strfree(sampleXml);
        }
    } else
        *errorP = NULL;
}



/* The idea here is to avoid using stack for each CDATA section when
the whole file is parsed with one call. */



static Processor cdataSectionProcessor;

static void
cdataSectionProcessor(XML_Parser       const xmlParserP,
                      const char *     const startArg,
                      const char *     const end,
                      const char **    const endPtr,
                      enum XML_Error * const errorCodeP,
                      const char **    const errorP) {
    
    Parser * const parser = (Parser *) xmlParserP;
    
    enum XML_Error result;
    const char * start;

    start = startArg;

    result =
        doCdataSection(xmlParserP, parser->m_encoding, &start, end, endPtr);

    if (start) {
        processor = contentProcessor;
        contentProcessor(xmlParserP, start, end, endPtr, errorCodeP, errorP);
    } else {
        *errorCodeP = result;
        *errorP = NULL;
    }
}



static void
doEntityRef(XML_Parser       const xmlParserP,
            const ENCODING * const enc,
            const char *     const s,
            const char *     const next,
            enum XML_Error * const errorCodeP,
            const char **    const errorP) {
            
    Parser * const parser = (Parser *) xmlParserP;

    XML_Char const ch = XmlPredefinedEntityName(enc,
                                                s + enc->minBytesPerChar,
                                                next - enc->minBytesPerChar);
    const XML_Char *name;
    ENTITY *entity;
    *errorP = NULL;

    if (ch) {
        if (characterDataHandler)
            characterDataHandler(handlerArg, &ch, 1);
        else if (defaultHandler)
            reportDefault(xmlParserP, enc, s, next);
        *errorCodeP = XML_ERROR_NONE;
        return;
    }
    name = poolStoreString(&dtd.pool, enc,
                           s + enc->minBytesPerChar,
                           next - enc->minBytesPerChar);
    if (!name) {
        *errorCodeP = XML_ERROR_NO_MEMORY;
        return;
    }
    entity = (ENTITY *)lookup(&dtd.generalEntities, name, 0);
    poolDiscard(&dtd.pool);
    if (!entity) {
        if (dtd.complete || dtd.standalone)
            *errorCodeP = XML_ERROR_UNDEFINED_ENTITY;
        else {
            if (defaultHandler)
                reportDefault(xmlParserP, enc, s, next);
            *errorCodeP = XML_ERROR_NONE;
        }
        return;
    }
    if (entity->open) {
        *errorCodeP = XML_ERROR_RECURSIVE_ENTITY_REF;
        return;
    }
    if (entity->notation) {
        *errorCodeP = XML_ERROR_BINARY_ENTITY_REF;
        return;
    }
    if (entity) {
        if (entity->textPtr) {
            OPEN_INTERNAL_ENTITY openEntity;
            if (defaultHandler && !defaultExpandInternalEntities) {
                reportDefault(xmlParserP, enc, s, next);
                *errorCodeP = XML_ERROR_NONE;
                return;
            }
            entity->open = 1;
            openEntity.next = openInternalEntities;
            openInternalEntities = &openEntity;
            openEntity.entity = entity;
            openEntity.internalEventPtr = 0;
            openEntity.internalEventEndPtr = 0;
            doContent(xmlParserP,
                      tagLevel,
                      internalEncoding,
                      (char *)entity->textPtr,
                      (char *)(entity->textPtr + entity->textLen),
                      0, errorCodeP, errorP);
            entity->open = 0;
            openInternalEntities = openEntity.next;
            if (*errorCodeP != XML_ERROR_NONE)
                return;
        } else if (externalEntityRefHandler) {
            const XML_Char *context;
            entity->open = 1;
            context = getContext(xmlParserP);
            entity->open = 0;
            if (!context) {
                *errorCodeP = XML_ERROR_NO_MEMORY;
                return;
            }
            if (!externalEntityRefHandler(externalEntityRefHandlerArg,
                                          context,
                                          entity->base,
                                          entity->systemId,
                                          entity->publicId)) {
                *errorCodeP = XML_ERROR_EXTERNAL_ENTITY_HANDLING;
                return;
            }
            poolDiscard(&tempPool);
        } else if (defaultHandler)
            reportDefault(xmlParserP, enc, s, next);
    }
    *errorCodeP = XML_ERROR_NONE;
}



static void
doStartTagNoAtts(XML_Parser       const xmlParserP,
                 const ENCODING * const enc,
                 const char *     const s,
                 const char *     const next,
                 const char **    const nextPtr,
                 enum XML_Error * const errorCodeP,
                 const char **    const errorP) {

    Parser * const parser = (Parser *) xmlParserP;

    TAG *tag;

    *errorP = NULL;

    if (freeTagList) {
        tag = freeTagList;
        freeTagList = freeTagList->parent;
    } else {
        tag = malloc(sizeof(TAG));
        if (!tag) {
            *errorCodeP = XML_ERROR_NO_MEMORY;
            return;
        }
        tag->buf = malloc(INIT_TAG_BUF_SIZE);
        if (!tag->buf) {
            free(tag);
            *errorCodeP = XML_ERROR_NO_MEMORY;
            return;
        }
        tag->bufEnd = tag->buf + INIT_TAG_BUF_SIZE;
    }
    tag->bindings = NULL;
    tag->parent = tagStack;
    tagStack = tag;
    tag->name.localPart = 0;
    tag->rawName = s + enc->minBytesPerChar;
    tag->rawNameLength = XmlNameLength(enc, tag->rawName);
    if (nextPtr) {
        /* Need to guarantee that: tag->buf +
           ROUND_UP(tag->rawNameLength, sizeof(XML_Char)) <=
           tag->bufEnd - sizeof(XML_Char)
        */

        if (tag->rawNameLength +
            (int)(sizeof(XML_Char) - 1) +
            (int)sizeof(XML_Char) > tag->bufEnd - tag->buf) {
            int bufSize = tag->rawNameLength * 4;
            bufSize = ROUND_UP(bufSize, sizeof(XML_Char));
            tag->buf = realloc(tag->buf, bufSize);
            if (!tag->buf) {
                *errorCodeP = XML_ERROR_NO_MEMORY;
                return;
            }
            tag->bufEnd = tag->buf + bufSize;
        }
        memcpy(tag->buf, tag->rawName, tag->rawNameLength);
        tag->rawName = tag->buf;
    }
    ++tagLevel;
    if (startElementHandler) {
        enum XML_Error result;
        XML_Char *toPtr;
        for (;;) {
            const char *rawNameEnd = tag->rawName + tag->rawNameLength;
            const char *fromPtr = tag->rawName;
            if (nextPtr)
                toPtr = (XML_Char *)
                    (tag->buf + ROUND_UP(tag->rawNameLength,
                                         sizeof(XML_Char)));
            else
                toPtr = (XML_Char *)tag->buf;
            tag->name.str = toPtr;
            XmlConvert(enc,
                       &fromPtr, rawNameEnd,
                       (ICHAR **)&toPtr, (ICHAR *)tag->bufEnd - 1);
            if (fromPtr == rawNameEnd)
                break;
            else {
                size_t const bufSize = (tag->bufEnd - tag->buf) << 1;
                tag->buf = realloc(tag->buf, bufSize);
                if (!tag->buf) {
                    *errorCodeP = XML_ERROR_NO_MEMORY;
                    return;
                }
                tag->bufEnd = tag->buf + bufSize;
                if (nextPtr)
                    tag->rawName = tag->buf;
            }
        }
        *toPtr = XML_T('\0');
        result = storeAtts(xmlParserP, enc, s,
                           &(tag->name), &(tag->bindings));
        if (result) {
            *errorCodeP = result;
            return;
        }
        startElementHandler(handlerArg, tag->name.str,
                            (const XML_Char **)atts);
        poolClear(&tempPool);
    } else {
        tag->name.str = 0;
        if (defaultHandler)
            reportDefault(xmlParserP, enc, s, next);
    }
}



static void
doEmptyElementNoAtts(XML_Parser       const xmlParserP,
                     const ENCODING * const enc,
                     const char *     const s,
                     const char *     const end,
                     const char *     const next,
                     const char **    const nextPtr,
                     const char **    const eventPP,
                     const char **    const eventEndPP,
                     bool *           const doneP,
                     enum XML_Error * const errorCodeP,
                     const char **    const errorP) {
    
    Parser * const parser = (Parser *) xmlParserP;

    if (startElementHandler || endElementHandler) {
        const char * const rawName = s + enc->minBytesPerChar;

        enum XML_Error result;
        BINDING * bindings;
        TAG_NAME name;

        bindings = NULL;  /* initial value */
        name.str = poolStoreString(&tempPool, enc, rawName,
                                   rawName + XmlNameLength(enc, rawName));
        if (!name.str) {
            *errorCodeP = XML_ERROR_NO_MEMORY;
            return;
        }
        poolFinish(&tempPool);
        result = storeAtts(xmlParserP, enc, s, &name, &bindings);
        if (result) {
            *errorCodeP = result;
            return;
        }
        poolFinish(&tempPool);
        if (startElementHandler)
            startElementHandler(handlerArg, name.str, (const XML_Char **)atts);
        if (endElementHandler) {
            if (startElementHandler)
                *eventPP = *eventEndPP;
            endElementHandler(handlerArg, name.str);
        }
        poolClear(&tempPool);
        while (bindings) {
            BINDING * const b = bindings;
            if (endNamespaceDeclHandler)
                endNamespaceDeclHandler(handlerArg, b->prefix->name);
            bindings = bindings->nextTagBinding;
            b->nextTagBinding = freeBindingList;
            freeBindingList = b;
            b->prefix->binding = b->prevPrefixBinding;
        }
    } else if (defaultHandler)
        reportDefault(xmlParserP, enc, s, next);

    if (tagLevel == 0) {
        epilogProcessor(xmlParserP, next, end, nextPtr, errorCodeP, errorP);
        *doneP = true;
    } else
        *doneP = false;
}



static void
doEndTag(XML_Parser       const xmlParserP,
         const ENCODING * const enc,
         const char *     const s,
         const char *     const end,
         const char *     const next,
         const char **    const nextPtr,
         int              const startTagLevel,
         const char **    const eventPP,
         bool *           const doneP,
         enum XML_Error * const errorCodeP,
         const char **    const errorP) {

    Parser * const parser = (Parser *) xmlParserP;

    if (tagLevel == startTagLevel)
        *errorCodeP = XML_ERROR_ASYNC_ENTITY;
    else {
        TAG * const tag = tagStack;

        int len;
        const char * rawName;

        tagStack = tag->parent;
        tag->parent = freeTagList;
        freeTagList = tag;
        rawName = s + enc->minBytesPerChar*2;
        len = XmlNameLength(enc, rawName);
        if (len != tag->rawNameLength
            || memcmp(tag->rawName, rawName, len) != 0) {
            *eventPP = rawName;
            *errorCodeP = XML_ERROR_TAG_MISMATCH;
        } else {
            --tagLevel;
            if (endElementHandler && tag->name.str) {
                if (tag->name.localPart) {
                    XML_Char * to;
                    const XML_Char * from;
                    to   = (XML_Char *)tag->name.str + tag->name.uriLen;
                    from = tag->name.localPart;
                    while ((*to++ = *from++) != 0)
                        ;
                }
                endElementHandler(handlerArg, tag->name.str);
            } else if (defaultHandler)
                reportDefault(xmlParserP, enc, s, next);

            while (tag->bindings) {
                BINDING * const b = tag->bindings;
                if (endNamespaceDeclHandler)
                    endNamespaceDeclHandler(handlerArg, b->prefix->name);
                tag->bindings = tag->bindings->nextTagBinding;
                b->nextTagBinding = freeBindingList;
                freeBindingList = b;
                b->prefix->binding = b->prevPrefixBinding;
            }
            if (tagLevel == 0) {
                epilogProcessor(xmlParserP, next, end, nextPtr,
                                errorCodeP, errorP);
                *doneP = true;
            } else {
                *errorCodeP = XML_ERROR_NONE;
                *doneP = false;
            }
        }
    }
}



static void
processContentToken(XML_Parser       const xmlParserP,
                    int              const tok,
                    const ENCODING * const enc,
                    const char *     const s,
                    const char *     const end,
                    const char **    const nextP,
                    const char **    const nextPtr,
                    int              const startTagLevel,
                    const char **    const eventPP,
                    const char **    const eventEndPP,
                    bool *           const doneP,
                    enum XML_Error * const errorCodeP,
                    const char **    const errorP) {

    Parser * const parser = (Parser *) xmlParserP;

    *errorP = NULL;
    *errorCodeP = XML_ERROR_NONE;

    switch (tok) {
    case XML_TOK_TRAILING_CR:
      if (nextPtr) {
          *nextPtr = s;
          *doneP = true;
      } else {
          *eventEndPP = end;

          if (characterDataHandler) {
              XML_Char c = 0xA;
              characterDataHandler(handlerArg, &c, 1);
          } else if (defaultHandler)
              reportDefault(xmlParserP, enc, s, end);

          if (startTagLevel == 0)
              *errorCodeP = XML_ERROR_NO_ELEMENTS;
          else if (tagLevel != startTagLevel) {
              *errorCodeP = XML_ERROR_ASYNC_ENTITY;
          } else
              *doneP = true;
      }
      break;
    case XML_TOK_NONE:
        if (nextPtr) {
            *nextPtr = s;
            *doneP = true;
        } else if (startTagLevel > 0) {
            if (tagLevel != startTagLevel)
                *errorCodeP = XML_ERROR_ASYNC_ENTITY;
            else
                *doneP = true;
        } else
            *errorCodeP = XML_ERROR_NO_ELEMENTS;
        break;
    case XML_TOK_INVALID:
        *eventPP = *nextP;
        *errorCodeP = XML_ERROR_INVALID_TOKEN;
        xmlrpc_asprintf(errorP, "Invalid token, starting %ld bytes in",
                        (long)(*nextP - s));
        break;
    case XML_TOK_PARTIAL:
        if (nextPtr) {
            *nextPtr = s;
            *doneP = true;
        } else
            *errorCodeP = XML_ERROR_UNCLOSED_TOKEN;
        break;
    case XML_TOK_PARTIAL_CHAR:
        if (nextPtr) {
            *nextPtr = s;
            *doneP = true;
        } else
            *errorCodeP = XML_ERROR_PARTIAL_CHAR;
        break;
    case XML_TOK_ENTITY_REF:
        doEntityRef(xmlParserP, enc, s, *nextP, errorCodeP, errorP);
        break;
    case XML_TOK_START_TAG_WITH_ATTS:
        if (!startElementHandler)
            *errorCodeP = storeAtts(xmlParserP, enc, s, 0, 0);
        if (*errorCodeP == XML_ERROR_NONE)
            doStartTagNoAtts(xmlParserP, enc, s, *nextP, nextPtr,
                             errorCodeP, errorP);
        break;
    case XML_TOK_START_TAG_NO_ATTS:
        doStartTagNoAtts(xmlParserP, enc, s, *nextP, nextPtr,
                         errorCodeP, errorP);
        break;
    case XML_TOK_EMPTY_ELEMENT_WITH_ATTS:
        if (!startElementHandler)
            *errorCodeP = storeAtts(xmlParserP, enc, s, 0, 0);
        
        if (*errorCodeP == XML_ERROR_NONE)
            doEmptyElementNoAtts(xmlParserP, enc, s, end, *nextP, nextPtr,
                                 eventPP, eventEndPP,
                                 doneP, errorCodeP, errorP);
        break;
    case XML_TOK_EMPTY_ELEMENT_NO_ATTS:
        doEmptyElementNoAtts(xmlParserP, enc, s, end, *nextP, nextPtr,
                             eventPP, eventEndPP,
                             doneP, errorCodeP, errorP);
        break;
    case XML_TOK_END_TAG:
        doEndTag(xmlParserP, enc, s, end, *nextP, nextPtr, startTagLevel,
                 eventPP, doneP, errorCodeP, errorP);
        break;
    case XML_TOK_CHAR_REF: {
        int const n = XmlCharRefNumber(enc, s);
        if (n < 0)
            *errorCodeP = XML_ERROR_BAD_CHAR_REF;
        else {
            if (characterDataHandler) {
                XML_Char buf[XML_ENCODE_MAX];
                characterDataHandler(handlerArg, buf,
                                     XmlEncode(n, (ICHAR *)buf));
            } else if (defaultHandler)
                reportDefault(xmlParserP, enc, s, *nextP);
        }
    } break;
    case XML_TOK_XML_DECL:
        *errorCodeP = XML_ERROR_MISPLACED_XML_PI;
        break;
    case XML_TOK_DATA_NEWLINE:
        if (characterDataHandler) {
            XML_Char c = 0xA;
            characterDataHandler(handlerArg, &c, 1);
        } else if (defaultHandler)
            reportDefault(xmlParserP, enc, s, *nextP);
        break;
    case XML_TOK_CDATA_SECT_OPEN: {
        enum XML_Error result;
        if (startCdataSectionHandler)
            startCdataSectionHandler(handlerArg);
        else if (defaultHandler)
            reportDefault(xmlParserP, enc, s, *nextP);
        result = doCdataSection(xmlParserP, enc, nextP, end, nextPtr);
        if (!*nextP) {
            processor = cdataSectionProcessor;
            *errorCodeP = result;
        }
    } break;
    case XML_TOK_TRAILING_RSQB:
        if (nextPtr) {
            *nextPtr = s;
            *errorCodeP = XML_ERROR_NONE;
        } else {
            if (characterDataHandler) {
                if (MUST_CONVERT(enc, s)) {
                    const char * from;
                    ICHAR * dataPtr;
                    from = s;
                    dataPtr = (ICHAR *)dataBuf;
                    XmlConvert(enc, &from, end, &dataPtr, (ICHAR *)dataBufEnd);
                    {
                        size_t const len = dataPtr - (ICHAR *)dataBuf;
                        assert((size_t)(int)len == len);   /* parser reqt */
                        characterDataHandler(handlerArg, dataBuf, (int)len);
                    }
                } else {
                    size_t const len = (XML_Char *)end - (XML_Char *)s;
                    assert((size_t)(int)len == len);   /* parser reqt */
                    characterDataHandler(handlerArg, (XML_Char *)s, (int)len);
                }
            } else if (defaultHandler)
                reportDefault(xmlParserP, enc, s, end);

            if (startTagLevel == 0) {
                *eventPP = end;
                *errorCodeP = XML_ERROR_NO_ELEMENTS;
            } else if (tagLevel != startTagLevel) {
                *eventPP = end;
                *errorCodeP = XML_ERROR_ASYNC_ENTITY;
            } else
                *doneP = true;
        }
        break;
    case XML_TOK_DATA_CHARS:
        if (characterDataHandler) {
            if (MUST_CONVERT(enc, s)) {
                for (;;) {
                    const char * from;
                    ICHAR * dataPtr;
                    dataPtr = (ICHAR *)dataBuf;
                    from = s;
                    XmlConvert(enc, &from, *nextP, &dataPtr,
                               (ICHAR *)dataBufEnd);
                    *eventEndPP = from;
                    {
                        size_t const len = dataPtr - (ICHAR *)dataBuf;
                        assert((size_t)(int)len == len);   /* parser reqt */
                        characterDataHandler(handlerArg, dataBuf, (int)len);
                    }
                    if (from == *nextP)
                        break;
                    *eventPP = from;
                }
            } else {
                size_t const len = (XML_Char *)*nextP - (XML_Char *)s;
                assert((size_t)(int)len == len);   /* parser reqt */
                characterDataHandler(handlerArg, (XML_Char *)s, len);
            }
        } else if (defaultHandler)
            reportDefault(xmlParserP, enc, s, *nextP);
        break;
    case XML_TOK_PI:
        if (!reportProcessingInstruction(xmlParserP, enc, s, *nextP))
            *errorCodeP = XML_ERROR_NO_MEMORY;
        break;
    case XML_TOK_COMMENT:
        if (!reportComment(xmlParserP, enc, s, *nextP))
            *errorCodeP = XML_ERROR_NO_MEMORY;
        break;
    default:
        if (defaultHandler)
            reportDefault(xmlParserP, enc, s, *nextP);
        break;
    }
}



static void
doContent(XML_Parser       const xmlParserP,
          int              const startTagLevel,
          const ENCODING * const enc,
          const char *     const startArg,
          const char *     const end,
          const char **    const nextPtr,
          enum XML_Error * const errorCodeP,
          const char **    const errorP) {

    Parser * const parser = (Parser *) xmlParserP;

    const char **eventPP;
    const char **eventEndPP;
    const char * s;
    bool done;

    if (enc == parser->m_encoding) {
        eventPP = &eventPtr;
        eventEndPP = &eventEndPtr;
    } else {
        eventPP = &(openInternalEntities->internalEventPtr);
        eventEndPP = &(openInternalEntities->internalEventEndPtr);
    }

    s = startArg;
    *eventPP = s;
    done = false;
    *errorCodeP = XML_ERROR_NONE;
    *errorP = NULL;

    while (*errorCodeP == XML_ERROR_NONE && !done) {
        int tok;
        const char * next;
        const char * error;

        next = s; /* XmlContentTok doesn't always set the last arg */
        /* XmlContentTok() is normally normal_contentTok(), aka
           PREFIX(contentTok)() in xmltok/xmltok_impl.c
        */
        tok = XmlContentTok(enc, s, end, &next);
        *eventEndPP = next;

        processContentToken(xmlParserP, tok, enc, s, end, &next, nextPtr,
                            startTagLevel, eventPP, eventEndPP, &done,
                            errorCodeP, &error);

        if (*errorCodeP != XML_ERROR_NONE) {
            const char * const xmlSample = extractXmlSample(s, end, 40);

            if (error) {
                xmlrpc_asprintf(errorP, "Problem with token at '%s...': %s",
                                xmlSample, error);
                xmlrpc_strfree(error);
            } else
                xmlrpc_asprintf(errorP, "Problem with token at '%s...': %s",
                                xmlSample,
                                xmlrpc_XML_ErrorString(*errorCodeP));

            xmlrpc_strfree(xmlSample);
        }
        *eventPP = s = next;
    }
}



static Processor externalEntityContentProcessor;

static void
externalEntityContentProcessor(XML_Parser       const xmlParserP,
                               const char *     const start,
                               const char *     const end,
                               const char **    const endPtr,
                               enum XML_Error * const errorCodeP,
                               const char **    const errorP) {

    Parser * const parser = (Parser *) xmlParserP;

    *errorP = NULL;

    doContent(xmlParserP, 1, parser->m_encoding, start, end, endPtr,
              errorCodeP, errorP);
}



static Processor externalEntityInitProcessor3;

static void
externalEntityInitProcessor3(XML_Parser       const xmlParserP,
                             const char *     const startArg,
                             const char *     const end,
                             const char **    const endPtr,
                             enum XML_Error * const errorCodeP,
                             const char **    const errorP) {

    Parser * const parser = (Parser *) xmlParserP;

    const char * start;
    const char *next;
    int tok;
    
    tok = XmlContentTok(parser->m_encoding, startArg, end, &next);

    *errorP = NULL;

    start = startArg;

    switch (tok) {
    case XML_TOK_XML_DECL:
    {
        enum XML_Error result = processXmlDecl(xmlParserP, 1, start, next);
        if (result != XML_ERROR_NONE) {
            *errorCodeP = result;
            return;
        }
        start = next;
    }
    break;
    case XML_TOK_PARTIAL:
        if (endPtr) {
            *endPtr = start;
            *errorCodeP = XML_ERROR_NONE;
            return;
        }
        eventPtr = start;
        *errorCodeP = XML_ERROR_UNCLOSED_TOKEN;
        return;
    case XML_TOK_PARTIAL_CHAR:
        if (endPtr) {
            *endPtr = start;
            *errorCodeP = XML_ERROR_NONE;
            return;
        }
        eventPtr = start;
        *errorCodeP = XML_ERROR_PARTIAL_CHAR;
        return;
    }
    processor = externalEntityContentProcessor;
    tagLevel = 1;
    doContent(xmlParserP, 1, parser->m_encoding, start, end, endPtr,
              errorCodeP, errorP);
}



static Processor externalEntityInitProcessor2;

static void
externalEntityInitProcessor2(XML_Parser       const xmlParserP,
                             const char *     const startArg,
                             const char *     const end,
                             const char **    const endPtr,
                             enum XML_Error * const errorCodeP,
                             const char **    const errorP) {

    Parser * const parser = (Parser *)xmlParserP;

    const char * start;
    const char * next;
    int tok;
    
    tok = XmlContentTok(parser->m_encoding, startArg, end, &next);

    start = startArg;

    switch (tok) {
    case XML_TOK_BOM:
        start = next;
        break;
    case XML_TOK_PARTIAL:
        if (endPtr) {
            *endPtr = start;
            *errorCodeP = XML_ERROR_NONE;
            *errorP = NULL;
        } else {
            eventPtr = start;
            *errorCodeP = XML_ERROR_UNCLOSED_TOKEN;
            *errorP = NULL;
        }
        return;
    case XML_TOK_PARTIAL_CHAR:
        if (endPtr) {
            *endPtr = start;
            *errorCodeP = XML_ERROR_NONE;
            *errorP = NULL;
        } else {
            eventPtr = start;
            *errorCodeP = XML_ERROR_PARTIAL_CHAR;
            *errorP = NULL;
        }
        return;
    }
    processor = externalEntityInitProcessor3;
    externalEntityInitProcessor3(xmlParserP, start, end, endPtr,
                                 errorCodeP, errorP);
}



static Processor externalEntityInitProcessor;

static void
externalEntityInitProcessor(XML_Parser       const parser,
                            const char *     const start,
                            const char *     const end,
                            const char **    const endPtr,
                            enum XML_Error * const errorCodeP,
                            const char **    const errorP) {

    enum XML_Error result;

    result = initializeEncoding(parser);

    if (result != XML_ERROR_NONE) {
        *errorCodeP = result;
        *errorP = NULL;
    } else {
        processor = externalEntityInitProcessor2;

        externalEntityInitProcessor2(parser, start, end, endPtr,
                                     errorCodeP, errorP);
    }
}



static enum XML_Error
doIgnoreSection(XML_Parser       const xmlParserP,
                const ENCODING * const enc,
                const char **    const startPtr,
                const char *     const end,
                const char **    const nextPtr) {
/*----------------------------------------------------------------------------

  We set *startPtr to non-null is the section is closed, and to null if
  the section is not yet closed.
-----------------------------------------------------------------------------*/
    Parser * const parser = (Parser *) xmlParserP;
    const char * const s = *startPtr;

    enum XML_Error retval;
    const char * next;
    int tok;
    const char ** eventPP;
    const char ** eventEndPP;

    if (enc == parser->m_encoding) {
        eventPP = &eventPtr;
        eventEndPP = &eventEndPtr;
    } else {
        eventPP = &(openInternalEntities->internalEventPtr);
        eventEndPP = &(openInternalEntities->internalEventEndPtr);
    }
    *eventPP = s;
    *startPtr = '\0';
    tok = XmlIgnoreSectionTok(enc, s, end, &next);
    *eventEndPP = next;

    switch (tok) {
    case XML_TOK_IGNORE_SECT:
        if (defaultHandler)
            reportDefault(xmlParserP, enc, s, next);
        *startPtr = next;
        retval = XML_ERROR_NONE;
        break;
    case XML_TOK_INVALID:
        *eventPP = next;
        retval = XML_ERROR_INVALID_TOKEN;
        break;
    case XML_TOK_PARTIAL_CHAR:
        if (nextPtr) {
            *nextPtr = s;
            retval = XML_ERROR_NONE;
        } else
            retval = XML_ERROR_PARTIAL_CHAR;
        break;
    case XML_TOK_PARTIAL:
    case XML_TOK_NONE:
        if (nextPtr) {
            *nextPtr = s;
            retval = XML_ERROR_NONE;
        } else
            retval = XML_ERROR_SYNTAX; /* XML_ERROR_UNCLOSED_IGNORE_SECTION */
        break;
    default:
        assert(false);  /* All possibilities are handled above */
        retval = 99; /* quiet compiler warning */
    }

    return retval;
}



static Processor prologProcessor;


/* The idea here is to avoid using stack for each IGNORE section when
the whole file is parsed with one call. */

static Processor ignoreSectionProcessor;

static void
ignoreSectionProcessor(XML_Parser       const xmlParserP,
                       const char *     const startArg,
                       const char *     const end,
                       const char **    const endPtr,
                       enum XML_Error * const errorCodeP,
                       const char **    const errorP) {
    
    Parser * const parser = (Parser *) xmlParserP;

    enum XML_Error result;
    const char * start;

    start = startArg;  /* initial value */

    result = doIgnoreSection(parser, parser->m_encoding, &start, end, endPtr);

    if (start) {
        processor = prologProcessor;
        prologProcessor(xmlParserP, start, end, endPtr, errorCodeP, errorP);
    } else {
        *errorCodeP = result;
        *errorP = NULL;
    }
}



/* Forward declaration for recursive reference: */
static void
processInternalParamEntity(XML_Parser       const parser,
                           ENTITY *         const entity,
                           enum XML_Error * const errorCodeP,
                           const char **    const errorP);

static void
doProlog(XML_Parser       const xmlParserP,
         const ENCODING * const encArg,
         const char *     const startArg,
         const char *     const end,
         int              const tokArg,
         const char *     const nextArg,
         const char **    const nextPtr,
         enum XML_Error * const errorCodeP,
         const char **    const errorP) {
    
  Parser * const parser = (Parser *) xmlParserP;

  int tok;
  const char * next;
  const ENCODING * enc;
  const char * s;

  static const XML_Char externalSubsetName[] = { '#' , '\0' };

  const char **eventPP;
  const char **eventEndPP;

  *errorP = NULL;

  tok = tokArg;
  next = nextArg;
  enc = encArg;
  s = startArg;

  if (enc == parser->m_encoding) {
    eventPP = &eventPtr;
    eventEndPP = &eventEndPtr;
  }
  else {
    eventPP = &(openInternalEntities->internalEventPtr);
    eventEndPP = &(openInternalEntities->internalEventEndPtr);
  }
  for (;;) {
    int role;
    *eventPP = s;
    *eventEndPP = next;
    if (tok <= 0) {
      if (nextPtr != 0 && tok != XML_TOK_INVALID) {
        *nextPtr = s;
        *errorCodeP = XML_ERROR_NONE;
        return;
      }
      switch (tok) {
      case XML_TOK_INVALID:
        *eventPP = next;
        *errorCodeP = XML_ERROR_INVALID_TOKEN;
        return;
      case XML_TOK_PARTIAL:
        *errorCodeP = XML_ERROR_UNCLOSED_TOKEN;
        return;
      case XML_TOK_PARTIAL_CHAR:
        *errorCodeP = XML_ERROR_PARTIAL_CHAR;
        return;
      case XML_TOK_NONE:
        if (enc != parser->m_encoding)
          *errorCodeP = XML_ERROR_NONE;
        else {
            if (parentParser) {
                if (XmlTokenRole(&prologState, XML_TOK_NONE, end, end, enc)
                    == XML_ROLE_ERROR) {
                    *errorCodeP = XML_ERROR_SYNTAX;
                } else {
                    *errorCodeP = XML_ERROR_NONE;
                    hadExternalDoctype = 0;
                }
            } else
                *errorCodeP = XML_ERROR_NO_ELEMENTS;
        }
        return;
      default:
        tok = -tok;
        next = end;
        break;
      }
    }
    role = XmlTokenRole(&prologState, tok, s, next, enc);
    switch (role) {
    case XML_ROLE_XML_DECL: {
        enum XML_Error result = processXmlDecl(xmlParserP, 0, s, next);
        if (result != XML_ERROR_NONE) {
          *errorCodeP = result;
          return;
        }
        enc = parser->m_encoding;
      }
      break;
    case XML_ROLE_DOCTYPE_NAME:
      if (startDoctypeDeclHandler) {
        const XML_Char *name = poolStoreString(&tempPool, enc, s, next);
        if (!name) {
          *errorCodeP = XML_ERROR_NO_MEMORY;
          return;
        }
        startDoctypeDeclHandler(handlerArg, name);
        poolClear(&tempPool);
      }
      break;
    case XML_ROLE_TEXT_DECL: {
        enum XML_Error result = processXmlDecl(xmlParserP, 1, s, next);
        if (result != XML_ERROR_NONE) {
          *errorCodeP = result;
          return;
        }
        enc = parser->m_encoding;
      }
      break;
    case XML_ROLE_DOCTYPE_PUBLIC_ID:
      declEntity = (ENTITY *)lookup(&dtd.paramEntities,
                                    externalSubsetName,
                                    sizeof(ENTITY));
      if (!declEntity) {
        *errorCodeP = XML_ERROR_NO_MEMORY;
        return;
      }
      /* fall through */
    case XML_ROLE_ENTITY_PUBLIC_ID:
      if (!XmlIsPublicId(enc, s, next, eventPP)) {
        *errorCodeP = XML_ERROR_SYNTAX;
        return;
      }
      if (declEntity) {
        XML_Char *tem = poolStoreString(&dtd.pool,
                                        enc,
                                        s + enc->minBytesPerChar,
                                        next - enc->minBytesPerChar);
        if (!tem) {
          *errorCodeP = XML_ERROR_NO_MEMORY;
          return;
        }
        normalizePublicId(tem);
        declEntity->publicId = tem;
        poolFinish(&dtd.pool);
      }
      break;
    case XML_ROLE_DOCTYPE_CLOSE:
      if (dtd.complete && hadExternalDoctype) {
        dtd.complete = 0;
        if (paramEntityParsing && externalEntityRefHandler) {
          ENTITY *entity = (ENTITY *)lookup(&dtd.paramEntities,
                                            externalSubsetName,
                                            0);
          if (!externalEntityRefHandler(externalEntityRefHandlerArg,
                                        0,
                                        entity->base,
                                        entity->systemId,
                                        entity->publicId)) {
           *errorCodeP = XML_ERROR_EXTERNAL_ENTITY_HANDLING;
           return;
          }
        }
        if (!dtd.complete
            && !dtd.standalone
            && notStandaloneHandler
            && !notStandaloneHandler(handlerArg)) {
          *errorCodeP = XML_ERROR_NOT_STANDALONE;
          return;
        }
      }
      if (endDoctypeDeclHandler)
        endDoctypeDeclHandler(handlerArg);
      break;
    case XML_ROLE_INSTANCE_START: {
      processor = contentProcessor;
      contentProcessor(xmlParserP, s, end, nextPtr, errorCodeP, errorP);
      return;
    }
    case XML_ROLE_ATTLIST_ELEMENT_NAME:
      {
        const XML_Char *name = poolStoreString(&dtd.pool, enc, s, next);
        if (!name) {
          *errorCodeP = XML_ERROR_NO_MEMORY;
          return;
        }
        declElementType = (ELEMENT_TYPE *)
            lookup(&dtd.elementTypes, name, sizeof(ELEMENT_TYPE));
        if (!declElementType) {
          *errorCodeP = XML_ERROR_NO_MEMORY;
          return;
        }
        if (declElementType->name != name)
          poolDiscard(&dtd.pool);
        else {
          poolFinish(&dtd.pool);
          if (!setElementTypePrefix(xmlParserP, declElementType)) {
            *errorCodeP = XML_ERROR_NO_MEMORY;
            return;
          }
        }
        break;
      }
    case XML_ROLE_ATTRIBUTE_NAME:
      declAttributeId = getAttributeId(xmlParserP, enc, s, next);
      if (!declAttributeId) {
        *errorCodeP = XML_ERROR_NO_MEMORY;
        return;
      }
      declAttributeIsCdata = 0;
      declAttributeIsId = 0;
      break;
    case XML_ROLE_ATTRIBUTE_TYPE_CDATA:
      declAttributeIsCdata = 1;
      break;
    case XML_ROLE_ATTRIBUTE_TYPE_ID:
      declAttributeIsId = 1;
      break;
    case XML_ROLE_IMPLIED_ATTRIBUTE_VALUE:
    case XML_ROLE_REQUIRED_ATTRIBUTE_VALUE:
      if (dtd.complete
          && !defineAttribute(declElementType, declAttributeId,
                              declAttributeIsCdata,
                              declAttributeIsId, 0)) {
        *errorCodeP = XML_ERROR_NO_MEMORY;
        return;
      }
      break;
    case XML_ROLE_DEFAULT_ATTRIBUTE_VALUE:
    case XML_ROLE_FIXED_ATTRIBUTE_VALUE:
      {
        const XML_Char *attVal;
        enum XML_Error result
          = storeAttributeValue(xmlParserP, enc, declAttributeIsCdata,
                                s + enc->minBytesPerChar,
                                next - enc->minBytesPerChar,
                                &dtd.pool);
        if (result) {
          *errorCodeP = result;
          return;
        }
        attVal = poolStart(&dtd.pool);
        poolFinish(&dtd.pool);
        if (dtd.complete
            /* ID attributes aren't allowed to have a default */
            && !defineAttribute(declElementType, declAttributeId,
                                declAttributeIsCdata, 0, attVal)) {
          *errorCodeP = XML_ERROR_NO_MEMORY;
          return;
        }
        break;
      }
    case XML_ROLE_ENTITY_VALUE:
      {
        enum XML_Error result = storeEntityValue(xmlParserP, enc,
                                                 s + enc->minBytesPerChar,
                                                 next - enc->minBytesPerChar);
        if (declEntity) {
          declEntity->textPtr = poolStart(&dtd.pool);
          declEntity->textLen = poolLength(&dtd.pool);
          poolFinish(&dtd.pool);
          if (internalParsedEntityDeclHandler
              /* Check it's not a parameter entity */
              && ((ENTITY *)lookup(&dtd.generalEntities, declEntity->name, 0)
                  == declEntity)) {
            *eventEndPP = s;
            internalParsedEntityDeclHandler(handlerArg,
                                            declEntity->name,
                                            declEntity->textPtr,
                                            declEntity->textLen);
          }
        }
        else
          poolDiscard(&dtd.pool);
        if (result != XML_ERROR_NONE) {
          *errorCodeP = result;
          return;
        }
      }
      break;
    case XML_ROLE_DOCTYPE_SYSTEM_ID:
      if (!dtd.standalone
          && !paramEntityParsing
          && notStandaloneHandler
          && !notStandaloneHandler(handlerArg)) {
        *errorCodeP = XML_ERROR_NOT_STANDALONE;
        return;
      }
      hadExternalDoctype = 1;
      if (!declEntity) {
        declEntity = (ENTITY *)lookup(&dtd.paramEntities,
                                      externalSubsetName,
                                      sizeof(ENTITY));
        if (!declEntity) {
          *errorCodeP = XML_ERROR_NO_MEMORY;
          return;
        }
      }
      /* fall through */
    case XML_ROLE_ENTITY_SYSTEM_ID:
      if (declEntity) {
        declEntity->systemId = poolStoreString(&dtd.pool, enc,
                                               s + enc->minBytesPerChar,
                                               next - enc->minBytesPerChar);
        if (!declEntity->systemId) {
          *errorCodeP = XML_ERROR_NO_MEMORY;
          return;
        }
        declEntity->base = curBase;
        poolFinish(&dtd.pool);
      }
      break;
    case XML_ROLE_ENTITY_NOTATION_NAME:
      if (declEntity) {
        declEntity->notation = poolStoreString(&dtd.pool, enc, s, next);
        if (!declEntity->notation) {
          *errorCodeP = XML_ERROR_NO_MEMORY;
          return;
        }
        poolFinish(&dtd.pool);
        if (unparsedEntityDeclHandler) {
          *eventEndPP = s;
          unparsedEntityDeclHandler(handlerArg,
                                    declEntity->name,
                                    declEntity->base,
                                    declEntity->systemId,
                                    declEntity->publicId,
                                    declEntity->notation);
        }

      }
      break;
    case XML_ROLE_EXTERNAL_GENERAL_ENTITY_NO_NOTATION:
      if (declEntity && externalParsedEntityDeclHandler) {
        *eventEndPP = s;
        externalParsedEntityDeclHandler(handlerArg,
                                        declEntity->name,
                                        declEntity->base,
                                        declEntity->systemId,
                                        declEntity->publicId);
      }
      break;
    case XML_ROLE_GENERAL_ENTITY_NAME:
      {
        const XML_Char *name;
        if (XmlPredefinedEntityName(enc, s, next)) {
          declEntity = 0;
          break;
        }
        name = poolStoreString(&dtd.pool, enc, s, next);
        if (!name) {
          *errorCodeP = XML_ERROR_NO_MEMORY;
          return;
        }
        if (dtd.complete) {
            declEntity = (ENTITY *)
                lookup(&dtd.generalEntities, name, sizeof(ENTITY));
          if (!declEntity) {
            *errorCodeP = XML_ERROR_NO_MEMORY;
            return;
          }
          if (declEntity->name != name) {
            poolDiscard(&dtd.pool);
            declEntity = 0;
          }
          else
            poolFinish(&dtd.pool);
        }
        else {
          poolDiscard(&dtd.pool);
          declEntity = 0;
        }
      }
      break;
    case XML_ROLE_PARAM_ENTITY_NAME:
      if (dtd.complete) {
        const XML_Char *name = poolStoreString(&dtd.pool, enc, s, next);
        if (!name) {
          *errorCodeP = XML_ERROR_NO_MEMORY;
          return;
        }
        declEntity = (ENTITY *)
            lookup(&dtd.paramEntities, name, sizeof(ENTITY));
        if (!declEntity) {
          *errorCodeP = XML_ERROR_NO_MEMORY;
          return;
        }
        if (declEntity->name != name) {
          poolDiscard(&dtd.pool);
          declEntity = 0;
        }
        else
          poolFinish(&dtd.pool);
      }
      break;
    case XML_ROLE_NOTATION_NAME:
      declNotationPublicId = 0;
      declNotationName = 0;
      if (notationDeclHandler) {
        declNotationName = poolStoreString(&tempPool, enc, s, next);
        if (!declNotationName) {
          *errorCodeP = XML_ERROR_NO_MEMORY;
          return;
        }
        poolFinish(&tempPool);
      }
      break;
    case XML_ROLE_NOTATION_PUBLIC_ID:
      if (!XmlIsPublicId(enc, s, next, eventPP)) {
        *errorCodeP = XML_ERROR_SYNTAX;
        return;
      }
      if (declNotationName) {
        XML_Char *tem = poolStoreString(&tempPool,
                                        enc,
                                        s + enc->minBytesPerChar,
                                        next - enc->minBytesPerChar);
        if (!tem) {
          *errorCodeP = XML_ERROR_NO_MEMORY;
          return;
        }
        normalizePublicId(tem);
        declNotationPublicId = tem;
        poolFinish(&tempPool);
      }
      break;
    case XML_ROLE_NOTATION_SYSTEM_ID:
      if (declNotationName && notationDeclHandler) {
        const XML_Char *systemId
          = poolStoreString(&tempPool, enc,
                            s + enc->minBytesPerChar,
                            next - enc->minBytesPerChar);
        if (!systemId) {
          *errorCodeP = XML_ERROR_NO_MEMORY;
          return;
        }
        *eventEndPP = s;
        notationDeclHandler(handlerArg,
                            declNotationName,
                            curBase,
                            systemId,
                            declNotationPublicId);
      }
      poolClear(&tempPool);
      break;
    case XML_ROLE_NOTATION_NO_SYSTEM_ID:
      if (declNotationPublicId && notationDeclHandler) {
        *eventEndPP = s;
        notationDeclHandler(handlerArg,
                            declNotationName,
                            curBase,
                            0,
                            declNotationPublicId);
      }
      poolClear(&tempPool);
      break;
    case XML_ROLE_ERROR:
      switch (tok) {
      case XML_TOK_PARAM_ENTITY_REF:
        *errorCodeP = XML_ERROR_PARAM_ENTITY_REF;
        break;
      case XML_TOK_XML_DECL:
        *errorCodeP = XML_ERROR_MISPLACED_XML_PI;
        break;
      default:
        *errorCodeP = XML_ERROR_SYNTAX;
      }
      return;
    case XML_ROLE_IGNORE_SECT:
      {
        enum XML_Error result;
        if (defaultHandler)
          reportDefault(xmlParserP, enc, s, next);
        result = doIgnoreSection(xmlParserP, enc, &next, end, nextPtr);
        if (!next) {
          processor = ignoreSectionProcessor;
          *errorCodeP = result;
          return;
        }
      }
      break;
    case XML_ROLE_GROUP_OPEN:
      if (prologState.level >= groupSize) {
        if (groupSize)
          groupConnector = realloc(groupConnector, groupSize *= 2);
        else
          groupConnector = malloc(groupSize = 32);
        if (!groupConnector) {
          *errorCodeP = XML_ERROR_NO_MEMORY;
          return;
        }
      }
      groupConnector[prologState.level] = 0;
      break;
    case XML_ROLE_GROUP_SEQUENCE:
      if (groupConnector[prologState.level] == '|') {
        *errorCodeP = XML_ERROR_SYNTAX;
        return;
      }
      groupConnector[prologState.level] = ',';
      break;
    case XML_ROLE_GROUP_CHOICE:
      if (groupConnector[prologState.level] == ',') {
        *errorCodeP =  XML_ERROR_SYNTAX;
        return;
      }
      groupConnector[prologState.level] = '|';
      break;
    case XML_ROLE_PARAM_ENTITY_REF:
    case XML_ROLE_INNER_PARAM_ENTITY_REF:
      if (paramEntityParsing
          && (dtd.complete || role == XML_ROLE_INNER_PARAM_ENTITY_REF)) {
        const XML_Char *name;
        ENTITY *entity;
        name = poolStoreString(&dtd.pool, enc,
                                s + enc->minBytesPerChar,
                                next - enc->minBytesPerChar);
        if (!name) {
          *errorCodeP = XML_ERROR_NO_MEMORY;
          return;
        }
        entity = (ENTITY *)lookup(&dtd.paramEntities, name, 0);
        poolDiscard(&dtd.pool);
        if (!entity) {
          /* FIXME what to do if !dtd.complete? */
          *errorCodeP = XML_ERROR_UNDEFINED_ENTITY;
          return;
        }
        if (entity->open) {
          *errorCodeP = XML_ERROR_RECURSIVE_ENTITY_REF;
          return;
        }
        if (entity->textPtr) {
            processInternalParamEntity(xmlParserP, entity, errorCodeP, errorP);
            if (*errorCodeP != XML_ERROR_NONE)
                return;
            break;
        }
        if (role == XML_ROLE_INNER_PARAM_ENTITY_REF) {
          *errorCodeP = XML_ERROR_PARAM_ENTITY_REF;
          return;
        }
        if (externalEntityRefHandler) {
          dtd.complete = 0;
          entity->open = 1;
          if (!externalEntityRefHandler(externalEntityRefHandlerArg,
                                        0,
                                        entity->base,
                                        entity->systemId,
                                        entity->publicId)) {
            entity->open = 0;
            *errorCodeP = XML_ERROR_EXTERNAL_ENTITY_HANDLING;
            return;
          }
          entity->open = 0;
          if (dtd.complete)
            break;
        }
      }
      if (!dtd.standalone
          && notStandaloneHandler
          && !notStandaloneHandler(handlerArg)) {
        *errorCodeP = XML_ERROR_NOT_STANDALONE;
        return;
      }
      dtd.complete = 0;
      if (defaultHandler)
        reportDefault(xmlParserP, enc, s, next);
      break;
    case XML_ROLE_NONE:
      switch (tok) {
      case XML_TOK_PI:
        if (!reportProcessingInstruction(xmlParserP, enc, s, next)) {
          *errorCodeP = XML_ERROR_NO_MEMORY;
          return;
        }
        break;
      case XML_TOK_COMMENT:
        if (!reportComment(xmlParserP, enc, s, next)) {
          *errorCodeP = XML_ERROR_NO_MEMORY;
          return;
        }
        break;
      }
      break;
    }
    if (defaultHandler) {
      switch (tok) {
      case XML_TOK_PI:
      case XML_TOK_COMMENT:
      case XML_TOK_BOM:
      case XML_TOK_XML_DECL:
      case XML_TOK_IGNORE_SECT:
      case XML_TOK_PARAM_ENTITY_REF:
        break;
      default:
        if (role != XML_ROLE_IGNORE_SECT)
          reportDefault(xmlParserP, enc, s, next);
      }
    }
    s = next;
    tok = XmlPrologTok(enc, s, end, &next);
  }
  /* not reached */
}



static Processor prologProcessor;

static void
prologProcessor(XML_Parser       const xmlParserP,
                const char *     const s,
                const char *     const end,
                const char **    const nextPtr,
                enum XML_Error * const errorCodeP,
                const char **    const errorP) {

    Parser * const parser = (Parser *) xmlParserP;

    const char * next;
    int tok;

    *errorP = NULL;

    tok = XmlPrologTok(parser->m_encoding, s, end, &next);

    doProlog(xmlParserP, parser->m_encoding, s, end, tok, next, nextPtr,
             errorCodeP, errorP);
}



static Processor prologInitProcessor;

static void
prologInitProcessor(XML_Parser       const parser,
                    const char *     const s,
                    const char *     const end,
                    const char **    const nextPtr,
                    enum XML_Error * const errorCodeP,
                    const char **    const errorP) {

    enum XML_Error result;

    *errorP = NULL;
    
    result = initializeEncoding(parser);
    
    if (result != XML_ERROR_NONE)
        *errorCodeP = result;
    else {
        processor = prologProcessor;
        prologProcessor(parser, s, end, nextPtr, errorCodeP, errorP);
    }
}



static void
processInternalParamEntity(XML_Parser       const parser,
                           ENTITY *         const entity,
                           enum XML_Error * const errorCodeP,
                           const char **    const errorP) {

    const char *s, *end, *next;
    int tok;
    OPEN_INTERNAL_ENTITY openEntity;

    entity->open = 1;
    openEntity.next = openInternalEntities;
    openInternalEntities = &openEntity;
    openEntity.entity = entity;
    openEntity.internalEventPtr = 0;
    openEntity.internalEventEndPtr = 0;
    s = (char *)entity->textPtr;
    end = (char *)(entity->textPtr + entity->textLen);
    tok = XmlPrologTok(internalEncoding, s, end, &next);

    doProlog(parser, internalEncoding, s, end, tok, next, 0,
             errorCodeP, errorP);

    entity->open = 0;
    openInternalEntities = openEntity.next;
}



XML_Parser
xmlrpc_XML_ParserCreate(const XML_Char * const encodingName) {

    XML_Parser const xmlParserP = malloc(sizeof(Parser));
    Parser * const parser = (Parser *)xmlParserP;
    if (xmlParserP) {
        processor = prologInitProcessor;
        xmlrpc_XmlPrologStateInit(&prologState);
        userData = 0;
        handlerArg = 0;
        startElementHandler = 0;
        endElementHandler = 0;
        characterDataHandler = 0;
        processingInstructionHandler = 0;
        commentHandler = 0;
        startCdataSectionHandler = 0;
        endCdataSectionHandler = 0;
        defaultHandler = 0;
        startDoctypeDeclHandler = 0;
        endDoctypeDeclHandler = 0;
        unparsedEntityDeclHandler = 0;
        notationDeclHandler = 0;
        externalParsedEntityDeclHandler = 0;
        internalParsedEntityDeclHandler = 0;
        startNamespaceDeclHandler = 0;
        endNamespaceDeclHandler = 0;
        notStandaloneHandler = 0;
        externalEntityRefHandler = 0;
        externalEntityRefHandlerArg = parser;
        unknownEncodingHandler = 0;
        buffer = 0;
        bufferPtr = 0;
        bufferEnd = 0;
        parseEndByteIndex = 0;
        parseEndPtr = 0;
        bufferLim = 0;
        declElementType = 0;
        declAttributeId = 0;
        declEntity = 0;
        declNotationName = 0;
        declNotationPublicId = 0;
        memset(&position, 0, sizeof(POSITION));
        errorCode = XML_ERROR_NONE;
        errorString = NULL;
        eventPtr = 0;
        eventEndPtr = 0;
        positionPtr = 0;
        openInternalEntities = 0;
        tagLevel = 0;
        tagStack = 0;
        freeTagList = 0;
        freeBindingList = 0;
        inheritedBindings = 0;
        attsSize = INIT_ATTS_SIZE;
        atts = malloc(attsSize * sizeof(ATTRIBUTE));
        nSpecifiedAtts = 0;
        dataBuf = malloc(INIT_DATA_BUF_SIZE * sizeof(XML_Char));
        groupSize = 0;
        groupConnector = 0;
        hadExternalDoctype = 0;
        unknownEncodingMem = 0;
        unknownEncodingRelease = 0;
        unknownEncodingData = 0;
        unknownEncodingHandlerData = 0;
        namespaceSeparator = '!';
        parentParser = 0;
        paramEntityParsing = XML_PARAM_ENTITY_PARSING_NEVER;
        ns = 0;
        poolInit(&tempPool);
        poolInit(&temp2Pool);
        protocolEncodingName =
            encodingName ? poolCopyString(&tempPool, encodingName) : 0;
        curBase = 0;
        if (!dtdInit(&dtd) || !atts || !dataBuf
            || (encodingName && !protocolEncodingName)) {
            xmlrpc_XML_ParserFree(xmlParserP);
            return 0;
        }
        dataBufEnd = dataBuf + INIT_DATA_BUF_SIZE;
        xmlrpc_XmlInitEncoding(&parser->m_initEncoding,
                               &parser->m_encoding,
                               0);
        internalEncoding = XmlGetInternalEncoding();
    }
    return xmlParserP;
}

XML_Parser
xmlrpc_XML_ParserCreateNS(const XML_Char * const encodingName,
                          XML_Char         const nsSep) {

    static
        const XML_Char implicitContext[] = {
            XML_T('x'), XML_T('m'), XML_T('l'), XML_T('='),
            XML_T('h'), XML_T('t'), XML_T('t'), XML_T('p'), XML_T(':'),
            XML_T('/'), XML_T('/'), XML_T('w'), XML_T('w'), XML_T('w'),
            XML_T('.'), XML_T('w'), XML_T('3'),
            XML_T('.'), XML_T('o'), XML_T('r'), XML_T('g'),
            XML_T('/'), XML_T('X'), XML_T('M'), XML_T('L'),
            XML_T('/'), XML_T('1'), XML_T('9'), XML_T('9'), XML_T('8'),
            XML_T('/'), XML_T('n'), XML_T('a'), XML_T('m'), XML_T('e'),
            XML_T('s'), XML_T('p'), XML_T('a'), XML_T('c'), XML_T('e'),
            XML_T('\0')
        };

    XML_Parser const xmlParserP = xmlrpc_XML_ParserCreate(encodingName);
    Parser * const parser = (Parser *)xmlParserP;
    XML_Parser retval;

    if (xmlParserP) {
        int succeeded;
        xmlrpc_XmlInitEncodingNS(&initEncoding, &parser->m_encoding, 0);
        ns = 1;
        internalEncoding = XmlGetInternalEncodingNS();
        namespaceSeparator = nsSep;

        succeeded = setContext(xmlParserP, implicitContext);
        if (succeeded)
            retval = xmlParserP;
        else {
            xmlrpc_XML_ParserFree(xmlParserP);
            retval = NULL;
        }
    } else
        retval = NULL;

    return retval;
}



int
xmlrpc_XML_SetEncoding(XML_Parser parser, const XML_Char *encodingName)
{
  if (!encodingName)
    protocolEncodingName = 0;
  else {
    protocolEncodingName = poolCopyString(&tempPool, encodingName);
    if (!protocolEncodingName)
      return 0;
  }
  return 1;
}



XML_Parser
xmlrpc_XML_ExternalEntityParserCreate(XML_Parser oldParser,
                                      const XML_Char *context,
                                      const XML_Char *encodingName)
{
  XML_Parser parser = oldParser;
  DTD *oldDtd = &dtd;
  XML_StartElementHandler oldStartElementHandler = startElementHandler;
  XML_EndElementHandler oldEndElementHandler = endElementHandler;
  XML_CharacterDataHandler oldCharacterDataHandler = characterDataHandler;
  XML_ProcessingInstructionHandler oldProcessingInstructionHandler = processingInstructionHandler;
  XML_CommentHandler oldCommentHandler = commentHandler;
  XML_StartCdataSectionHandler oldStartCdataSectionHandler = startCdataSectionHandler;
  XML_EndCdataSectionHandler oldEndCdataSectionHandler = endCdataSectionHandler;
  XML_DefaultHandler oldDefaultHandler = defaultHandler;
  XML_UnparsedEntityDeclHandler oldUnparsedEntityDeclHandler = unparsedEntityDeclHandler;
  XML_NotationDeclHandler oldNotationDeclHandler = notationDeclHandler;
  XML_ExternalParsedEntityDeclHandler oldExternalParsedEntityDeclHandler = externalParsedEntityDeclHandler;
  XML_InternalParsedEntityDeclHandler oldInternalParsedEntityDeclHandler = internalParsedEntityDeclHandler;
  XML_StartNamespaceDeclHandler oldStartNamespaceDeclHandler = startNamespaceDeclHandler;
  XML_EndNamespaceDeclHandler oldEndNamespaceDeclHandler = endNamespaceDeclHandler;
  XML_NotStandaloneHandler oldNotStandaloneHandler = notStandaloneHandler;
  XML_ExternalEntityRefHandler oldExternalEntityRefHandler = externalEntityRefHandler;
  XML_UnknownEncodingHandler oldUnknownEncodingHandler = unknownEncodingHandler;
  void *oldUserData = userData;
  void *oldHandlerArg = handlerArg;
  int oldDefaultExpandInternalEntities = defaultExpandInternalEntities;
  void *oldExternalEntityRefHandlerArg = externalEntityRefHandlerArg;
  int oldParamEntityParsing = paramEntityParsing;
  parser = (ns
            ? xmlrpc_XML_ParserCreateNS(encodingName, namespaceSeparator)
            : xmlrpc_XML_ParserCreate(encodingName));
  if (!parser)
    return 0;
  startElementHandler = oldStartElementHandler;
  endElementHandler = oldEndElementHandler;
  characterDataHandler = oldCharacterDataHandler;
  processingInstructionHandler = oldProcessingInstructionHandler;
  commentHandler = oldCommentHandler;
  startCdataSectionHandler = oldStartCdataSectionHandler;
  endCdataSectionHandler = oldEndCdataSectionHandler;
  defaultHandler = oldDefaultHandler;
  unparsedEntityDeclHandler = oldUnparsedEntityDeclHandler;
  notationDeclHandler = oldNotationDeclHandler;
  externalParsedEntityDeclHandler = oldExternalParsedEntityDeclHandler;
  internalParsedEntityDeclHandler = oldInternalParsedEntityDeclHandler;
  startNamespaceDeclHandler = oldStartNamespaceDeclHandler;
  endNamespaceDeclHandler = oldEndNamespaceDeclHandler;
  notStandaloneHandler = oldNotStandaloneHandler;
  externalEntityRefHandler = oldExternalEntityRefHandler;
  unknownEncodingHandler = oldUnknownEncodingHandler;
  userData = oldUserData;
  if (oldUserData == oldHandlerArg)
    handlerArg = userData;
  else
    handlerArg = parser;
  if (oldExternalEntityRefHandlerArg != oldParser)
    externalEntityRefHandlerArg = oldExternalEntityRefHandlerArg;
  defaultExpandInternalEntities = oldDefaultExpandInternalEntities;
  paramEntityParsing = oldParamEntityParsing;
  if (context) {
    if (!dtdCopy(&dtd, oldDtd) || !setContext(parser, context)) {
      xmlrpc_XML_ParserFree(parser);
      return 0;
    }
    processor = externalEntityInitProcessor;
  }
  else {
    dtdSwap(&dtd, oldDtd);
    parentParser = oldParser;
    xmlrpc_XmlPrologStateInitExternalEntity(&prologState);
    dtd.complete = 1;
    hadExternalDoctype = 1;
  }
  return parser;
}

static
void destroyBindings(BINDING *bindings)
{
  for (;;) {
    BINDING *b = bindings;
    if (!b)
      break;
    bindings = b->nextTagBinding;
    free(b->uri);
    free(b);
  }
}

void
xmlrpc_XML_ParserFree(XML_Parser parser)
{
  for (;;) {
    TAG *p;
    if (tagStack == 0) {
      if (freeTagList == 0)
        break;
      tagStack = freeTagList;
      freeTagList = 0;
    }
    p = tagStack;
    tagStack = tagStack->parent;
    free(p->buf);
    destroyBindings(p->bindings);
    free(p);
  }
  destroyBindings(freeBindingList);
  destroyBindings(inheritedBindings);
  poolDestroy(&tempPool);
  poolDestroy(&temp2Pool);
  if (parentParser) {
    if (hadExternalDoctype)
      dtd.complete = 0;
    dtdSwap(&dtd, &((Parser *)parentParser)->m_dtd);
  }
  dtdDestroy(&dtd);
  free((void *)atts);
  free(groupConnector);
  free(buffer);
  free(dataBuf);
  free(unknownEncodingMem);
  if (unknownEncodingRelease)
    unknownEncodingRelease(unknownEncodingData);
  if (errorString)
    xmlrpc_strfree(errorString);
  free(parser);
}

void
xmlrpc_XML_UseParserAsHandlerArg(XML_Parser parser)
{
  handlerArg = parser;
}

void
xmlrpc_XML_SetUserData(XML_Parser parser, void *p)
{
  if (handlerArg == userData)
    handlerArg = userData = p;
  else
    userData = p;
}

int
xmlrpc_XML_SetBase(XML_Parser parser, const XML_Char *p)
{
  if (p) {
    p = poolCopyString(&dtd.pool, p);
    if (!p)
      return 0;
    curBase = p;
  }
  else
    curBase = 0;
  return 1;
}

const XML_Char *
xmlrpc_XML_GetBase(XML_Parser parser)
{
  return curBase;
}

int
xmlrpc_XML_GetSpecifiedAttributeCount(XML_Parser parser)
{
  return nSpecifiedAtts;
}

int
xmlrpc_XML_GetIdAttributeIndex(XML_Parser parser)
{
  return idAttIndex;
}

void
xmlrpc_XML_SetElementHandler(XML_Parser parser,
                             XML_StartElementHandler start,
                             XML_EndElementHandler end)
{
  startElementHandler = start;
  endElementHandler = end;
}

void
xmlrpc_XML_SetCharacterDataHandler(XML_Parser parser,
                                   XML_CharacterDataHandler handler)
{
  characterDataHandler = handler;
}

void
xmlrpc_XML_SetProcessingInstructionHandler(
    XML_Parser parser,
    XML_ProcessingInstructionHandler handler)
{
  processingInstructionHandler = handler;
}

void
xmlrpc_XML_SetCommentHandler(XML_Parser parser,
                             XML_CommentHandler handler)
{
  commentHandler = handler;
}

void
xmlrpc_XML_SetCdataSectionHandler(XML_Parser parser,
                                  XML_StartCdataSectionHandler start,
                                  XML_EndCdataSectionHandler end)
{
  startCdataSectionHandler = start;
  endCdataSectionHandler = end;
}

void
xmlrpc_XML_SetDefaultHandler(XML_Parser parser,
                             XML_DefaultHandler handler)
{
  defaultHandler = handler;
  defaultExpandInternalEntities = 0;
}

void
xmlrpc_XML_SetDefaultHandlerExpand(XML_Parser parser,
                                   XML_DefaultHandler handler)
{
  defaultHandler = handler;
  defaultExpandInternalEntities = 1;
}

void
xmlrpc_XML_SetDoctypeDeclHandler(XML_Parser parser,
                                 XML_StartDoctypeDeclHandler start,
                                 XML_EndDoctypeDeclHandler end)
{
  startDoctypeDeclHandler = start;
  endDoctypeDeclHandler = end;
}

void
xmlrpc_XML_SetUnparsedEntityDeclHandler(XML_Parser parser,
                                        XML_UnparsedEntityDeclHandler handler)
{
  unparsedEntityDeclHandler = handler;
}

void
xmlrpc_XML_SetExternalParsedEntityDeclHandler(
    XML_Parser parser,
    XML_ExternalParsedEntityDeclHandler handler)
{
  externalParsedEntityDeclHandler = handler;
}

void
xmlrpc_XML_SetInternalParsedEntityDeclHandler(
    XML_Parser parser,
    XML_InternalParsedEntityDeclHandler handler)
{
  internalParsedEntityDeclHandler = handler;
}

void
xmlrpc_XML_SetNotationDeclHandler(XML_Parser parser,
                                  XML_NotationDeclHandler handler)
{
  notationDeclHandler = handler;
}

void
xmlrpc_XML_SetNamespaceDeclHandler(XML_Parser parser,
                                   XML_StartNamespaceDeclHandler start,
                                   XML_EndNamespaceDeclHandler end)
{
  startNamespaceDeclHandler = start;
  endNamespaceDeclHandler = end;
}

void
xmlrpc_XML_SetNotStandaloneHandler(XML_Parser parser,
                                   XML_NotStandaloneHandler handler)
{
  notStandaloneHandler = handler;
}

void
xmlrpc_XML_SetExternalEntityRefHandler(XML_Parser parser,
                                       XML_ExternalEntityRefHandler handler)
{
  externalEntityRefHandler = handler;
}

void
xmlrpc_XML_SetExternalEntityRefHandlerArg(XML_Parser parser, void *arg)
{
  if (arg)
    externalEntityRefHandlerArg = arg;
  else
    externalEntityRefHandlerArg = parser;
}

void
xmlrpc_XML_SetUnknownEncodingHandler(XML_Parser parser,
                                     XML_UnknownEncodingHandler handler,
                                     void *data)
{
  unknownEncodingHandler = handler;
  unknownEncodingHandlerData = data;
}



int
xmlrpc_XML_SetParamEntityParsing(
    XML_Parser                  const parser  ATTR_UNUSED,
    enum XML_ParamEntityParsing const parsing) {
    
    int retval;

    paramEntityParsing = parsing;
    retval = 1;

    return retval;
}



static Processor errorProcessor;

static void
errorProcessor(XML_Parser       const parser      ATTR_UNUSED,
               const char *     const s           ATTR_UNUSED,
               const char *     const end         ATTR_UNUSED,
               const char **    const nextPtr     ATTR_UNUSED,
               enum XML_Error * const errorCodeP,
               const char **    const errorP) {

    *errorP     = errorString;
    *errorCodeP = errorCode;
}



static void
parseFinalLen0(Parser * const parser,
               int *    const retvalP) {

    positionPtr = bufferPtr;
    parseEndPtr = bufferEnd;

    processor(parser, bufferPtr, bufferEnd, 0, &errorCode, &errorString);

    if (errorCode == XML_ERROR_NONE)
        *retvalP = 1;
    else {
        eventEndPtr = eventPtr;
        processor   = errorProcessor;
        *retvalP    = 0;
    }
}



static void
parseNoBuffer(Parser *     const parser,
              const char * const s,
              size_t       const len,
              bool         const isFinal,
              int *        const succeededP) {

    parseEndByteIndex += len;
    positionPtr = s;

    if (isFinal) {
        parseEndPtr = s + len;
        processor(parser, s, parseEndPtr, 0, &errorCode, &errorString);
        if (errorCode == XML_ERROR_NONE)
            *succeededP = true;
        else {
            eventEndPtr = eventPtr;
            processor = errorProcessor;
            *succeededP = false;
        }
    } else {
        const char * end;

        parseEndPtr = s + len;
        processor(parser, s, s + len, &end, &errorCode, &errorString);
        if (errorCode != XML_ERROR_NONE) {
            eventEndPtr = eventPtr;
            processor = errorProcessor;
            *succeededP = false;
        } else {
            int const nLeftOver = s + len - end;
            XmlUpdatePosition(parser->m_encoding, positionPtr, end, &position);
            if (nLeftOver > 0) {
                if (buffer == 0 || nLeftOver > bufferLim - buffer) {
                    REALLOCARRAY(buffer, len * 2);
                    if (buffer)
                        bufferLim = buffer + len * 2;
                }

                if (buffer) {
                    memcpy(buffer, end, nLeftOver);
                    bufferPtr = buffer;
                    bufferEnd = buffer + nLeftOver;
                    *succeededP = true;
                } else {
                    errorCode = XML_ERROR_NO_MEMORY;
                    eventPtr = eventEndPtr = 0;
                    processor = errorProcessor;
                    *succeededP = false;
                }
            } else
                *succeededP = true;
        }
    }
}



int
xmlrpc_XML_Parse(XML_Parser   const xmlParserP,
                 const char * const s,
                 size_t       const len,
                 int          const isFinal) {

    Parser * const parser = (Parser *) xmlParserP;

    int retval;

    if (errorString) {
        xmlrpc_strfree(errorString);
        errorString = NULL;
    }

    if (len == 0) {
        if (!isFinal)
            retval = 1;
        else
            parseFinalLen0(parser, &retval);
    } else if (bufferPtr == bufferEnd)
        parseNoBuffer(parser, s, len, isFinal, &retval);
    else {
        memcpy(xmlrpc_XML_GetBuffer(parser, len), s, len);
        retval = xmlrpc_XML_ParseBuffer(parser, len, isFinal);
    }
    return retval;
}



int
xmlrpc_XML_ParseBuffer(XML_Parser const xmlParserP,
                       int        const len,
                       int        const isFinal) {

    Parser * const parser = (Parser *)xmlParserP;

    const char * const start = bufferPtr;

    if (errorString) {
        xmlrpc_strfree(errorString);
        errorString = NULL;
    }

    positionPtr = start;
    bufferEnd += len;
    parseEndByteIndex += len;
    processor(xmlParserP, start, parseEndPtr = bufferEnd,
              isFinal ? (const char **)0 : &bufferPtr,
              &errorCode, &errorString);
    if (errorCode == XML_ERROR_NONE) {
        if (!isFinal)
            XmlUpdatePosition(parser->m_encoding, positionPtr, bufferPtr,
                              &position);
        return 1;
    } else {
        eventEndPtr = eventPtr;
        processor = errorProcessor;
        return 0;
    }
}



void *
xmlrpc_XML_GetBuffer(XML_Parser const xmlParserP,
                     size_t     const len) {

    Parser * const parser = (Parser *)xmlParserP;

    assert(bufferLim >= bufferEnd);

    if (len > (size_t)(bufferLim - bufferEnd)) {
        /* FIXME avoid integer overflow */
        size_t neededSize = len + (bufferEnd - bufferPtr);
        assert(bufferLim >= buffer);
        if (neededSize  <= (size_t)(bufferLim - buffer)) {
            memmove(buffer, bufferPtr, bufferEnd - bufferPtr);
            bufferEnd = buffer + (bufferEnd - bufferPtr);
            bufferPtr = buffer;
        } else {
            size_t bufferSize;
            char * newBuf;

            bufferSize = bufferLim > bufferPtr ?
                bufferLim - bufferPtr : INIT_BUFFER_SIZE;
            
            do {
                bufferSize *= 2;
            } while (bufferSize < neededSize);
            newBuf = malloc(bufferSize);
            if (newBuf == 0) {
                errorCode = XML_ERROR_NO_MEMORY;
                return 0;
            }
            bufferLim = newBuf + bufferSize;
            if (bufferPtr) {
                memcpy(newBuf, bufferPtr, bufferEnd - bufferPtr);
                free(buffer);
            }
            bufferEnd = newBuf + (bufferEnd - bufferPtr);
            bufferPtr = buffer = newBuf;
        }
    }
    return bufferEnd;
}



enum XML_Error
xmlrpc_XML_GetErrorCode(XML_Parser const parser) {

    return errorCode;
}



const char *
xmlrpc_XML_GetErrorString(XML_Parser const parser) {

    if (errorString)
        return errorString;
    else if (errorCode == XML_ERROR_NONE)
        return NULL;
    else
        return xmlrpc_XML_ErrorString(errorCode);
}



long
xmlrpc_XML_GetCurrentByteIndex(XML_Parser const parser) {

    long retval;

    if (eventPtr) {
        size_t const bytesLeft = parseEndPtr - eventPtr;

        if ((size_t)(long)(bytesLeft) != bytesLeft)
            retval = -1;
        else
            retval = parseEndByteIndex - (long)bytesLeft;
    } else
        retval = -1;

    return retval;
}



int
xmlrpc_XML_GetCurrentByteCount(XML_Parser const parser) {

    int retval;

    if (eventEndPtr && eventPtr) {
        size_t const byteCount = eventEndPtr - eventPtr;

        assert((size_t)(int)byteCount == byteCount);

        retval = (int)byteCount;
    } else 
        retval = 0;

    return retval;
}



int
xmlrpc_XML_GetCurrentLineNumber(XML_Parser const xmlParserP) {

    Parser * const parser = (Parser *) xmlParserP;

    if (eventPtr) {
        XmlUpdatePosition(parser->m_encoding, positionPtr, eventPtr,
                          &position);
        positionPtr = eventPtr;
    }
    return position.lineNumber + 1;
}



int
xmlrpc_XML_GetCurrentColumnNumber(XML_Parser const xmlParserP) {

    Parser * const parser = (Parser *) xmlParserP;

    if (eventPtr) {
        XmlUpdatePosition(parser->m_encoding, positionPtr, eventPtr,
                          &position);
        positionPtr = eventPtr;
    }
    return position.columnNumber;
}



void
xmlrpc_XML_DefaultCurrent(XML_Parser const xmlParserP) {

    Parser * const parser = (Parser *) xmlParserP;

    if (defaultHandler) {
        if (openInternalEntities)
            reportDefault(xmlParserP,
                          internalEncoding,
                          openInternalEntities->internalEventPtr,
                          openInternalEntities->internalEventEndPtr);
        else
            reportDefault(xmlParserP, parser->m_encoding,
                          eventPtr, eventEndPtr);
    }
}

const XML_LChar *
xmlrpc_XML_ErrorString(int const code) {

    static const XML_LChar * const message[] = {
        /* NONE */                    NULL,
        /* NO_MEMORY */               XML_T("out of memory"),
        /* SYNTAX */                  XML_T("syntax error"),
        /* NO_ELEMENTS */             XML_T("no element found"),
        /* INVALID_TOKEN */           XML_T("not well-formed"),
        /* UNCLOSED_TOKEN */          XML_T("unclosed token"),
        /* PARTIAL_CHAR */            XML_T("unclosed token"),
        /* TAG_MISMATCH */            XML_T("mismatched tag"),
        /* DUPLICATE_ATTRIBUTE */     XML_T("duplicate attribute"),
        /* JUNK_AFTER_DOC_ELEMENT */  XML_T("junk after document element"),
        /* PARAM_ENTITY_REF */
             XML_T("illegal parameter entity reference"),
        /* UNDEFINED_ENTITY */        XML_T("undefined entity"),
        /* RECURSIVE_ENTITY_REF */    XML_T("recursive entity reference"),
        /* ASYNC_ENTITY */            XML_T("asynchronous entity"),
        /* BAD_CHAR_REF */
             XML_T("reference to invalid character number"),
        /* BINARY_ENTITY_REF */       XML_T("reference to binary entity"),
        /* ATTRIBUTE_EXTERNAL_ENTITY_REF */
             XML_T("reference to external entity in attribute"),
        /* MISPLACED_XML_PI */
             XML_T("xml processing instruction not at start "
                   "of external entity"),
        /* UNKNOWN_ENCODING */        XML_T("unknown encoding"),
        /* INCORRECT_ENCODING */
             XML_T("encoding specified in XML declaration is incorrect"),
        /* UNCLOSED_CDATA_SECTION */  XML_T("unclosed CDATA section"),
        /* EXTERNAL_ENTITY_HANDLING */
             XML_T("error in processing external entity reference"),
        /* NOT_STANDALONE */          XML_T("document is not standalone")
    };

    const XML_LChar * retval;

    if (code > 0 && (unsigned)code < ARRAY_SIZE(message))
        retval = message[code];
    else
        retval = NULL;
    
    return retval;
}
