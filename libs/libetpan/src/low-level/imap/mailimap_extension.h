/*
 * libEtPan! -- a mail stuff library
 *
 * Copyright (C) 2001, 2005 - DINH Viet Hoa
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the libEtPan! project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef MAILIMAP_EXTENSION_H

#define MAILIMAP_EXTENSION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libetpan/mailimap_types.h>
#include <libetpan/mailimap_extension_types.h>

/*
  you add a (static) mailimap_extension_api to the list of extensions
  by calling register. making the list of
  extensions contain all extensions statically may prove detrimental
  to speed if you have many extensions and don't need any of them.
  as unregistering single extensions does not really make any sense,
  it's not provided - just an unregister_all which is primarily used
  to free the clist on exit.
*/

LIBETPAN_EXPORT
int
mailimap_extension_register(struct mailimap_extension_api * extension);

LIBETPAN_EXPORT
void
mailimap_extension_unregister_all(void);

/*
  this is called as the main parser wrapper for all extensions.
  it gos through the list of registered extensions and calls
  all of the extensions' parsers looking for one that doesn't
  return MAILIMAP_ERROR_PARSE.
*/
LIBETPAN_EXPORT
int
mailimap_extension_data_parse(int calling_parser,
        mailstream * fd, MMAPString * buffer,
        size_t * index, struct mailimap_extension_data ** result,
        size_t progr_rate,
        progress_function * progr_fun);

LIBETPAN_EXPORT
struct mailimap_extension_data *
mailimap_extension_data_new(struct mailimap_extension_api * extension,
        int type, void * data);

/*
  wrapper for the extensions' free. calls the correct extension's free
  based on data->extension.
*/
LIBETPAN_EXPORT
void
mailimap_extension_data_free(struct
        mailimap_extension_data * data);

/*
  stores the ext_data in the session (only needed for extensions
  that embed directly into response-data).
*/
void mailimap_extension_data_store(mailimap * session,
    struct mailimap_extension_data ** ext_data);

/*
  return 1 if the extension of the given name is supported.
  the name is searched in the capabilities.
*/

LIBETPAN_EXPORT
int mailimap_has_extension(mailimap * session, char * extension_name);

#ifdef __cplusplus
}
#endif

#endif
