/* Copyright information is at the end of the file */

/*=========================================================================
**  XML-RPC Array Functions
**=========================================================================
*/

#include "xmlrpc_config.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#include "xmlrpc-c/util.h"
#include "xmlrpc-c/base.h"
#include "xmlrpc-c/base_int.h"



void
xmlrpc_abort_if_array_bad(xmlrpc_value * const arrayP) {

    if (arrayP == NULL)
        abort();
    else if (arrayP->_type != XMLRPC_TYPE_ARRAY)
        abort();
    else {
        size_t const arraySize =
            XMLRPC_MEMBLOCK_SIZE(xmlrpc_value*, &arrayP->_block);
        xmlrpc_value ** const contents = 
            XMLRPC_MEMBLOCK_CONTENTS(xmlrpc_value*, &arrayP->_block);
        
        if (contents == NULL)
            abort();
        else {
            size_t index;
            
            for (index = 0; index < arraySize; ++index) {
                xmlrpc_value * const itemP = contents[index];
                if (itemP == NULL)
                    abort();
                else if (itemP->_refcount < 1)
                    abort();
            }
        }
    }
}



void
xmlrpc_destroyArrayContents(xmlrpc_value * const arrayP) {
/*----------------------------------------------------------------------------
   Dispose of the contents of an array (but not the array value itself).
   The value is not valid after this.
-----------------------------------------------------------------------------*/
    size_t const arraySize =
        XMLRPC_MEMBLOCK_SIZE(xmlrpc_value*, &arrayP->_block);
    xmlrpc_value ** const contents = 
        XMLRPC_MEMBLOCK_CONTENTS(xmlrpc_value*, &arrayP->_block);

    size_t index;
    
    XMLRPC_ASSERT_ARRAY_OK(arrayP);

    /* Release our reference to each item in the array */
    for (index = 0; index < arraySize; ++index) {
        xmlrpc_value * const itemP = contents[index];
        xmlrpc_DECREF(itemP);
    }
    XMLRPC_MEMBLOCK_CLEAN(xmlrpc_value *, &arrayP->_block);
}



int 
xmlrpc_array_size(xmlrpc_env *         const envP,
                  const xmlrpc_value * const arrayP) {

    int retval;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_VALUE_OK(arrayP);

    if (arrayP->_type != XMLRPC_TYPE_ARRAY) {
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_TYPE_ERROR, "Value is not an array");
        retval = -1;
    } else {
        size_t const size =
            XMLRPC_MEMBLOCK_SIZE(xmlrpc_value *, &arrayP->_block);

        assert((size_t)(int)(size) == size);

        retval = (int)size;
    }
    return retval;
}



void 
xmlrpc_array_append_item(xmlrpc_env *   const envP,
                         xmlrpc_value * const arrayP,
                         xmlrpc_value * const valueP) {

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_VALUE_OK(arrayP);
    
    if (xmlrpc_value_type(arrayP) != XMLRPC_TYPE_ARRAY)
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_TYPE_ERROR, "Value is not an array");
    else {
        size_t const size = 
            XMLRPC_MEMBLOCK_SIZE(xmlrpc_value *, &arrayP->_block);

        XMLRPC_MEMBLOCK_RESIZE(xmlrpc_value *, envP, &arrayP->_block, size+1);

        if (!envP->fault_occurred) {
            xmlrpc_value ** const contents =
                XMLRPC_MEMBLOCK_CONTENTS(xmlrpc_value*, &arrayP->_block);
            xmlrpc_INCREF(valueP);
            contents[size] = valueP;
        }
    }
}



void
xmlrpc_array_read_item(xmlrpc_env *         const envP,
                       const xmlrpc_value * const arrayP,
                       unsigned int         const index,
                       xmlrpc_value **      const valuePP) {

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_VALUE_OK(arrayP);
    XMLRPC_ASSERT_PTR_OK(valuePP);

    if (arrayP->_type != XMLRPC_TYPE_ARRAY)
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_TYPE_ERROR, "Attempt to read array item from "
            "a value that is not an array");
    else {
        xmlrpc_value ** const contents = 
            XMLRPC_MEMBLOCK_CONTENTS(xmlrpc_value *, &arrayP->_block);
        size_t const size = 
            XMLRPC_MEMBLOCK_SIZE(xmlrpc_value *, &arrayP->_block);

        if (index >= size)
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_INDEX_ERROR, "Array index %u is beyond end "
                "of %u-item array", index, (unsigned int)size);
        else {
            *valuePP = contents[index];
            xmlrpc_INCREF(*valuePP);
        }
    }
}



xmlrpc_value * 
xmlrpc_array_get_item(xmlrpc_env *         const envP,
                      const xmlrpc_value * const arrayP,
                      int                  const index) {

    xmlrpc_value * valueP;

    if (index < 0)
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_INDEX_ERROR, "Index %d is negative.", index);
    else {
        xmlrpc_array_read_item(envP, arrayP, index, &valueP);

        if (!envP->fault_occurred)
            xmlrpc_DECREF(valueP);
    }
    if (envP->fault_occurred)
        valueP = NULL;
        
    return valueP;
}



xmlrpc_value *
xmlrpc_array_new(xmlrpc_env * const envP) {
/*----------------------------------------------------------------------------
   Create an empty array xmlrpc_value.
-----------------------------------------------------------------------------*/
    xmlrpc_value * arrayP;

    xmlrpc_createXmlrpcValue(envP, &arrayP);
    if (!envP->fault_occurred) {
        arrayP->_type = XMLRPC_TYPE_ARRAY;
        XMLRPC_MEMBLOCK_INIT(xmlrpc_value*, envP, &arrayP->_block, 0);
        if (envP->fault_occurred)
            free(arrayP);
    }
    return arrayP;
}



/* Copyright (C) 2001 by First Peer, Inc. All rights reserved.
** Copyright (C) 2001 by Eric Kidd. All rights reserved.
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
