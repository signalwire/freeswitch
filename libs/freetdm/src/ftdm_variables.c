/*
 * Copyright (c) 2010, Sangoma Technologies
 * David Yat Sin <dyatsin@sangoma.com>
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Contributors: 
 *
 * Moises Silva <moy@sangoma.com>
 *
 */

#include "private/ftdm_core.h"

FT_DECLARE(ftdm_status_t) ftdm_sigmsg_add_var(ftdm_sigmsg_t *sigmsg, const char *var_name, const char *value)
{
	char *t_name = 0, *t_val = 0;

	if (!sigmsg || !var_name || !value) {
		return FTDM_FAIL;
	}
	
	if (!sigmsg->variables) {
		/* initialize on first use */
		sigmsg->variables = create_hashtable(16, ftdm_hash_hashfromstring, ftdm_hash_equalkeys);
		ftdm_assert_return(sigmsg->variables, FTDM_FAIL, "Failed to create hash table\n");
	}
	
	t_name = ftdm_strdup(var_name);
	t_val = ftdm_strdup(value);
	hashtable_insert(sigmsg->variables, t_name, t_val, HASHTABLE_FLAG_FREE_KEY | HASHTABLE_FLAG_FREE_VALUE);
	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_sigmsg_remove_var(ftdm_sigmsg_t *sigmsg, const char *var_name)
{
	if (sigmsg && sigmsg->variables) {
		hashtable_remove(sigmsg->variables, (void *)var_name);
	}
	return FTDM_SUCCESS;
}

FT_DECLARE(const char *) ftdm_sigmsg_get_var(ftdm_sigmsg_t *sigmsg, const char *var_name)
{
	const char *var = NULL;
	
	if (!sigmsg || !sigmsg->variables || !var_name) {
		return NULL;
	}

	var = (const char *)hashtable_search(((struct hashtable*)sigmsg->variables), (void *)var_name);
	return var;
}

FT_DECLARE(ftdm_iterator_t *) ftdm_sigmsg_get_var_iterator(const ftdm_sigmsg_t *sigmsg, ftdm_iterator_t *iter)
{
	ftdm_hash_iterator_t *hashiter = NULL;
	if (!sigmsg) {
		return NULL;
	}
	
	hashiter = sigmsg->variables == NULL ? NULL : hashtable_first(sigmsg->variables);
	
	if (hashiter == NULL) {
		return NULL;
	}
	
	if (!(iter = ftdm_get_iterator(FTDM_ITERATOR_VARS, iter))) {
		return NULL;
	}
	iter->pvt.hashiter = hashiter;
	return iter;
}

FT_DECLARE(ftdm_status_t) ftdm_get_current_var(ftdm_iterator_t *iter, const char **var_name, const char **var_val)
{
	const void *key = NULL;
	void *val = NULL;

	*var_name = NULL;
	*var_val = NULL;

	ftdm_assert_return(iter && (iter->type == FTDM_ITERATOR_VARS) && iter->pvt.hashiter, FTDM_FAIL, "Cannot get variable from invalid iterator!\n");

	hashtable_this(iter->pvt.hashiter, &key, NULL, &val);

	*var_name = key;
	*var_val = val;

	return FTDM_SUCCESS;
}


FT_DECLARE(ftdm_status_t) ftdm_usrmsg_add_var(ftdm_usrmsg_t *usrmsg, const char *var_name, const char *value)
{
	char *t_name = 0, *t_val = 0;

	if (!usrmsg || !var_name || !value) {
		return FTDM_FAIL;
	}
	
	if (!usrmsg->variables) {
		/* initialize on first use */
		usrmsg->variables = create_hashtable(16, ftdm_hash_hashfromstring, ftdm_hash_equalkeys);
		ftdm_assert_return(usrmsg->variables, FTDM_FAIL, "Failed to create hash table\n");
	}
	
	t_name = ftdm_strdup(var_name);
	t_val = ftdm_strdup(value);
	hashtable_insert(usrmsg->variables, t_name, t_val, HASHTABLE_FLAG_FREE_KEY | HASHTABLE_FLAG_FREE_VALUE);
	return FTDM_SUCCESS;
}

FT_DECLARE(const char *) ftdm_usrmsg_get_var(ftdm_usrmsg_t *usrmsg, const char *var_name)
{
	const char *var = NULL;
	
	if (!usrmsg || !usrmsg->variables || !var_name) {
		return NULL;
	}

	var = (const char *)hashtable_search(((struct hashtable*)usrmsg->variables), (void *)var_name);
	return var;
}
