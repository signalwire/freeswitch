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

#ifndef ANNOTATEMORE_PARSER_H

#define ANNOTATEMORE_PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mailimap_parser.h"
#include "annotatemore_types.h"

int
mailimap_annotatemore_annotate_data_parse(mailstream * fd, MMAPString *buffer,
    size_t * index,
    struct mailimap_annotatemore_annotate_data ** result,
    size_t progr_rate,
    progress_function * progr_fun);

int
mailimap_annotatemore_entry_list_parse(mailstream * fd, MMAPString *buffer,
    size_t * index,
    struct mailimap_annotatemore_entry_list ** result,
    size_t progr_rate,
    progress_function * progr_fun);

int
mailimap_annotatemore_entry_att_parse(mailstream * fd, MMAPString *buffer,
    size_t * index,
    struct mailimap_annotatemore_entry_att ** result,
    size_t progr_rate,
    progress_function * progr_fun);

int
mailimap_annotatemore_att_value_parse(mailstream * fd, MMAPString *buffer,
    size_t * index,
    struct mailimap_annotatemore_att_value ** result,
    size_t progr_rate,
    progress_function * progr_fun);

int
mailimap_annotatemore_attrib_parse(mailstream * fd, MMAPString *buffer,
    size_t * index, char ** result,
    size_t progr_rate,
    progress_function * progr_fun);

int
mailimap_annotatemore_value_parse(mailstream * fd, MMAPString *buffer,
    size_t * index, char ** result,
    size_t progr_rate,
    progress_function * progr_fun);

int
mailimap_annotatemore_entry_parse(mailstream * fd, MMAPString *buffer,
    size_t * index, char ** result,
    size_t progr_rate,
    progress_function * progr_fun);

int
mailimap_annotatemore_text_code_annotatemore_parse(mailstream * fd,
    MMAPString *buffer, size_t * index, int * result,
    size_t progr_rate, progress_function * progr_fun);

int mailimap_annotatemore_parse(int calling_parser, mailstream * fd,
    MMAPString * buffer, size_t * index,
    struct mailimap_extension_data ** result,
    size_t progr_rate,
    progress_function * progr_fun);

#ifdef __cplusplus
}
#endif

#endif
