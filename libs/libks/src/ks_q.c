/*
 * Copyright (c) 2007-2014, Anthony Minessale II
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
 */

#include <ks.h>

typedef struct ks_qnode_s {
	void *ptr;
	struct ks_qnode_s *next;
	struct ks_qnode_s *prev;
} ks_qnode_t;

struct ks_q_s {
	ks_pool_t *pool;
	ks_flush_fn_t flush_fn;
	void *flush_data;
	ks_size_t len;
	ks_size_t maxlen;
	ks_cond_t *pop_cond;
	ks_cond_t *push_cond;
	ks_mutex_t *list_mutex;
	uint32_t pushers;
	uint32_t poppers;
	struct ks_qnode_s *head;
	struct ks_qnode_s *tail;
	struct ks_qnode_s *empty;
	uint8_t active;
};

static void ks_q_cleanup(ks_pool_t *mpool, void *ptr, void *arg, int type, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t ctype)
{
	ks_q_t *q = (ks_q_t *) ptr;
	ks_qnode_t *np, *fp;

	if (ctype == KS_MPCL_GLOBAL_FREE) {
		return;
	}

	switch(action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		np = q->head;
		while(np) {
			fp = np;
			np = np->next;
			ks_pool_free(q->pool, fp);
		}
		
		np = q->empty;
		while(np) {
			fp = np;
			np = np->next;
			ks_pool_free(q->pool, fp);
		}
		break;
	case KS_MPCL_DESTROY:
		ks_cond_destroy(&q->pop_cond);
		ks_cond_destroy(&q->push_cond);
		ks_mutex_destroy(&q->list_mutex);
		break;
	}
}

