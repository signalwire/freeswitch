/* Copyright information is at end of file */

#include "xmlrpc_config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <xmlparse.h> /* Expat */

#include "bool.h"

#include "xmlrpc-c/base.h"
#include "xmlrpc-c/base_int.h"
#include "xmlrpc-c/string_int.h"
#include "xmlrpc-c/xmlparser.h"

/* Define the contents of our internal structure. */
struct _xml_element {
    struct _xml_element *_parent;
    char *_name;
    xmlrpc_mem_block _cdata;    /* char */
    xmlrpc_mem_block _children; /* xml_element* */
};

/* Check that we're using expat in UTF-8 mode, not wchar_t mode.
** If you need to use expat in wchar_t mode, write a subroutine to
** copy a wchar_t string to a char string & return an error for
** any non-ASCII characters. Then call this subroutine on all
** XML_Char strings passed to our event handlers before using the
** data. */
/* #if sizeof(char) != sizeof(XML_Char)
** #error expat must define XML_Char to be a regular char. 
** #endif
*/

#define XMLRPC_ASSERT_ELEM_OK(elem) \
    XMLRPC_ASSERT((elem) != NULL && (elem)->_name != XMLRPC_BAD_POINTER)


/*=========================================================================
**  xml_element_new
**=========================================================================
**  Create a new xml_element. This routine isn't exported, because the
**  arguments are implementation-dependent.
*/

static xml_element *
xml_element_new (xmlrpc_env * const env,
                 const char * const name) {

    xml_element *retval;
    int name_valid, cdata_valid, children_valid;

    XMLRPC_ASSERT_ENV_OK(env);
    XMLRPC_ASSERT(name != NULL);

    /* Set up our error-handling preconditions. */
    retval = NULL;
    name_valid = cdata_valid = children_valid = 0;

    /* Allocate our xml_element structure. */
    retval = (xml_element*) malloc(sizeof(xml_element));
    XMLRPC_FAIL_IF_NULL(retval, env, XMLRPC_INTERNAL_ERROR,
                        "Couldn't allocate memory for XML element");

    /* Set our parent field to NULL. */
    retval->_parent = NULL;
    
    /* Copy over the element name. */
    retval->_name = (char*) malloc(strlen(name) + 1);
    XMLRPC_FAIL_IF_NULL(retval->_name, env, XMLRPC_INTERNAL_ERROR,
                        "Couldn't allocate memory for XML element");
    name_valid = 1;
    strcpy(retval->_name, name);

    /* Initialize a block to hold our CDATA. */
    XMLRPC_TYPED_MEM_BLOCK_INIT(char, env, &retval->_cdata, 0);
    XMLRPC_FAIL_IF_FAULT(env);
    cdata_valid = 1;

    /* Initialize a block to hold our child elements. */
    XMLRPC_TYPED_MEM_BLOCK_INIT(xml_element*, env, &retval->_children, 0);
    XMLRPC_FAIL_IF_FAULT(env);
    children_valid = 1;

 cleanup:
    if (env->fault_occurred) {
        if (retval) {
            if (name_valid)
                free(retval->_name);
            if (cdata_valid)
                xmlrpc_mem_block_clean(&retval->_cdata);
            if (children_valid)
                xmlrpc_mem_block_clean(&retval->_children);
            free(retval);
        }
        return NULL;
    } else {
        return retval;
    }
}


/*=========================================================================
**  xml_element_free
**=========================================================================
**  Blow away an existing element & all of its child elements.
*/
void
xml_element_free(xml_element * const elemP) {

    xmlrpc_mem_block * childrenP;
    size_t size, i;
    xml_element ** contents;

    XMLRPC_ASSERT_ELEM_OK(elemP);

    free(elemP->_name);
    elemP->_name = XMLRPC_BAD_POINTER;
    XMLRPC_MEMBLOCK_CLEAN(xml_element *, &elemP->_cdata);

    /* Deallocate all of our children recursively. */
    childrenP = &elemP->_children;
    contents = XMLRPC_MEMBLOCK_CONTENTS(xml_element *, childrenP);
    size = XMLRPC_MEMBLOCK_SIZE(xml_element *, childrenP);
    for (i = 0; i < size; ++i)
        xml_element_free(contents[i]);

    XMLRPC_MEMBLOCK_CLEAN(xml_element *, &elemP->_children);

    free(elemP);
}


/*=========================================================================
**  Miscellaneous Accessors
**=========================================================================
**  Return the fields of the xml_element. See the header for more
**  documentation on each function works.
*/



const char *
xml_element_name(const xml_element * const elemP) {

    XMLRPC_ASSERT_ELEM_OK(elemP);
    return elemP->_name;
}



