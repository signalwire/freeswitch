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
 * $Id: mailthread_types.c,v 1.12 2006/05/22 13:39:40 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailthread_types.h"

#include "mail.h"
#include <stdlib.h>

struct mailmessage_tree *
mailmessage_tree_new(char * node_msgid, time_t node_date,
    mailmessage * node_msg)
{
  struct mailmessage_tree * tree;
  carray * array;

  array = carray_new(16);
  if (array == NULL)
    return NULL;

  tree = malloc(sizeof(* tree));
  tree->node_parent = NULL;
  tree->node_date = node_date;
  tree->node_msgid = node_msgid;
  tree->node_msg = node_msg;
  tree->node_children = array;
  tree->node_base_subject = NULL;
  tree->node_is_reply = FALSE;

  return tree;
}

void mailmessage_tree_free(struct mailmessage_tree * tree)
{
  if (tree->node_base_subject != NULL)
    free(tree->node_base_subject);

  if (tree->node_children != NULL)
    carray_free(tree->node_children);
  if (tree->node_msgid != NULL)
    free(tree->node_msgid);

  free(tree);
}

void mailmessage_tree_free_recursive(struct mailmessage_tree * tree)
{
  unsigned int i;

  for(i = 0 ; i < carray_count(tree->node_children) ; i++) {
    struct mailmessage_tree * child;

    child = carray_get(tree->node_children, i);

    mailmessage_tree_free_recursive(child);
  }

  mailmessage_tree_free(tree);
}
