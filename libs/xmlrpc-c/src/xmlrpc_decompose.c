/* By Bryan Henderson July 2006.

   Contributed to the public domain.
*/

#include "xmlrpc_config.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "bool.h"
#include "c_util.h"
#include "mallocvar.h"
#include "stdargx.h"

#include "xmlrpc-c/base.h"
#include "xmlrpc-c/base_int.h"
#include "xmlrpc-c/string_int.h"


/* THE DECOMPOSITION TREE

   We execute xmlrpc_decompose_value() in two steps:

   1) Create a "decomposition tree" that tells how Caller wants the XML-RPC
      value decomposed.

   2) Using that tree, decompose the value.  I.e. store stuff in the variables
      in which Caller wants it stored.

   The decomposition tree is composed of information from the format
   string and the variable arguments that the format string describes.
   Nothing in the tree is derived from the actual XML-RPC value being
   decomposed, and the tree may in fact be invalid for the particular
   XML-RPC value it's meant for.

   If the XML-RPC value is a simple value such as an integer, the
   decomposition tree is trivial -- it's a single node that says
   "store the value of an integer via pointer P".

   Where it gets interesting is where the XML-RPC value to be decomposed
   is a complex value (array or struct).  Then, the root node of the tree
   says, e.g., "decompose a 5-item array according to the following
   5 decomposition trees" and it points to 5 additional nodes.  Each of
   those nodes is the root of another decomposition tree (which can also
   be called a branch in this context).  Each of those branches tells
   how to decompose one of the items of the array.

   Roots, interior nodes, and leaves are all essentially the same data
   type.
*/

struct integerDecomp {
    xmlrpc_int32 * valueP;
};

struct boolDecomp {
    xmlrpc_bool * valueP;
};

struct doubleDecomp {
    double * valueP;
};

struct datetimeTDecomp {
    time_t * valueP;
};

struct datetime8Decomp {
    const char ** valueP;
};

struct stringDecomp {
    const char ** valueP;
    size_t * sizeP;
        /* NULL means don't store a size */
};

struct wideStringDecomp {
#if HAVE_UNICODE_WCHAR
    const wchar_t ** valueP;
#endif
    size_t * sizeP;
        /* NULL means don't store a size */
};

struct bitStringDecomp {
    const unsigned char ** valueP;
    size_t * sizeP;
};

struct cptrDecomp {
    void ** valueP;
};

struct i8Decomp {
    xmlrpc_int64 * valueP;
};

struct valueDecomp {
    xmlrpc_value ** valueP;
};

struct arrayValDecomp {
    xmlrpc_value ** valueP;
};

struct structValDecomp {
    xmlrpc_value ** valueP;
};

struct arrayDecomp {
    unsigned int itemCnt;
    bool ignoreExcess;
        /* If there are more that 'itemCnt' items in the array, just
           extract the first 'itemCnt' and ignore the rest, rather than
           fail the decomposition.
        */
    struct decompTreeNode * itemArray[16];
        /* Only first 'itemCnt' elements of this array are defined */
};

struct mbrDecomp {
    const char * key;
        /* The key for the member whose value client wants to extract */
    struct decompTreeNode * decompTreeP;
        /* Instructions on how to decompose (extract) member's value */
};

struct structDecomp {
    unsigned int mbrCnt;
    struct mbrDecomp mbrArray[16];
};


struct decompTreeNode {
    char formatSpecChar;
        /* e.g. 'i', 'b', '8', 'A'.  '(' means array; '{' means struct */
    union {
    /*------------------------------------------------------------------------
      'formatSpecChar' selects among these members.
    -------------------------------------------------------------------------*/
        struct integerDecomp    Tinteger;
        struct boolDecomp       Tbool;
        struct doubleDecomp     Tdouble;
        struct datetimeTDecomp  TdatetimeT;
        struct datetime8Decomp  Tdatetime8;
        struct stringDecomp     Tstring;
        struct wideStringDecomp TwideString;
        struct bitStringDecomp  TbitString;
        struct cptrDecomp       Tcptr;
        struct i8Decomp         Ti8;
        struct valueDecomp      Tvalue;
        struct arrayValDecomp   TarrayVal;
        struct structValDecomp  TstructVal;
        struct arrayDecomp      Tarray;
        struct structDecomp     Tstruct;
    } store;

};



