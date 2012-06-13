/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */

#include "zrtp.h"
#include "queue.h"

struct zrtp_queue {
	zrtp_sem_t*			size_sem;
	zrtp_sem_t*			main_sem;
	zrtp_mutex_t*		mutex;    
	mlist_t				head;
	uint32_t			size;
};


zrtp_status_t zrtp_test_queue_create(zrtp_queue_t** queue) {
	
	zrtp_status_t s = zrtp_status_fail;
	zrtp_queue_t* new_queue = (zrtp_queue_t*) zrtp_sys_alloc(sizeof(zrtp_queue_t));
	if (! new_queue) {
		return zrtp_status_fail;
	}
	zrtp_memset(new_queue, 0, sizeof(zrtp_queue_t));
	
	do {
		s = zrtp_sem_init(&new_queue->size_sem, ZRTP_QUEUE_SIZE, ZRTP_QUEUE_SIZE);
		if (zrtp_status_ok != s) {
			break;
		}
		s = zrtp_sem_init(&new_queue->main_sem, 0, ZRTP_QUEUE_SIZE);
		if (zrtp_status_ok != s) {
			break;
		}
		
		s = zrtp_mutex_init(&new_queue->mutex);
		if (zrtp_status_ok != s) {
			break;
		}
		
		init_mlist(&new_queue->head);
		new_queue->size = 0;
		
		s = zrtp_status_ok;
	} while (0);
	
	if (zrtp_status_ok != s) {
		if (new_queue->size_sem) {
			zrtp_sem_destroy(new_queue->size_sem);
		}
		if (new_queue->main_sem) {
			zrtp_sem_destroy(new_queue->main_sem);
		}
		if (new_queue->mutex) {
			zrtp_mutex_destroy(new_queue->mutex);
		}
	}
	
	*queue = new_queue;
				
    return s;	
}

void zrtp_test_queue_destroy(zrtp_queue_t* queue) {
	if (queue->size_sem) {
		zrtp_sem_destroy(queue->size_sem);
	}
	if (queue->main_sem) {
		zrtp_sem_destroy(queue->main_sem);
	}
	if (queue->mutex) {
		zrtp_mutex_destroy(queue->mutex);
	}
}


void zrtp_test_queue_push(zrtp_queue_t* queue, zrtp_queue_elem_t* elem) {
	zrtp_sem_wait(queue->size_sem);
	
	zrtp_mutex_lock(queue->mutex);
	mlist_add_tail(&queue->head, &elem->_mlist);
	queue->size++;
	zrtp_mutex_unlock(queue->mutex);
	
	zrtp_sem_post(queue->main_sem);
}

zrtp_queue_elem_t* zrtp_test_queue_pop(zrtp_queue_t* queue) {
	zrtp_queue_elem_t* res = NULL;
	zrtp_sem_wait(queue->main_sem);
	
	zrtp_mutex_lock(queue->mutex);
	if (queue->size) {
		zrtp_queue_elem_t* elem_cover = mlist_get_struct(zrtp_queue_elem_t, _mlist, queue->head.next);
		res = elem_cover;
		mlist_del(queue->head.next);
	
		queue->size--;
		zrtp_sem_post(queue->size_sem);
	} else {
		zrtp_sem_post(queue->main_sem);
	}
	zrtp_mutex_unlock(queue->mutex);
	
	return res;
}
