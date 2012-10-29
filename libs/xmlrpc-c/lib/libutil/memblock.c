/* Copyright information is at end of file */
#include "xmlrpc_config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "mallocvar.h"
#include "xmlrpc-c/util_int.h"
#include "xmlrpc-c/util.h"

#ifdef EFENCE
        /* when looking for corruption don't allocate extra slop */
#define BLOCK_ALLOC_MIN (1)
#else
#define BLOCK_ALLOC_MIN (16)
#endif
#define BLOCK_ALLOC_MAX (128 * 1024 * 1024)


xmlrpc_mem_block * 
xmlrpc_mem_block_new(xmlrpc_env * const envP, 
                     size_t       const size) {

    xmlrpc_mem_block * block;

    XMLRPC_ASSERT_ENV_OK(envP);

    MALLOCVAR(block);
    
    if (block == NULL)
        xmlrpc_faultf(envP, "Can't allocate memory block");
    else {
        xmlrpc_mem_block_init(envP, block, size);

        if (envP->fault_occurred) {
            free(block);
            block = NULL;
        }
    }
    return block;
}



/* Destroy an existing xmlrpc_mem_block, and everything it contains. */
void
xmlrpc_mem_block_free(xmlrpc_mem_block * const blockP) {

    XMLRPC_ASSERT(blockP != NULL);
    XMLRPC_ASSERT(blockP->_block != NULL);

    xmlrpc_mem_block_clean(blockP);
    free(blockP);
}



/* Initialize the contents of the provided xmlrpc_mem_block. */
void
xmlrpc_mem_block_init(xmlrpc_env *       const envP,
                      xmlrpc_mem_block * const blockP,
                      size_t             const size) {

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT(blockP != NULL);

    blockP->_size = size;
    if (size < BLOCK_ALLOC_MIN)
        blockP->_allocated = BLOCK_ALLOC_MIN;
    else
        blockP->_allocated = size;

    blockP->_block = (void*) malloc(blockP->_allocated);
    if (!blockP->_block)
        xmlrpc_faultf(envP, "Can't allocate %u-byte memory block",
                      (unsigned)blockP->_allocated);
}



/* Deallocate the contents of the provided xmlrpc_mem_block, but not
   the block itself.
*/
void
xmlrpc_mem_block_clean(xmlrpc_mem_block * const blockP) {

    XMLRPC_ASSERT(blockP != NULL);
    XMLRPC_ASSERT(blockP->_block != NULL);

    free(blockP->_block);
    blockP->_block = XMLRPC_BAD_POINTER;
}



/* Get the size of the xmlrpc_mem_block. */
size_t 
xmlrpc_mem_block_size(const xmlrpc_mem_block * const blockP) {

    XMLRPC_ASSERT(blockP != NULL);
    return blockP->_size;
}



/* Get the contents of the xmlrpc_mem_block. */
void * 
xmlrpc_mem_block_contents(const xmlrpc_mem_block * const blockP) {

    XMLRPC_ASSERT(blockP != NULL);
    return blockP->_block;
}



/* Resize an xmlrpc_mem_block, preserving as much of the contents as
   possible.
*/
void 
xmlrpc_mem_block_resize (xmlrpc_env *       const envP,
                         xmlrpc_mem_block * const blockP,
                         size_t             const size) {

    size_t proposed_alloc;
    void* new_block;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT(blockP != NULL);

    /* Check to see if we already have enough space. Maybe we'll get lucky. */
    if (size <= blockP->_allocated) {
        blockP->_size = size;
        return;
    }

    /* Calculate a new allocation size. */
#ifdef EFENCE
    proposed_alloc = size;
#else
    proposed_alloc = blockP->_allocated;
    while (proposed_alloc < size && proposed_alloc <= BLOCK_ALLOC_MAX)
        proposed_alloc *= 2;
#endif /* DEBUG_MEM_ERRORS */

    if (proposed_alloc > BLOCK_ALLOC_MAX)
        XMLRPC_FAIL(envP, XMLRPC_INTERNAL_ERROR, "Memory block too large");

    /* Allocate our new memory block. */
    new_block = (void*) malloc(proposed_alloc);
    XMLRPC_FAIL_IF_NULL(new_block, envP, XMLRPC_INTERNAL_ERROR,
                        "Can't resize memory block");

    /* Copy over our data and update the xmlrpc_mem_block struct. */
    memcpy(new_block, blockP->_block, blockP->_size);
    free(blockP->_block);
    blockP->_block     = new_block;
    blockP->_size      = size;
    blockP->_allocated = proposed_alloc;

 cleanup:
    return;
}



void 
xmlrpc_mem_block_append(xmlrpc_env *       const envP,
                        xmlrpc_mem_block * const blockP,
                        const void *       const data, 
                        size_t             const len) {

    size_t const originalSize = blockP->_size;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT(blockP != NULL);

    xmlrpc_mem_block_resize(envP, blockP, originalSize + len);
    if (!envP->fault_occurred) {
        memcpy(((unsigned char*) blockP->_block) + originalSize, data, len);
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
** SUCH DAMAGE.
*/