/* prototype for recursive calls */
static void
releaseDecomposition(const struct decompTreeNode * const decompRootP,
                     bool                          const oldstyleMemMgmt);


static void
releaseDecompArray(struct arrayDecomp const arrayDecomp,
                   bool               const oldstyleMemMgmt) {

    unsigned int i;
    for (i = 0; i < arrayDecomp.itemCnt; ++i) {
        releaseDecomposition(arrayDecomp.itemArray[i], oldstyleMemMgmt);
    }
}


static void
releaseDecompStruct(struct structDecomp const structDecomp,
                    bool                const oldstyleMemMgmt) {

    unsigned int i;
    for (i = 0; i < structDecomp.mbrCnt; ++i) {
        releaseDecomposition(structDecomp.mbrArray[i].decompTreeP,
                             oldstyleMemMgmt);
    }
}



static void
releaseDecomposition(const struct decompTreeNode * const decompRootP,
                     bool                          const oldstyleMemMgmt) {
/*----------------------------------------------------------------------------
   Assuming that Caller has decomposed something according to 'decompRootP',
   release whatever resources the decomposed information occupies.

   E.g. if it's  an XML-RPC string, Caller would have allocated memory
   for the C string that represents the decomposed value of XML-RPC string,
   and we release that memory.
-----------------------------------------------------------------------------*/
    switch (decompRootP->formatSpecChar) {
    case 'i':
    case 'b':
    case 'd':
    case 'n':
    case 'I':
    case 't':
    case 'p':
        /* Nothing was allocated; nothing to release */
        break;
    case '8':
        xmlrpc_strfree(*decompRootP->store.Tdatetime8.valueP);
        break;
    case 's':
        xmlrpc_strfree(*decompRootP->store.Tstring.valueP);
        break;
    case 'w':
        free((void*)*decompRootP->store.TwideString.valueP);
        break;
    case '6':
        free((void*)*decompRootP->store.TbitString.valueP);
        break;
    case 'V':
        xmlrpc_DECREF(*decompRootP->store.Tvalue.valueP);
        break;
    case 'A':
        xmlrpc_DECREF(*decompRootP->store.TarrayVal.valueP);
        break;
    case 'S':
        xmlrpc_DECREF(*decompRootP->store.TstructVal.valueP);
        break;
    case '(':
        releaseDecompArray(decompRootP->store.Tarray, oldstyleMemMgmt);
        break;
    case '{':
        releaseDecompStruct(decompRootP->store.Tstruct, oldstyleMemMgmt);
        break;
    }
}



/* Prototype for recursive invocation: */

static void 
decomposeValueWithTree(xmlrpc_env *                  const envP,
                       xmlrpc_value *                const valueP,
                       bool                          const oldstyleMemMgmt,
                       const struct decompTreeNode * const decompRootP);



static void
validateArraySize(xmlrpc_env *         const envP,
                  const xmlrpc_value * const arrayP,
                  struct arrayDecomp   const arrayDecomp) {
    
    unsigned int size;
              
    size = xmlrpc_array_size(envP, arrayP);
    if (!envP->fault_occurred) {
        if (arrayDecomp.itemCnt > size)
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_INDEX_ERROR,
                "Format string requests %u items from array, but array "
                "has only %u items.", arrayDecomp.itemCnt, size);
        else if (arrayDecomp.itemCnt < size && !arrayDecomp.ignoreExcess)
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_INDEX_ERROR,
                "Format string requests exactly %u items from array, "
                "but array has %u items.  (A '*' at the end would avoid "
                "this failure)", arrayDecomp.itemCnt, size);
    }
}



static void 
parsearray(xmlrpc_env *         const envP,
           const xmlrpc_value * const arrayP,
           struct arrayDecomp   const arrayDecomp,
           bool                 const oldstyleMemMgmt) {

    validateArraySize(envP, arrayP, arrayDecomp);

    if (!envP->fault_occurred) {
        unsigned int doneCnt;

        doneCnt = 0;
        while(doneCnt < arrayDecomp.itemCnt && !envP->fault_occurred) {
            xmlrpc_value * itemP;
            
            xmlrpc_array_read_item(envP, arrayP, doneCnt, &itemP);
            
            if (!envP->fault_occurred) {
                XMLRPC_ASSERT(doneCnt < ARRAY_SIZE(arrayDecomp.itemArray));
                decomposeValueWithTree(envP, itemP, oldstyleMemMgmt,
                                       arrayDecomp.itemArray[doneCnt]);
                
                if (!envP->fault_occurred)
                    ++doneCnt;
                
                xmlrpc_DECREF(itemP);
            }
        }
        if (envP->fault_occurred) {
            /* Release the items we completed before we failed. */
            unsigned int i;
            for (i = 0; i < doneCnt; ++i)
                releaseDecomposition(arrayDecomp.itemArray[i],
                                     oldstyleMemMgmt);
        }
    }
}



