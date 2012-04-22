/* iksemel (XML parser for Jabber)
** Copyright (C) 2000-2003 Gurer Ozen <madcat@e-kolay.net>
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU Lesser General Public License.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "iksemel.h"

iks *my_x;
int nr_tests;

#define PR_TEST printf ("DOM test %d:\n", nr_tests)

void
document (char *xml)
{
	enum ikserror err;
	iksparser *p;

	nr_tests++;
	if (my_x) iks_delete (my_x);
	p = iks_dom_new (&my_x);
	err = iks_parse (p, xml, 0, 1);
	switch (err) {
		case IKS_OK:
			break;
		case IKS_NOMEM:
			PR_TEST;
			puts ("Not enough memory.");
			exit (1);
		case IKS_BADXML:
			PR_TEST;
			printf ("Invalid xml at byte %ld in\n[%s]\n", iks_nr_bytes (p), xml);
			exit (1);
		case IKS_HOOK:
			PR_TEST;
			puts ("Hook.");
	}
	iks_parser_delete (p);
}

void
tag (char *name, ...)
{
	iks *x;
	va_list ap;

	x = my_x;
	va_start (ap, name);
	while (1) {
		char *name = iks_name (x);
		char *tmp = va_arg (ap, char*);
		if (NULL == tmp) break;
		x = iks_find (x, tmp);
		if (!x) {
			PR_TEST;
			printf ("Tag <%s> is not a child of tag <%s>\n", tmp, name);
			exit (1);
		}
	}
	if (!x || NULL == iks_find (x, name)) {
		PR_TEST;
		printf ("Tag <%s> is not a child of tag <%s>\n", name, iks_name (x));
		exit (1);
	}
	va_end (ap);
}

void
cdata (char *data, ...)
{
	iks *x;
	va_list ap;

	x = my_x;
	va_start (ap, data);
	while (1) {
		char *name = iks_name (x);
		char *tmp = va_arg (ap, char*);
		if (NULL == tmp) break;
		x = iks_find (x, tmp);
		if (!x) {
			PR_TEST;
			printf ("Tag <%s> is not a child of tag <%s>\n", tmp, name);
			exit (1);
		}
	}
	if (iks_strcmp ( iks_cdata (iks_child (x)), data) != 0) {
		PR_TEST;
		printf ("CDATA [%s] not found.\n", data);
		exit (1);
	}
	va_end (ap);
}

void
attrib (char *att, char *val, ...)
{
	iks *x;
	va_list ap;

	x = my_x;
	va_start (ap, val);
	while (1) {
		char *name = iks_name (x);
		char *tmp = va_arg (ap, char*);
		if (NULL == tmp) break;
		x = iks_find (x, tmp);
		if (!x) {
			PR_TEST;
			printf ("Tag <%s> is not a child of tag <%s>\n", tmp, name);
			exit (1);
		}
	}
	if (iks_strcmp (val, iks_find_attrib (x, att)) != 0) {
		PR_TEST;
		printf ("Attribute '%s' not found.\n", att);
		exit (1);
	}
	va_end (ap);
}

void
string (char *xml)
{
	char *tmp;

	tmp = iks_string (iks_stack (my_x), my_x);
	if (iks_strcmp (tmp, xml) != 0) {
		PR_TEST;
		printf ("Result:   %s\n", tmp);
		printf ("Expected: %s\n", xml);
		exit (1);
	}
}

static char buf[] =
	"<presence id='JCOM_11' to='lala@j.org' type='available'><status>"
	"&quot; &lt;online&amp;dangerous&gt; &quot;</status>meow<a><b c='d'/>"
	"</a><test/></presence>";

int main (int argc, char *argv[])
{
	document ("<atag></atag>");
	string ("<atag/>");

	document ("<test>lala<b>bold</b>blablabla<a><c/></a></test>");
	tag ("b", 0);
	tag ("c", "a", 0);
	string ("<test>lala<b>bold</b>blablabla<a><c/></a></test>");

	document (buf);
	cdata ("\" <online&dangerous> \"", "status", 0);
	attrib ("c", "d", "a", "b", 0);
	tag ("test", 0);
	string (buf);

	return 0;
}