KS_DECLARE(ks_status_t) ks_q_flush(ks_q_t *q)
{
	void *ptr;

	if (!q->active) return KS_STATUS_INACTIVE;
	if (!q->flush_fn) return KS_STATUS_FAIL;
	
	while(ks_q_trypop(q, &ptr) == KS_STATUS_SUCCESS) {
		q->flush_fn(q, ptr, q->flush_data);
	}

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_q_set_flush_fn(ks_q_t *q, ks_flush_fn_t fn, void *flush_data)
{
	if (!q->active) return KS_STATUS_INACTIVE;

	q->flush_fn = fn;
	q->flush_data = flush_data;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_q_wake(ks_q_t *q)
{
	ks_mutex_lock(q->list_mutex);
	ks_cond_broadcast(q->push_cond);
	ks_cond_broadcast(q->pop_cond);
	ks_mutex_unlock(q->list_mutex);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_size_t) ks_q_term(ks_q_t *q)
{
	int active;

	ks_mutex_lock(q->list_mutex);
	active = q->active;
	q->active = 0;
	ks_mutex_unlock(q->list_mutex);

	if (active) {
		ks_q_wake(q);
	}

	return active ? KS_STATUS_SUCCESS : KS_STATUS_INACTIVE;
}

KS_DECLARE(ks_size_t) ks_q_size(ks_q_t *q)
{
	ks_size_t size;

	ks_mutex_lock(q->list_mutex);
	size = q->len;
	ks_mutex_unlock(q->list_mutex);

	return size;
}

KS_DECLARE(ks_status_t) ks_q_destroy(ks_q_t **qP)
{
	ks_q_t *q;
	ks_pool_t *pool;

	ks_assert(qP);
	
	q = *qP;
	*qP = NULL;

	if (q) {
		ks_q_flush(q);
		ks_q_term(q);

		pool = q->pool;
		ks_pool_free(pool, q);
		pool = NULL;

		return KS_STATUS_SUCCESS;
	}

	return KS_STATUS_FAIL;
}

KS_DECLARE(ks_status_t) ks_q_create(ks_q_t **qP, ks_pool_t *pool, ks_size_t maxlen)
{
	ks_q_t *q = NULL;

	q = ks_pool_alloc(pool, sizeof(*q));
	ks_assert(q);

	q->pool = pool;


	ks_mutex_create(&q->list_mutex, KS_MUTEX_FLAG_DEFAULT, pool);
	ks_assert(q->list_mutex);

	ks_cond_create_ex(&q->pop_cond, pool, q->list_mutex);
	ks_assert(q->pop_cond);

	ks_cond_create_ex(&q->push_cond, pool, q->list_mutex);
	ks_assert(q->push_cond);

	q->maxlen = maxlen;
	q->active = 1;

	ks_pool_set_cleanup(pool, q, NULL, 0, ks_q_cleanup);	

	*qP = q;

	return KS_STATUS_SUCCESS;
}

static ks_qnode_t *new_node(ks_q_t *q)
{
	ks_qnode_t *np;

	if (q->empty) {
		np = q->empty;
		q->empty = q->empty->next;
	} else {
		np = ks_pool_alloc(q->pool, sizeof(*np));
	}

	np->prev = np->next = NULL;
	np->ptr = NULL;

	return np;
}

static ks_status_t do_push(ks_q_t *q, void *ptr)
{
	ks_qnode_t *node;

	ks_mutex_lock(q->list_mutex);
	if (!q->active) {
		ks_mutex_unlock(q->list_mutex);
		return KS_STATUS_INACTIVE;
	}

	node = new_node(q);
	node->ptr = ptr;

	if (!q->head) {
		q->head = q->tail = node;
	} else {
		q->tail->next = node;
		node->prev = q->tail;
		q->tail = node;
	}
	q->len++;
	ks_mutex_unlock(q->list_mutex);

	return KS_STATUS_SUCCESS;
}


KS_DECLARE(ks_status_t) ks_q_push(ks_q_t *q, void *ptr)
{
	ks_status_t r;

	ks_mutex_lock(q->list_mutex);
	if (q->active == 0) {
		r = KS_STATUS_INACTIVE;
		goto end;
	}


	if (q->maxlen && q->len == q->maxlen) {
		q->pushers++;
		ks_cond_wait(q->push_cond);
		q->pushers--;

		if (q->maxlen && q->len == q->maxlen) {
			if (!q->active) {
				r = KS_STATUS_INACTIVE;
			} else {
				r = KS_STATUS_BREAK;
			}
			goto end;
		}
	}

	r = do_push(q, ptr);

	if (q->poppers) {
		ks_cond_signal(q->pop_cond);
	}
	
 end:

	ks_mutex_unlock(q->list_mutex);
	return r;
}

KS_DECLARE(ks_status_t) ks_q_trypush(ks_q_t *q, void *ptr)
{
	ks_status_t r;

	ks_mutex_lock(q->list_mutex);
	if (q->active == 0) {
		r = KS_STATUS_INACTIVE;
		goto end;
	}

	if (q->maxlen && q->len == q->maxlen) {
		r = KS_STATUS_BREAK;
		goto end;
	}

	r = do_push(q, ptr);
	
	if (q->poppers) {
		ks_cond_signal(q->pop_cond);
	}
	
 end:
	
	ks_mutex_unlock(q->list_mutex);

	return r;
}

static ks_status_t do_pop(ks_q_t *q, void **ptr)
{
	ks_qnode_t *np;

	ks_mutex_lock(q->list_mutex);

	if (!q->active) {
		ks_mutex_unlock(q->list_mutex);
		return KS_STATUS_INACTIVE;
	}

	if (!q->head) {
		*ptr = NULL;
	} else {
		np = q->head;
		if ((q->head = q->head->next)) {
			q->head->prev = NULL;
		}
		
		*ptr = np->ptr;

		np->next = q->empty;
		np->prev = NULL;
		np->ptr = NULL;
		q->empty = np;
	}

	q->len--;
	ks_mutex_unlock(q->list_mutex);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_q_pop_timeout(ks_q_t *q, void **ptr, uint32_t timeout)
{
	ks_status_t r;

	ks_mutex_lock(q->list_mutex);

	if (!q->active) {
		r = KS_STATUS_INACTIVE;
		goto end;
	}

	if (q->len == 0) {
		if (q->active) {
			q->poppers++;
			if (timeout) {
				r = ks_cond_timedwait(q->pop_cond, timeout);
			} else {
				r = ks_cond_wait(q->pop_cond);
			}
			q->poppers--;

			if (timeout && r != KS_STATUS_SUCCESS) {
				goto end;
			}
		}

		if (q->len == 0) {
			if (!q->active) {
				r = KS_STATUS_INACTIVE;
			} else {
				r = KS_STATUS_BREAK;
			}
			goto end;
		}
	}

	r = do_pop(q, ptr);

	if (q->pushers) {
		ks_cond_signal(q->push_cond);
	}

 end:

	ks_mutex_unlock(q->list_mutex);

	return r;

}

KS_DECLARE(ks_status_t) ks_q_pop(ks_q_t *q, void **ptr)
{
	return ks_q_pop_timeout(q, ptr, 0);
}

KS_DECLARE(ks_status_t) ks_q_trypop(ks_q_t *q, void **ptr)
{
	ks_status_t r;

	ks_mutex_lock(q->list_mutex);

	if (!q->active) {
		r = KS_STATUS_INACTIVE;
		goto end;
	}

	if (q->len == 0) {
		r = KS_STATUS_BREAK;
		goto end;
	}

	r = do_pop(q, ptr);
	
	if (q->pushers) {
		ks_cond_signal(q->push_cond);
	}
	
 end:

	ks_mutex_unlock(q->list_mutex);

	return r;

}




KS_DECLARE(ks_status_t) ks_q_wait(ks_q_t *q)
{
	ks_status_t r = KS_STATUS_SUCCESS;
	int done = 0;

	do {
		ks_mutex_lock(q->list_mutex);

		if (!q->active) {
			r = KS_STATUS_INACTIVE;
			done = 1;
		}

		if (q->len == 0) {
			done = 1;
		}

		ks_mutex_unlock(q->list_mutex);

	} while (!done);

	return r;
}


/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