static void 
parsestruct(xmlrpc_env *        const envP,
            xmlrpc_value *      const structP,
            struct structDecomp const structDecomp,
            bool                const oldstyleMemMgmt) {

    unsigned int doneCount;
    
    doneCount = 0;  /* No members done yet */

    while (doneCount < structDecomp.mbrCnt && !envP->fault_occurred) {
        const char * const key = structDecomp.mbrArray[doneCount].key;

        xmlrpc_value * valueP;

        xmlrpc_struct_read_value(envP, structP, key, &valueP);

        if (!envP->fault_occurred) {
            decomposeValueWithTree(
                envP, valueP, oldstyleMemMgmt,
                structDecomp.mbrArray[doneCount].decompTreeP);

            if (!envP->fault_occurred)
                ++doneCount;

            xmlrpc_DECREF(valueP);
        }
    }

    if (envP->fault_occurred) {
        unsigned int i;
        for (i = 0; i < doneCount; ++i)
            releaseDecomposition(structDecomp.mbrArray[i].decompTreeP,
                                 oldstyleMemMgmt);
    }
}



static void
readString(xmlrpc_env *         const envP,
           const xmlrpc_value * const valueP,
           const char **        const stringValueP,
           bool                 const oldstyleMemMgmt) {

    if (oldstyleMemMgmt) {
        xmlrpc_read_string_old(envP, valueP, stringValueP);
    } else
        xmlrpc_read_string(envP, valueP, stringValueP);
}



static void
readStringLp(xmlrpc_env *         const envP,
             const xmlrpc_value * const valueP,
             size_t *             const lengthP,
             const char **        const stringValueP,
             bool                 const oldstyleMemMgmt) {

    if (oldstyleMemMgmt) {
        xmlrpc_read_string_lp_old(envP, valueP, lengthP, stringValueP);
    } else
        xmlrpc_read_string_lp(envP, valueP, lengthP, stringValueP);
}



#if HAVE_UNICODE_WCHAR
static void
readStringW(xmlrpc_env *     const envP,
            xmlrpc_value *   const valueP,
            const wchar_t ** const stringValueP,
            bool             const oldstyleMemMgmt) {

    if (oldstyleMemMgmt) {
        xmlrpc_read_string_w_old(envP, valueP, stringValueP);
    } else
        xmlrpc_read_string_w(envP, valueP, stringValueP);
}



static void
readStringWLp(xmlrpc_env *     const envP,
              xmlrpc_value *   const valueP,
              size_t *         const lengthP,
              const wchar_t ** const stringValueP,
              bool             const oldstyleMemMgmt) {

    if (oldstyleMemMgmt) {
        xmlrpc_read_string_w_lp_old(envP, valueP, lengthP, stringValueP);
    } else
        xmlrpc_read_string_w_lp(envP, valueP, lengthP, stringValueP);
}
#endif


static void
readDatetime8Str(xmlrpc_env *         const envP,
                 const xmlrpc_value * const valueP,
                 const char **        const stringValueP,
                 bool                 const oldstyleMemMgmt) {

    if (oldstyleMemMgmt)
        xmlrpc_read_datetime_str_old(envP, valueP, stringValueP);
    else
        xmlrpc_read_datetime_str(envP, valueP, stringValueP);
}



static void
readBase64(xmlrpc_env *           const envP,
           const xmlrpc_value *   const valueP,
           size_t *               const lengthP,
           const unsigned char ** const byteStringValueP,
           bool                   const oldstyleMemMgmt) {

    if (oldstyleMemMgmt)
        xmlrpc_read_base64_old(envP, valueP, lengthP, byteStringValueP);
    else
        xmlrpc_read_base64(envP, valueP, lengthP, byteStringValueP);
}


