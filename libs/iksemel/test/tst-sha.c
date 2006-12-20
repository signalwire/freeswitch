/* iksemel (XML parser for Jabber)
** Copyright (C) 2000-2003 Gurer Ozen <madcat@e-kolay.net>
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU Lesser General Public License.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "iksemel.h"

int main (int argc, char *argv[])
{
	struct lala {
		char *str;
		char *hash;
	} known_hashes[] = {
		{ "abc", "a9993e364706816aba3e25717850c26c9cd0d89d" },
		{ "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
		  "84983e441c3bd26ebaae4aa1f95129e5e54670f1" },
		{ NULL, NULL }
	};
	int i = 0;
	char buf[42];

	while (known_hashes[i].str) {
		iks_sha (known_hashes[i].str, buf);
		if (strcmp (buf, known_hashes[i].hash) != 0) {
			printf("SHA1 hash of \"%s\"\n", known_hashes[i].str);
			printf(" Result:   %s\n", buf);
			printf(" Expected: %s\n", known_hashes[i].hash);
			return 1;
		}
		i++;
	}
	return 0;
}
