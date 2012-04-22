/* iksemel (XML parser for Jabber)
** Copyright (C) 2000-2003 Gurer Ozen <madcat@e-kolay.net>
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU Lesser General Public License.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

#include "iksemel.h"

ikstack *my_stack;

void
print_id (iksid *id)
{
	printf (" Full: [%s]\n Partial: [%s]\n User: [%s]\n Server: [%s]\n Resource: [%s]\n",
		id->full, id->partial, id->user, id->server, id->resource);
}

#define BUG(x) { print_id ( (x) ); exit (1); }

void
test_id (char *id, char *partial, char *user, char *server, char *resource)
{
	iksid *a;

	a = iks_id_new (my_stack, id);
	if ((a->partial || partial) && iks_strcmp (a->partial, partial) != 0) BUG(a);
	if ((a->user || user) && iks_strcmp (a->user, user) != 0) BUG(a);
	if ((a->server || server) && iks_strcmp (a->server, server) != 0) BUG(a);
	if ((a->resource || resource) && iks_strcmp (a->resource, resource) != 0) BUG(a);
}

void
test_cmp (char *stra, char *strb, int parts, int diff)
{
	iksid *a, *b;

	a = iks_id_new (my_stack, stra);
	b = iks_id_new (my_stack, strb);
	if (diff != iks_id_cmp (a, b, parts)) exit (1);
}

int main (int argc, char *argv[])
{
	my_stack = iks_stack_new (1024, 1024);

	test_id ("jabber:madcat@jabber.org/cabbar", "madcat@jabber.org", "madcat", "jabber.org", "cabbar");
	test_id ("bob@silent.org", "bob@silent.org", "bob", "silent.org", NULL);

	test_cmp ("dante@jabber.org/hell", "dante@jabber.org/heaven", IKS_ID_PARTIAL, 0);
	test_cmp ("madcat@jabber.org/cabbar", "madcat@jabber.org/jabberx", IKS_ID_FULL, IKS_ID_RESOURCE);
	test_cmp ("dean@unseen.edu/pda", "librarian@unseen.edu/jabberx", IKS_ID_FULL, IKS_ID_USER | IKS_ID_RESOURCE);
	test_cmp ("patrician@morpork.gov/gabber", "cohen@guild.org/gsm", IKS_ID_FULL, IKS_ID_FULL);
	test_cmp ("peter@family.com", "peter@family.com/clam", IKS_ID_PARTIAL, 0);

	iks_stack_delete (&my_stack);

	return 0;
}