static void 
decomposeValueWithTree(xmlrpc_env *                  const envP,
                       xmlrpc_value *                const valueP,
                       bool                          const oldstyleMemMgmt,
                       const struct decompTreeNode * const decompRootP) {
/*----------------------------------------------------------------------------
   Decompose XML-RPC value *valueP, given the decomposition tree
   *decompRootP.  The decomposition tree tells what structure *valueP
   is expected to have and where to put the various components of it
   (e.g. it says "it's an array of 3 integers.  Put their values at
   locations x, y, and z")
-----------------------------------------------------------------------------*/
    switch (decompRootP->formatSpecChar) {
    case '-':
        /* There's nothing to validate or return */
        break;
    case 'i':
        xmlrpc_read_int(envP, valueP, decompRootP->store.Tinteger.valueP);
        break;

    case 'b':
        xmlrpc_read_bool(envP, valueP, decompRootP->store.Tbool.valueP);
        break;

    case 'd':
        xmlrpc_read_double(envP, valueP, decompRootP->store.Tdouble.valueP);
        break;

    case 't':
        xmlrpc_read_datetime_sec(envP, valueP,
                                 decompRootP->store.TdatetimeT.valueP);
        break;

    case '8':
        readDatetime8Str(envP, valueP, decompRootP->store.Tdatetime8.valueP,
                         oldstyleMemMgmt);
        break;

    case 's':
        if (decompRootP->store.Tstring.sizeP)
            readStringLp(envP, valueP,
                         decompRootP->store.Tstring.sizeP,
                         decompRootP->store.Tstring.valueP,
                         oldstyleMemMgmt);
        else
            readString(envP, valueP, decompRootP->store.Tstring.valueP,
                       oldstyleMemMgmt);
        break;

    case 'w':
#if HAVE_UNICODE_WCHAR
        if (decompRootP->store.Tstring.sizeP)
            readStringWLp(envP, valueP,
                          decompRootP->store.TwideString.sizeP,
                          decompRootP->store.TwideString.valueP,
                          oldstyleMemMgmt);
        else
            readStringW(envP, valueP, decompRootP->store.TwideString.valueP,
                        oldstyleMemMgmt);
#else
        XMLRPC_ASSERT(false);
#endif /* HAVE_UNICODE_WCHAR */
        break;
        
    case '6':
        readBase64(envP, valueP,
                   decompRootP->store.TbitString.sizeP,
                   decompRootP->store.TbitString.valueP,
                   oldstyleMemMgmt);
        break;

    case 'n':
        xmlrpc_read_nil(envP, valueP);
        break;

    case 'I':
        xmlrpc_read_i8(envP, valueP, decompRootP->store.Ti8.valueP);
        break;

    case 'p':
        xmlrpc_read_cptr(envP, valueP, decompRootP->store.Tcptr.valueP);
        break;

    case 'V':
        *decompRootP->store.Tvalue.valueP = valueP;
        if (!oldstyleMemMgmt)
            xmlrpc_INCREF(valueP);
        break;

    case 'A':
        if (xmlrpc_value_type(valueP) != XMLRPC_TYPE_ARRAY)
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_TYPE_ERROR, "Value to be decomposed is of type "
                "%s, but the 'A' specifier requires type ARRAY",
                xmlrpc_type_name(xmlrpc_value_type(valueP)));
        else {
            *decompRootP->store.TarrayVal.valueP = valueP;
            if (!oldstyleMemMgmt)
                xmlrpc_INCREF(valueP);
        }
        break;

    case 'S':
        if (xmlrpc_value_type(valueP) != XMLRPC_TYPE_STRUCT)
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_TYPE_ERROR, "Value to be decomposed is of type "
                "%s, but the 'S' specifier requires type STRUCT.",
                xmlrpc_type_name(xmlrpc_value_type(valueP)));
        else {
            *decompRootP->store.TstructVal.valueP = valueP;
            if (!oldstyleMemMgmt)
                xmlrpc_INCREF(valueP);
        }
        break;

    case '(':
        if (xmlrpc_value_type(valueP) != XMLRPC_TYPE_ARRAY)
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_TYPE_ERROR, "Value to be decomposed is of type "
                "%s, but the '(...)' specifier requires type ARRAY",
                xmlrpc_type_name(xmlrpc_value_type(valueP)));
        else
            parsearray(envP, valueP, decompRootP->store.Tarray,
                       oldstyleMemMgmt);
        break;

    case '{':
        if (xmlrpc_value_type(valueP) != XMLRPC_TYPE_STRUCT)
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_TYPE_ERROR, "Value to be decomposed is of type "
                "%s, but the '{...}' specifier requires type STRUCT",
                xmlrpc_type_name(xmlrpc_value_type(valueP)));
        else
            parsestruct(envP, valueP, decompRootP->store.Tstruct,
                        oldstyleMemMgmt);
        break;

    default:
        /* Every format character that is allowed in a decomposition tree
           node is handled above.
        */
        XMLRPC_ASSERT(false);
    }
}


