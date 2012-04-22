/* iksemel (XML parser for Jabber)
** Copyright (C) 2000-2003 Gurer Ozen <madcat@e-kolay.net>
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU Lesser General Public License.
*/

#include "common.h"
#include "iksemel.h"

static unsigned int
hash_str (const char *str)
{
	const char *p;
	unsigned int h = 0;

	for (p = str; *p != '\0'; p++) {
		h = ( h << 5 ) - h + *p;
	}
	return h;
}

struct item {
	char *name;
	unsigned int count;
	struct item *next;
};

struct hash_s {
	struct item **table;
	unsigned int size;
	unsigned int count;
	ikstack *s;
};

typedef struct hash_s hash;

hash *
hash_new (unsigned int table_size)
{
	hash *h;

	h = malloc (sizeof (struct hash_s));
	if (!h) return NULL;
	h->table = calloc (sizeof (struct item *), table_size);
	if (!h->table) {
		free (h);
		return NULL;
	}
	h->s = iks_stack_new (sizeof (hash) * 128, 8192);
	if (!h->s) {
		free (h->table);
		free (h);
		return NULL;
	}
	h->size = table_size;
	h->count = 0;

	return h;
}

char *
hash_insert (hash *h, const char *name)
{
	struct item *t, *p;
	unsigned int val;

	val = hash_str (name) % h->size;
	h->count++;

	for (t = h->table[val]; t; t = t->next) {
		if (strcmp (t->name, name) == 0)
			break;
	}
	if (NULL == t) {
		t = iks_stack_alloc (h->s, sizeof (struct item));
		if (!t) return NULL;
		t->name = iks_stack_strdup (h->s, name, 0);
		t->count = 0;
		t->next = NULL;
		p = h->table[val];
		if (!p) {
			h->table[val] = t;
		} else {
			while (1) {
				if (p->next == NULL) {
					p->next = t;
					break;
				}
				p = p->next;
			}
		}
	}
	t->count++;

	return t->name;
}

static int
my_cmp (const void *a, const void *b)
{
	unsigned int c1, c2;

	c1 = (*(struct item **)a)->count;
	c2 = (*(struct item **)b)->count;

	if (c1 > c2)
		return -1;
	else if (c1 == c2)
		return 0;
	else
		return 1;
}

void
hash_print (hash *h, char *title_fmt, char *line_fmt)
{
	struct item **tags, *t;
	unsigned int i = 0, pos = 0;

	tags = calloc (sizeof (struct tag *), h->count);

	for (; i < h->size; i ++) {
		for (t = h->table[i]; t; t = t->next) {
			tags[pos++] = t;
		}
	}

	qsort (tags, pos, sizeof (struct item *), my_cmp);

	printf (title_fmt, pos);
	for (i = 0; i < pos; i++) {
		printf (line_fmt, tags[i]->name, tags[i]->count);
	}

	free (tags);
}

void
hash_delete (hash *h)
{
	iks_stack_delete (h->s);
	free (h->table);
	free (h);
}