/* The result of this function is NOT VALID until the end_element handler
** has been called! */
size_t xml_element_cdata_size (xml_element *elem)
{
    XMLRPC_ASSERT_ELEM_OK(elem);
    return XMLRPC_TYPED_MEM_BLOCK_SIZE(char, &elem->_cdata) - 1;
}

char *xml_element_cdata (xml_element *elem)
{
    XMLRPC_ASSERT_ELEM_OK(elem);
    return XMLRPC_TYPED_MEM_BLOCK_CONTENTS(char, &elem->_cdata);
}



size_t
xml_element_children_size(const xml_element * const elemP) {
    XMLRPC_ASSERT_ELEM_OK(elemP);
    return XMLRPC_TYPED_MEM_BLOCK_SIZE(xml_element *, &elemP->_children);
}



xml_element **
xml_element_children(const xml_element * const elemP) {
    XMLRPC_ASSERT_ELEM_OK(elemP);
    return XMLRPC_TYPED_MEM_BLOCK_CONTENTS(xml_element *, &elemP->_children);
}



/*=========================================================================
**  Internal xml_element Utility Functions
**=========================================================================
*/

static void xml_element_append_cdata (xmlrpc_env *env,
                                      xml_element *elem,
                                      char *cdata,
                                      size_t size)
{
    XMLRPC_ASSERT_ENV_OK(env);
    XMLRPC_ASSERT_ELEM_OK(elem);    

    XMLRPC_TYPED_MEM_BLOCK_APPEND(char, env, &elem->_cdata, cdata, size);
}

/* Whether or not this function succeeds, it takes ownership of the 'child'
** argument.
** WARNING - This is the exact opposite of the usual memory ownership
** rules for xmlrpc_value! So please pay attention. */
static void xml_element_append_child (xmlrpc_env *env,
                                      xml_element *elem,
                                      xml_element *child)
{
    XMLRPC_ASSERT_ENV_OK(env);
    XMLRPC_ASSERT_ELEM_OK(elem);
    XMLRPC_ASSERT_ELEM_OK(child);
    XMLRPC_ASSERT(child->_parent == NULL);

    XMLRPC_TYPED_MEM_BLOCK_APPEND(xml_element*, env, &elem->_children,
                                  &child, 1);
    if (!env->fault_occurred)
        child->_parent = elem;
    else
        xml_element_free(child);
}


/*=========================================================================
**  Our parse context. We pass this around as expat user data.
**=========================================================================
*/

typedef struct {
    xmlrpc_env env;
    xml_element * rootP;
    xml_element * currentP;
} parseContext;


/*=========================================================================
**  Expat Event Handler Functions
**=========================================================================
*/

static void
startElement(void *      const userData,
             XML_Char *  const name,
             XML_Char ** const atts ATTR_UNUSED) {

    parseContext * const contextP = userData;

    XMLRPC_ASSERT(contextP != NULL);
    XMLRPC_ASSERT(name != NULL);

    if (!contextP->env.fault_occurred) {
        xml_element * elemP;

        elemP = xml_element_new(&contextP->env, name);
        if (!contextP->env.fault_occurred) {
            XMLRPC_ASSERT(elemP != NULL);

            /* Insert the new element in the appropriate place. */
            if (!contextP->rootP) {
                /* No root yet, so this element must be the root. */
                contextP->rootP = elemP;
                contextP->currentP = elemP;
            } else {
                XMLRPC_ASSERT(contextP->currentP != NULL);

                /* (We need to watch our error handling invariants
                   very carefully here. Read the docs for
                   xml_element_append_child.
                */
                xml_element_append_child(&contextP->env, contextP->currentP,
                                         elemP);
                if (!contextP->env.fault_occurred)
                    contextP->currentP = elemP;
            }
            if (contextP->env.fault_occurred)
                xml_element_free(elemP);
        }
        if (contextP->env.fault_occurred) {
            /* Having changed *contextP to reflect failure, we are responsible
               for undoing everything that has been done so far in this
               context.
            */
            if (contextP->rootP)
                xml_element_free(contextP->rootP);
        }
    }
}



static void
endElement(void *     const userData,
           XML_Char * const name ATTR_UNUSED) {

    parseContext * const contextP = userData;

    XMLRPC_ASSERT(contextP != NULL);
    XMLRPC_ASSERT(name != NULL);

    if (!contextP->env.fault_occurred) {
        /* I think Expat enforces these facts: */
        XMLRPC_ASSERT(xmlrpc_streq(name, contextP->currentP->_name));
        XMLRPC_ASSERT(contextP->currentP->_parent != NULL ||
                      contextP->currentP == contextP->rootP);

        /* Add a trailing NUL to our cdata. */
        xml_element_append_cdata(&contextP->env, contextP->currentP, "\0", 1);
        if (!contextP->env.fault_occurred)
            /* Pop our "stack" of elements. */
            contextP->currentP = contextP->currentP->_parent;

        if (contextP->env.fault_occurred) {
            /* Having changed *contextP to reflect failure, we are responsible
               for undoing everything that has been done so far in this
               context.
            */
            if (contextP->rootP)
                xml_element_free(contextP->rootP);
        }
    }
}