/* Forward declaration for recursive calls */

static void 
createDecompTreeNext(xmlrpc_env *             const envP,
                     const char **            const formatP,
                     va_listx *               const argsP,
                     struct decompTreeNode ** const decompNodePP);



static void
buildWideStringNode(xmlrpc_env *            const envP ATTR_UNUSED,
                    const char **           const formatP,
                    va_listx *              const argsP,
                    struct decompTreeNode * const decompNodeP) {

#if HAVE_UNICODE_WCHAR
    decompNodeP->store.TwideString.valueP =
        (const wchar_t**) va_arg(argsP->v, wchar_t**);
    if (**formatP == '#') {
        decompNodeP->store.TwideString.sizeP =
            (size_t*) va_arg(argsP->v, size_t**);
        (*formatP)++;
    } else
        decompNodeP->store.TwideString.sizeP = NULL;
#else
    xmlrpc_faultf(envP,
                  "This XML-RPC For C/C++ library was built without Unicode "
                  "wide character capability.  'w' isn't available.");
#endif /* HAVE_UNICODE_WCHAR */
}



static void
destroyDecompTree(struct decompTreeNode * const decompRootP) {

    switch (decompRootP->formatSpecChar) {
    case '(': { 
        unsigned int i;
        for (i = 0; i < decompRootP->store.Tarray.itemCnt; ++i)
            destroyDecompTree(decompRootP->store.Tarray.itemArray[i]);
    } break;
    case '{': {
        unsigned int i;
        for (i = 0; i < decompRootP->store.Tstruct.mbrCnt; ++i)
            destroyDecompTree(
                decompRootP->store.Tstruct.mbrArray[i].decompTreeP);
    } break;
    }

    free(decompRootP);
}



static void
processArraySpecTail(xmlrpc_env *  const envP,
                     const char ** const formatP,
                     bool *        const hasTrailingAsteriskP,
                     char          const delim) {

    if (**formatP == '*') {
        *hasTrailingAsteriskP = true;
        
        ++*formatP;
        
        if (!**formatP)
            xmlrpc_faultf(envP, "missing closing delimiter ('%c')", delim);
        else if (**formatP != delim)
            xmlrpc_faultf(envP, "character following '*' in array "
                          "specification should be the closing delimiter "
                          "'%c', but is '%c'", delim, **formatP);
    } else {
        *hasTrailingAsteriskP = false;
    
        if (!**formatP)
            xmlrpc_faultf(envP, "missing closing delimiter ('%c')", delim);
    }
    if (!envP->fault_occurred)
        XMLRPC_ASSERT(**formatP == delim);
}



static void
buildArrayDecompBranch(xmlrpc_env *            const envP,
                       const char **           const formatP,
                       char                    const delim,
                       va_listx *              const argsP,
                       struct decompTreeNode * const decompNodeP) {
/*----------------------------------------------------------------------------
   Fill in the decomposition tree node *decompNodeP to cover an array
   whose items are described by *formatP.  To wit, they are the values
   described by successive format specifiers in *formatP up to but not
   including the next 'delim' character.

   Plus, the last character before the delimiter might be a '*', which
   means "ignore any additional items in the array."

   We create a node (and whole branch if required) to describe each array
   item.

   The pointers to where those items are to be stored are given by
   'argsP'.

   We advance *formatP to the delimiter character, and advance 'argsP'
   past whatever arguments we use.
-----------------------------------------------------------------------------*/
    unsigned int itemCnt;
        /* Number of array items in the branch so far */

    itemCnt = 0;  /* Branch is empty so far */

    while (**formatP && **formatP != delim && **formatP != '*' &&
           !envP->fault_occurred) {
        if (itemCnt >= ARRAY_SIZE(decompNodeP->store.Tarray.itemArray))
            xmlrpc_faultf(envP, "Too many array items in format string.  "
                          "The most items you can have for an array in "
                          "a format string is %u.",
                          ARRAY_SIZE(decompNodeP->store.Tarray.itemArray));
        else {
            struct decompTreeNode * itemNodeP;
            
            createDecompTreeNext(envP, formatP, argsP, &itemNodeP);
                
            if (!envP->fault_occurred)
                decompNodeP->store.Tarray.itemArray[itemCnt++] = itemNodeP;
        }
    }
    if (!envP->fault_occurred) {
        decompNodeP->store.Tarray.itemCnt = itemCnt;
        processArraySpecTail(envP, formatP,
                             &decompNodeP->store.Tarray.ignoreExcess,
                             delim);
    }
    if (envP->fault_occurred) {
        unsigned int i;
        for (i = 0; i < itemCnt; ++i)
            destroyDecompTree(decompNodeP->store.Tarray.itemArray[i]);
    }
}



