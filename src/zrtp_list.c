/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */

#include "zrtp.h"

/*----------------------------------------------------------------------------*/
void init_mlist(mlist_t* head) {
    head->next = head;
    head->prev = head;
}

/*----------------------------------------------------------------------------*/
static void mlist_insert_node(mlist_t* node, mlist_t* prev, mlist_t* next) {
    next->prev	= node;
    node->next	= next;
    node->prev	= prev;
    prev->next	= node;    
}

void mlist_insert(mlist_t *prev, mlist_t *node) {
	mlist_insert_node(node, prev->prev, prev);
}

void mlist_add(mlist_t* head, mlist_t* node) {
    mlist_insert_node(node, head, head->next);
}

void mlist_add_tail(mlist_t *head, mlist_t *node) {
    mlist_insert_node(node, head->prev, head);
}

/*----------------------------------------------------------------------------*/
static void mlist_remove(mlist_t* prev, mlist_t* next) {
    next->prev = prev;
    prev->next = next;
}

void mlist_del(mlist_t *node) {
    mlist_remove(node->prev, node->next);
    node->next = node->prev = 0;
}

void mlist_del_tail(mlist_t *node) {
    mlist_remove(node->prev, node->next);
    node->next = node->prev = 0;
}

/*----------------------------------------------------------------------------*/
mlist_t* mlist_get(mlist_t *head) {
	return (head->next != head) ? head->next : 0;
}

mlist_t* mlist_get_tail(mlist_t *head) {
	return (head->prev != head) ? head->prev : 0;
}

/*----------------------------------------------------------------------------*/
int mlist_isempty(mlist_t *head) {
	return (head->next == head);
}
