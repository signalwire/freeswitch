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

/*
 * $Id: mailthread.h,v 1.14 2004/11/21 21:53:35 hoa Exp $
 */

#ifndef MAILTHREAD_H

#define MAILTHREAD_H

#include <libetpan/mailthread_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
  mail_build_thread constructs a tree with the message using the 
  given style.

  @param type is the type of threading to apply, the value can be
    MAIL_THREAD_REFERENCES, MAIL_THREAD_REFERENCES_NO_SUBJECT,
    MAIL_THREAD_ORDEREDSUBJECT or MAIL_THREAD_NONE,

  @param default_from is the default charset to use whenever the
    subject is not tagged with a charset. "US-ASCII" can be used
    if you don't know what to use.

  @param env_list is the message list (with header fields fetched)
    to use to build the message tree.

  @param result * result) will contain the resulting message tree.

  @param if comp_func is NULL, no sorting algorithm is used.

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error
*/

int mail_build_thread(int type, char * default_from,
    struct mailmessage_list * env_list,
    struct mailmessage_tree ** result,
     int (* comp_func)(struct mailmessage_tree **,
         struct mailmessage_tree **));

/*
  mail_thread_sort sort the messages in the message tree, using the
  given sort function.

  @param tree is the message tree to sort.
  
  @param comp_func is the sort function to use (this is the same kind of
    functions than those used for qsort()). mailthread_tree_timecomp can be
    used for default sort.

  @param sort_sub if this value is 0, only the children of the root message
    are sorted.
*/

int mail_thread_sort(struct mailmessage_tree * tree,
    int (* comp_func)(struct mailmessage_tree **,
        struct mailmessage_tree **),
    int sort_sub);

/*
  mailthread_tree_timecomp is the default sort function.

  The message are compared by date, then by message numbers.
  The tree are given in (* ptree1) and (* ptree2).
*/

int mailthread_tree_timecomp(struct mailmessage_tree ** ptree1,
    struct mailmessage_tree ** ptree2);

#ifdef __cplusplus
}
#endif

#endif