static void
doStructValue(xmlrpc_env *       const envP,
              const char **      const formatP,
              va_listx *         const argsP,
              struct mbrDecomp * const mbrP) {

    struct decompTreeNode * valueNodeP;

    mbrP->key = (const char*) va_arg(argsP->v, char*);
        
    createDecompTreeNext(envP, formatP, argsP,  &valueNodeP);
        
    if (!envP->fault_occurred)
        mbrP->decompTreeP = valueNodeP;
}



static void
skipAsterisk(xmlrpc_env *  const envP,
             const char ** const formatP,
             char          const delim) {

    if (**formatP == '*') {
        ++*formatP;

        if (!**formatP)
            xmlrpc_faultf(envP, "missing closing delimiter ('%c')", delim);
        else if (**formatP != delim)
            xmlrpc_faultf(envP, "junk after '*' in the specifier of an "
                          "array.  First character='%c'", **formatP);
    } else
        /* Conceptually, one can make it an error to leave some struct
           members behind, but we have never had code that knows how to
           recognize that case.
        */
        xmlrpc_faultf(envP,
                      "You must put a trailing '*' in the specifiers for "
                      "struct members to signify it's OK if there are "
                      "additional members you didn't get.");
}



static void
skipColon(xmlrpc_env * const envP,
          const char ** const formatP,
          char          const delim) {

    if (**formatP == '\0')
        xmlrpc_faultf(envP, "format string ends in the middle of a struct "
                      "member specifier");
    else if (**formatP == delim)
        xmlrpc_faultf(envP, "member list ends in the middle of a member");
    else if (**formatP != ':')
        xmlrpc_faultf(envP, "In a struct specifier, '%c' found "
                      "where a colon (':') separating key and "
                      "value was expected.", **formatP);
}



static void
skipComma(xmlrpc_env *  const envP,
          const char ** const formatP,
          char          const delim) {

    if (**formatP && **formatP != delim) {
        if (**formatP == ',')
            ++*formatP;  /* skip over comma */
        else
            xmlrpc_faultf(envP, "'%c' where we expected a ',' "
                          "to separate struct members", **formatP);
    }
}



