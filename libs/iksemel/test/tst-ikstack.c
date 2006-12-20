/* iksemel (XML parser for Jabber)
** Copyright (C) 2000-2004 Gurer Ozen <madcat@e-kolay.net>
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU Lesser General Public License.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "iksemel.h"

struct align_test { char a; double b; };
#define DEFAULT_ALIGNMENT  ((size_t) ((char *) &((struct align_test *) 0)->b - (char *) 0))
#define ALIGN_MASK ( DEFAULT_ALIGNMENT - 1 )

const char buf[] = "1234567890abcdefghijklmnopqrstuv";

void
test_stack (int cs)
{
	ikstack *s;
	char *mem, *old;
	int i;

	s = iks_stack_new (cs, cs);
	old = NULL;
	for (i = 0; i < strlen (buf); i++) {
		iks_stack_strdup (s, buf, i);
		mem = iks_stack_alloc (s, i);
		if (((unsigned long) mem) & ALIGN_MASK) {
			printf ("ikstack bug, addr %p should be a multiply of %d\n",
				mem, DEFAULT_ALIGNMENT);
			exit (1);
		}
		memset (mem, 'x', i);
		old = iks_stack_strcat (s, old, 0, buf + i, 1);
	}
	if (strcmp (old, buf) != 0) {
		printf ("ikstack strcat bug:\nExpected: %s\n  Result: %s\n", buf, old);
		exit (1);
	}
	iks_stack_delete (s);
}

int main (int argc, char *argv[])
{
	test_stack (0);
	test_stack (16);
	test_stack (237);
	test_stack (1024);

	return 0;
}
