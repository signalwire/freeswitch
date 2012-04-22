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

int main (int argc, char *argv[])
{
	static char xml[] =
		"<iq type='result' to='ydobon@jabber.org'><query xmlns='jabber:iq:version'>"
		"<name>TestClient</name><os>SuxOS 2000</os><version><stable solidity='rock'/>"
		"1.2.0 patchlevel 2</version></query></iq>";
	static char xml2[] =
		"<Ni><C/>lala<br/><A/>Hello World<B/></Ni>";
	iks *x, *y, *z;
	char *t;

	setlocale (LC_ALL, "");

	x = iks_new ("iq");
	iks_insert_attrib (x, "type", "resultypo");
	iks_insert_attrib (x, "type", "result");
	iks_insert_attrib (x, "to", "ydobon@jabber.org");
	y = iks_new_within ("query", iks_stack (x));
	iks_insert_cdata (iks_insert (y, "name"), "TestClient", 10);
	iks_insert_cdata (iks_insert (y, "os"), "SuxOS", 0);
	z = iks_insert (y, "version");
	iks_insert (z, "stable");
	iks_insert_cdata (z, "1.2", 3);
	iks_insert_cdata (z, ".0 patchlevel 2", 0);
	iks_insert_node (x, y);
	z = iks_find (y, "os");
	iks_insert_attrib (z, "error", "yes");
	iks_insert_attrib (z, "error", NULL);
	iks_insert_cdata (z, " 2000", 5);
	z = iks_next (z);
	z = iks_find (z, "stable");
	iks_insert_attrib (z, "solidity", "rock");
	z = iks_parent (iks_parent (z));
	iks_insert_attrib (z, "xmlns", "jabber:iq:version");

	t = iks_string (iks_stack (x), x);
	if(!t || strcmp(t, xml) != 0) {
		printf("Result:   %s\n", t);
		printf("Expected: %s\n", xml);
		return 1;
	}
	iks_delete(x);


	x = iks_new ("Ni");
	y = iks_insert (x, "br");
	z = iks_prepend_cdata (y, "lala", 4);
	iks_prepend (z, "C");
	z = iks_insert_cdata (x, "Hello", 5);
	y = iks_append (z, "B");
	iks_prepend (z, "A");
	iks_append_cdata (z, " ", 1);
	iks_prepend_cdata (y, "World", 5);

	t = iks_string (iks_stack (x), x);
	if(!t || strcmp(t, xml2) != 0) {
		printf("Result:   %s\n", t);
		printf("Expected: %s\n", xml2);
		return 1;
	}
	iks_delete(x);

	return 0;
}
