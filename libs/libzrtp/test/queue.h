/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */

#ifndef __ZRTP_TEST_QUEUE_H__
#define __ZRTP_TEST_QUEUE_H__

#include "zrtp.h"

#define ZRTP_QUEUE_SIZE 2000

typedef struct zrtp_queue_elem {
    char		data[1500];
    uint32_t	size;
    mlist_t		_mlist;
} zrtp_queue_elem_t;
typedef struct zrtp_queue zrtp_queue_t;

zrtp_status_t zrtp_test_queue_create(zrtp_queue_t** queue);
void zrtp_test_queue_destroy(zrtp_queue_t* queue);
void zrtp_test_queue_push(zrtp_queue_t* queue, zrtp_queue_elem_t* elem);
zrtp_queue_elem_t* zrtp_test_queue_pop(zrtp_queue_t* queue);

#endif  /* __ZRTP_TEST_QUEUE_H__ */
