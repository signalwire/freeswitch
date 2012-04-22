/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */


#ifndef __ZRTP_LIST_H__
#define __ZRTP_LIST_H__

#include "zrtp_config.h"

typedef struct mlist mlist_t;
struct mlist
{
    mlist_t  *next;
    mlist_t  *prev;
};

#if defined(__cplusplus)
extern "C"
{
#endif

/*
 * \warning
 * We cast pointer to integer. There is bad thing for 64 bit platforms but
 * calculated offset couldn't be bigger then 2x32 and it will be casted
 * to integer correctly.
 */
#define mlist_list_offset(type, list_name) ((size_t)&(((type*)0)->list_name))

#define mlist_get_struct(type, list_name, list_ptr) \
	    ((type*)(((char*)(list_ptr)) - mlist_list_offset(type,list_name)))

#define mlist_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)

#define mlist_for_each_safe(pos, n, head) \
	for (pos = (head)->next, n = pos->next; pos != (head); \
		pos = n, n = pos->next)

void init_mlist(mlist_t* head);

void mlist_add(mlist_t* head, mlist_t* node);
void mlist_add_tail(mlist_t *head, mlist_t *node);

void mlist_insert(mlist_t *prev, mlist_t *node);

void mlist_del(mlist_t *node);
void mlist_del_tail(mlist_t *node);

mlist_t* mlist_get(mlist_t *head);
mlist_t* mlist_get_tail(mlist_t *head);

int mlist_isempty(mlist_t *head);

#if defined(__cplusplus)
}
#endif


#endif /*__ZRTP_LIST_H__ */
