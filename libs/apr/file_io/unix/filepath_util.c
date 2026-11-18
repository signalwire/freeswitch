/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#define APR_WANT_STRFUNC
#define APR_WANT_MEMFUNC
#include "fspr_want.h"

#include "fspr_errno.h"
#include "fspr_pools.h"
#include "fspr_strings.h"
#include "fspr_tables.h"

#include "fspr_private.h"

fspr_status_t fspr_filepath_list_split_impl(fspr_array_header_t **pathelts,
                                          const char *liststr,
                                          char separator,
                                          fspr_pool_t *p)
{
    char *path, *part, *ptr;
    char separator_string[2] = { '\0', '\0' };
    fspr_array_header_t *elts;
    int nelts;

    separator_string[0] = separator;
    /* Count the number of path elements. We know there'll be at least
       one even if path is an empty string. */
    path = fspr_pstrdup(p, liststr);
    for (nelts = 0, ptr = path; ptr != NULL; ++nelts)
    {
        ptr = strchr(ptr, separator);
        if (ptr)
            ++ptr;
    }

    /* Split the path into the array. */
    elts = fspr_array_make(p, nelts, sizeof(char*));
    while ((part = fspr_strtok(path, separator_string, &ptr)) != NULL)
    {
        if (*part == '\0')      /* Ignore empty path components. */
            continue;

        *(char**)fspr_array_push(elts) = part;
        path = NULL;            /* For the next call to fspr_strtok */
    }

    *pathelts = elts;
    return APR_SUCCESS;
}


fspr_status_t fspr_filepath_list_merge_impl(char **liststr,
                                          fspr_array_header_t *pathelts,
                                          char separator,
                                          fspr_pool_t *p)
{
    fspr_size_t path_size = 0;
    char *path;
    int i;

    /* This test isn't 100% certain, but it'll catch at least some
       invalid uses... */
    if (pathelts->elt_size != sizeof(char*))
        return APR_EINVAL;

    /* Calculate the size of the merged path */
    for (i = 0; i < pathelts->nelts; ++i)
        path_size += strlen(((char**)pathelts->elts)[i]);

    if (path_size == 0)
    {
        *liststr = NULL;
        return APR_SUCCESS;
    }

    if (i > 0)                  /* Add space for the separators */
        path_size += (i - 1);

    /* Merge the path components */
    path = *liststr = fspr_palloc(p, path_size + 1);
    for (i = 0; i < pathelts->nelts; ++i)
    {
        /* ### Hmmmm. Calling strlen twice on the same string. Yuck.
               But is is better than reallocation in fspr_pstrcat? */
        const char *part = ((char**)pathelts->elts)[i];
        fspr_size_t part_size = strlen(part);
        if (part_size == 0)     /* Ignore empty path components. */
            continue;

        if (i > 0)
            *path++ = separator;
        memcpy(path, part, part_size);
        path += part_size;
    }
    *path = '\0';
    return APR_SUCCESS;
}