static void
characterData(void *     const userData,
              XML_Char * const s,
              int        const len) {
/*----------------------------------------------------------------------------
   This is an Expat character data (cdata) handler.  When an Expat
   parser comes across cdata, he calls one of these with the cdata as
   argument.  He can call it multiple times for consecutive cdata.

   We simply append the cdata to the cdata buffer for whatever XML
   element the parser is presently parsing.
-----------------------------------------------------------------------------*/
    parseContext * const contextP = userData;

    XMLRPC_ASSERT(contextP != NULL);
    XMLRPC_ASSERT(s != NULL);
    XMLRPC_ASSERT(len >= 0);

    if (!contextP->env.fault_occurred) {
        XMLRPC_ASSERT(contextP->currentP != NULL);
    
        xml_element_append_cdata(&contextP->env, contextP->currentP, s, len);
    }
}



static void
createParser(xmlrpc_env *   const envP,
             parseContext * const contextP,
             XML_Parser *   const parserP) {
/*----------------------------------------------------------------------------
   Create an Expat parser to parse our XML.
-----------------------------------------------------------------------------*/
    XML_Parser parser;

    parser = xmlrpc_XML_ParserCreate(NULL);
    if (parser == NULL)
        xmlrpc_faultf(envP, "Could not create expat parser");
    else {
        /* Initialize our parse context. */
        xmlrpc_env_init(&contextP->env);
        contextP->rootP    = NULL;
        contextP->currentP = NULL;

        xmlrpc_XML_SetUserData(parser, contextP);
        xmlrpc_XML_SetElementHandler(
            parser,
            (XML_StartElementHandler) startElement,
            (XML_EndElementHandler) endElement);
        xmlrpc_XML_SetCharacterDataHandler(
            parser,
            (XML_CharacterDataHandler) characterData);
    }
    *parserP = parser;
}



static void
destroyParser(XML_Parser     const parser,
              parseContext * const contextP) {

    xmlrpc_env_clean(&contextP->env);

    xmlrpc_XML_ParserFree(parser);
}



void
xml_parse(xmlrpc_env *   const envP,
          const char *   const xmlData,
          size_t         const xmlDataLen,
          xml_element ** const resultPP) {
/*----------------------------------------------------------------------------
  Parse the XML text 'xmlData', of length 'xmlDataLen'.  Return the
  description of the element that the XML text contains as *resultPP.
-----------------------------------------------------------------------------*/
    /* 
       This is an Expat driver.
   
       We set up event-based parser handlers for Expat and set Expat loose
       on the XML.  Expat walks through the XML, calling our handlers along
       the way.  Our handlers build up the element description in our
       'context' variable, so that when Expat is finished, our results are
       in 'context' and we just have to pluck them out.

       We should allow the user to specify the encoding in 'xmlData', but
       we don't.
    */
    XML_Parser parser;
    parseContext context;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT(xmlData != NULL);

    createParser(envP, &context, &parser);

    if (!envP->fault_occurred) {
        bool ok;

        ok = xmlrpc_XML_Parse(parser, xmlData, xmlDataLen, 1);
            /* sets 'context', *envP */
        if (!ok) {
            /* Expat failed on its own to parse it -- this is not an error
               that our handlers detected.
            */
            xmlrpc_env_set_fault(
                envP, XMLRPC_PARSE_ERROR,
                xmlrpc_XML_GetErrorString(parser));
            if (!context.env.fault_occurred) {
                /* Have to clean up what our handlers built before Expat
                   barfed.
                */
                if (context.rootP)
                    xml_element_free(context.rootP);
            }
        } else {
            /* Expat got through the XML OK, but when it called our handlers,
               they might have detected a problem.  They would have noted
               such a problem in *contextP.
            */
            if (context.env.fault_occurred)
                xmlrpc_env_set_fault_formatted(
                    envP, context.env.fault_code,
                    "XML doesn't parse.  %s", context.env.fault_string);
            else {
                XMLRPC_ASSERT(context.rootP != NULL);
                XMLRPC_ASSERT(context.currentP == NULL);
                
                *resultPP = context.rootP;
            }
        }
        destroyParser(parser, &context);
    }
}


/* Copyright (C) 2001 by First Peer, Inc. All rights reserved.
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
** SUCH DAMAGE. */