static void
buildStructDecompBranch(xmlrpc_env *            const envP,
                        const char **           const formatP,
                        char                    const delim,
                        va_listx *              const argsP,
                        struct decompTreeNode * const decompNodeP) {
/*----------------------------------------------------------------------------
   Fill in the decomposition tree node *decompNodeP to cover a struct
   whose members are described by *formatP.  To wit, they are the values
   described by successive format specifiers in *formatP up to but not
   including the next 'delim' character.

   We create a node (and whole branch if required) to describe each
   struct member value.

   The pointers to where those values are to be stored are given by
   'argsP'.

   The names of the members to be extracted are also given by 'argsP'.

   We advance *formatP to the delimiter character, and advance 'argsP'
   past whatever arguments we use.
-----------------------------------------------------------------------------*/
    unsigned int memberCnt;
        /* Number of struct members in the branch so far */

    memberCnt = 0;  /* Branch is empty so far */

    while (**formatP && **formatP != delim && **formatP != '*' &&
           !envP->fault_occurred) {
        if (memberCnt >= ARRAY_SIZE(decompNodeP->store.Tstruct.mbrArray))
            xmlrpc_faultf(envP,
                          "Too many structure members in format string.  "
                          "The most members you can specify in "
                          "a format string is %u.",
                          ARRAY_SIZE(decompNodeP->store.Tstruct.mbrArray));
        else {
            struct mbrDecomp * const mbrP =
                &decompNodeP->store.Tstruct.mbrArray[memberCnt];

            if (**formatP != 's')
                xmlrpc_faultf(envP, "In a struct specifier, the specifier "
                              "for the key is '%c', but it must be 's'.",
                              **formatP);
            else {
                ++*formatP;

                skipColon(envP, formatP, delim);

                if (!envP->fault_occurred) {
                    ++*formatP;

                    doStructValue(envP, formatP, argsP, mbrP);
                    
                    if (!envP->fault_occurred)
                        ++memberCnt;

                    skipComma(envP, formatP, delim);
                }                    
            }
        }
    }
    decompNodeP->store.Tstruct.mbrCnt = memberCnt;

    if (!envP->fault_occurred) {
        skipAsterisk(envP, formatP, delim);
        if (!envP->fault_occurred)
            XMLRPC_ASSERT(**formatP == delim);
    }

    if (envP->fault_occurred) {
        unsigned int i;
        for (i = 0; i < memberCnt; ++i)
            destroyDecompTree(
                decompNodeP->store.Tstruct.mbrArray[i].decompTreeP);
    }
}



static void 
createDecompTreeNext(xmlrpc_env *             const envP,
                     const char **            const formatP,
                     va_listx *               const argsP,
                     struct decompTreeNode ** const decompNodePP) {
/*----------------------------------------------------------------------------
   Create a branch of a decomposition tree that applies to the first
   value described by '*formatP', and advance *formatP past the description
   of that first value.  E.g.:

     - If *formatP is "isb", we create a branch consisting of one
       node -- for an integer.  We advance *formatP by one character, so
       it points to the "s".

     - If *formatP is "(isb)s", we create a branch that represents the
       array (isb) and advance *formatP past the closing parenthesis to
       point to the final "s".  We return as *decompNodePP a pointer to
       a node for the array, and that array in turn points to nodes for
       each of the 3 array items:  one for an integer, one for a string,
       and one for a boolean.

   The locations at which the components of that value are to be
   stored (which is the main contents of the branch we create) are
   given by 'argsP'.

   Return as *decompNodeP a pointer to the root node of the branch we
   generate.
-----------------------------------------------------------------------------*/
    struct decompTreeNode * decompNodeP;

    MALLOCVAR(decompNodeP);

    if (decompNodeP == NULL)
        xmlrpc_faultf(envP, "Could not allocate space for a decomposition "
                      "tree node");
    else {
        decompNodeP->formatSpecChar = *(*formatP)++;
        
        switch (decompNodeP->formatSpecChar) {
        case '-':
            /* There's nothing to store */
            break;
        case 'i':
            decompNodeP->store.Tinteger.valueP =
                (xmlrpc_int32*) va_arg(argsP->v, xmlrpc_int32*);
            break;
            
        case 'b':
            decompNodeP->store.Tbool.valueP =
                (xmlrpc_bool*) va_arg(argsP->v, xmlrpc_bool*);
            break;
            
        case 'd':
            decompNodeP->store.Tdouble.valueP =
                (double*) va_arg(argsP->v, double*);
            break;
            
        case 't':
            decompNodeP->store.TdatetimeT.valueP =
                va_arg(argsP->v, time_t*);
            break;

        case '8':
            decompNodeP->store.Tdatetime8.valueP =
                (const char**) va_arg(argsP->v, char**);
            break;

        case 's':
            decompNodeP->store.Tstring.valueP = 
                (const char**) va_arg(argsP->v, char**);
            if (**formatP == '#') {
                decompNodeP->store.Tstring.sizeP = 
                    (size_t*) va_arg(argsP->v, size_t**);
                ++*formatP;
            } else
                decompNodeP->store.Tstring.sizeP = NULL;
            break;

        case 'w':
            buildWideStringNode(envP, formatP, argsP, decompNodeP);
            break;
        
        case '6':
            decompNodeP->store.TbitString.valueP =
                (const unsigned char**) va_arg(argsP->v, unsigned char**);
            decompNodeP->store.TbitString.sizeP =
                (size_t*) va_arg(argsP->v, size_t**);        
            break;

        case 'n':
            /* There's no value to store */
            break;

        case 'I':
            decompNodeP->store.Ti8.valueP =
                (xmlrpc_int64 *) va_arg(argsP->v, xmlrpc_int64 *);
            break;
            
        case 'p':
            decompNodeP->store.Tcptr.valueP =
                (void**) va_arg(argsP->v, void**);
            break;

        case 'V':
            decompNodeP->store.Tvalue.valueP =
                (xmlrpc_value**) va_arg(argsP->v, xmlrpc_value**);
            break;

        case 'A':
            decompNodeP->store.TarrayVal.valueP =
                (xmlrpc_value**) va_arg(argsP->v, xmlrpc_value**);
            break;

        case 'S':
            decompNodeP->store.TstructVal.valueP =
                (xmlrpc_value**) va_arg(argsP->v, xmlrpc_value**);
            break;

        case '(':
            buildArrayDecompBranch(envP, formatP, ')', argsP, decompNodeP);
            ++(*formatP);  /* skip past closing ')' */
            break;

        case '{':
            buildStructDecompBranch(envP, formatP, '}', argsP, decompNodeP);
            ++(*formatP);  /* skip past closing '}' */
            break;

        default:
            xmlrpc_faultf(envP, "Invalid format character '%c'",
                          decompNodeP->formatSpecChar);
        }
        if (envP->fault_occurred)
            free(decompNodeP);
        else
            *decompNodePP = decompNodeP;
    }
}



static void
createDecompTree(xmlrpc_env *             const envP,
                 const char *             const format,
                 va_listx                 const args,
                 struct decompTreeNode ** const decompRootPP) {

    const char * formatCursor;
    struct decompTreeNode * decompRootP;
    va_listx currentArgs;

    currentArgs = args;
    formatCursor = &format[0];
    createDecompTreeNext(envP, &formatCursor, &currentArgs, &decompRootP);
    if (!envP->fault_occurred) {
        if (*formatCursor != '\0')
            xmlrpc_faultf(envP, "format string '%s' has garbage at the end: "
                          "'%s'.  It should be a specifier of a single value "
                          "(but that might be a complex value, such as an "
                          "array)", format, formatCursor);

        if (envP->fault_occurred)
            destroyDecompTree(decompRootP);
        else
            *decompRootPP = decompRootP;
    }
}



static void 
decomposeValue(xmlrpc_env *   const envP,
               xmlrpc_value * const valueP,
               bool           const oldstyleMemMgmt,
               const char *   const format,
               va_listx       const args) {

    struct decompTreeNode * decompRootP;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_VALUE_OK(valueP);
    XMLRPC_ASSERT(format != NULL);

    createDecompTree(envP, format, args, &decompRootP);

    if (!envP->fault_occurred) {
        decomposeValueWithTree(envP, valueP, oldstyleMemMgmt, decompRootP);

        destroyDecompTree(decompRootP);
    }
}



void 
xmlrpc_decompose_value_va(xmlrpc_env *   const envP,
                          xmlrpc_value * const valueP,
                          const char *   const format,
                          va_list        const args) {

    bool const oldstyleMemMgtFalse = false;
    va_listx argsx;

    init_va_listx(&argsx, args);

    decomposeValue(envP, valueP, oldstyleMemMgtFalse, format, argsx);
}



void 
xmlrpc_decompose_value(xmlrpc_env *   const envP,
                       xmlrpc_value * const value,
                       const char *   const format, 
                       ...) {

    va_list args;

    va_start(args, format);
    xmlrpc_decompose_value_va(envP, value, format, args);
    va_end(args);
}



void 
xmlrpc_parse_value_va(xmlrpc_env *   const envP,
                      xmlrpc_value * const valueP,
                      const char *   const format,
                      va_list        const args) {

    bool const oldstyleMemMgmtTrue = true;
    va_listx argsx;

    init_va_listx(&argsx, args);

    decomposeValue(envP, valueP, oldstyleMemMgmtTrue, format, argsx);
}



void 
xmlrpc_parse_value(xmlrpc_env *   const envP,
                   xmlrpc_value * const value,
                   const char *   const format, 
                   ...) {

    va_list args;

    va_start(args, format);
    xmlrpc_parse_value_va(envP, value, format, args);
    va_end(args);
}
