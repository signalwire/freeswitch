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

struct element_s {
	struct element_s *next;
	enum ikstype type;

	enum ikstagtype tag;
	char *name;
	int nr_atts;
	char *atts[10];
	char *vals[10];

	char *cdata;
	int len;
};

struct {
	char *doc;
	int len;
	struct element_s *elements;
	struct element_s *last_element;
	struct element_s *cur;
	int nr_tests;
	int nr_cur;
	int blocksize;
} tester;

void
document (char *xml)
{
	if (tester.elements) {
		struct element_s *tmp;
		for (; tester.elements; tester.elements = tmp) {
			tmp = tester.elements->next;
			free (tester.elements);
		}
	}
	tester.doc = xml;
	tester.len = strlen (xml);
	tester.elements = NULL;
	tester.last_element = NULL;
	tester.nr_tests++;
}

void
element (enum ikstype type, ...)
{
	struct element_s *el;
	va_list ap;
	char *tmp;

	el = malloc (sizeof (struct element_s));
	memset (el, 0, sizeof (struct element_s));
	el->type = type;

	va_start (ap, type);
	switch (type) {
		case IKS_TAG:
			el->tag = va_arg (ap, int);
			el->name = va_arg (ap, char*);
			if (IKS_CLOSE == el->tag) break;
			while (1) {
				tmp = va_arg (ap, char*);
				if (tmp) {
					el->atts[el->nr_atts] = tmp;
					el->vals[el->nr_atts] = va_arg (ap, char*);
					el->nr_atts++;
				} else {
					break;
				}
			}
			break;
		case IKS_CDATA:
			tmp = va_arg (ap, char*);
			el->cdata = tmp;
			el->len = strlen (tmp);
			break;
		case IKS_NONE:
		case IKS_ATTRIBUTE:
			puts ("invalid element() call");
			exit (1);
	}
	va_end (ap);

	if (NULL == tester.elements) tester.elements = el;
	if (tester.last_element) tester.last_element->next = el;
	tester.last_element = el;
}

#define PRINT_TEST 	printf ("Sax test %d, blocksize %d, element %d:\n", tester.nr_tests, tester.blocksize, tester.nr_cur)
#define NEXT_ELEM { tester.cur = tester.cur->next; tester.nr_cur++; }

void
debug_tag (enum ikstagtype type, char *name, char **atts)
{
	int i;

	PRINT_TEST;
	if (tester.cur && tester.cur->type == IKS_TAG) {
		switch (tester.cur->tag) {
			case IKS_OPEN:
				printf ("  Expecting tag <%s>\n", tester.cur->name);
				break;
			case IKS_CLOSE:
				printf ("  Expecting tag </%s>\n", tester.cur->name);
				break;
			case IKS_SINGLE:
				printf ("  Expecting tag <%s/>\n", tester.cur->name);
				break;
		}
		for (i = 0; i < tester.cur->nr_atts; i++) {
			printf ("    %s='%s'\n", tester.cur->atts[i], tester.cur->vals[i]);
		}
	} else {
		printf ("  Not expecting a tag here.\n");
	}
	switch (type) {
		case IKS_OPEN:
			printf ("  Got tag <%s>\n", name);
			break;
		case IKS_CLOSE:
			printf ("  Got tag </%s>\n", name);
			break;
		case IKS_SINGLE:
			printf ("  Got tag <%s/>\n", name);
			break;
	}
	i = 0;
	while (atts && atts[i]) {
		printf ("    %s='%s'\n", atts[i], atts[i+1]);
		i += 2;
	}
}

#define TAG_FAIL { debug_tag (type,name,atts); exit (1); }

int
tagHook (void *udata, char *name, char **atts, int type)
{
	int nr, i, flag;

	if (!tester.cur) TAG_FAIL;
	if (tester.cur->type != IKS_TAG) TAG_FAIL;
	if (tester.cur->tag != type) TAG_FAIL;
	if (iks_strcmp (tester.cur->name, name) != 0) TAG_FAIL;
	if (!atts && tester.cur->nr_atts > 0) TAG_FAIL;
	if (atts && tester.cur->nr_atts == 0) TAG_FAIL;

	nr = tester.cur->nr_atts;
	while (nr) {
		flag = 0;
		for (i = 0;atts[i]; i+= 2) {
			if (iks_strcmp (atts[i], tester.cur->atts[nr-1]) == 0 && iks_strcmp (atts[i+1], tester.cur->vals[nr-1]) == 0) {
				flag = 1;
				break;
			}
		}
		if (flag == 0) TAG_FAIL;
		nr--;
	}

	NEXT_ELEM;
	return IKS_OK;
}

void
debug_cdata (char *data, size_t len, int pos)
{
	int i;

	PRINT_TEST;
	if (tester.cur && tester.cur->type == IKS_CDATA)
		printf ("  Expecting cdata [%s]\n", tester.cur->cdata);
	else
		printf ("  Not expecting cdata here\n");
	printf ("  Got cdata [");
	for (i = 0; i < len; i++) putchar (data[i]);
	printf ("] at the pos %d.\n", pos);
}

#define CDATA_FAIL { debug_cdata (data, len, pos); exit (1); }

int
cdataHook (void *udata, char *data, size_t len)
{
	static int pos = 0;

	if (!tester.cur) CDATA_FAIL;
	if (tester.cur->type != IKS_CDATA) CDATA_FAIL;
	if (iks_strncmp (tester.cur->cdata + pos, data, len) != 0) CDATA_FAIL;
	pos += len;
	if (pos > tester.cur->len) CDATA_FAIL;
	if (pos == tester.cur->len) {
		pos = 0;
		NEXT_ELEM;
	}
	return IKS_OK;
}

void
test_size (int blocksize)
{
	enum ikserror err;
	iksparser *prs;
	int i, len;

	tester.cur = tester.elements;
	tester.nr_cur = 1;
	tester.blocksize = blocksize;
	len = tester.len;

	prs = iks_sax_new (NULL, tagHook, cdataHook);
	i = 0;
	if (0 == blocksize) blocksize = len;
	while (i < len) {
		if (i + blocksize > len) blocksize = len - i;
		err = iks_parse (prs, tester.doc + i, blocksize, 0);
		switch (err) {
			case IKS_OK:
				break;
			case IKS_NOMEM:
				exit (1);
			case IKS_BADXML:
				PRINT_TEST;
				printf ("Invalid xml at byte %ld in\n[%s]\n", iks_nr_bytes (prs), tester.doc);
				exit (1);
			case IKS_HOOK:
				exit (1);
		}
		i += blocksize;
	}
	if (tester.cur) exit (1);
	iks_parser_delete (prs);
}

void
test (void)
{
	int i;

	for (i = 0; i < tester.len; i++) {
		test_size (i);
	}
}

void
test_bad (int badbyte)
{
	iksparser *p;
	enum ikserror err;

	p = iks_sax_new (NULL, NULL, NULL);
	err = iks_parse (p, tester.doc, tester.len, 1);
	switch (err) {
		case IKS_OK:
			break;
		case IKS_NOMEM:
			exit (1);
		case IKS_BADXML:
			if (iks_nr_bytes (p) == badbyte) return;
			break;
		case IKS_HOOK:
			exit (1);
	}
	printf ("Sax test %d:\n", tester.nr_tests);
	printf ("Expected bad byte %d, got %ld in\n[%s]\n", badbyte, iks_nr_bytes (p), tester.doc);
	exit (1);
}

int
main (int argc, char *argv[])
{
	document ("<lonely/>");
	element (IKS_TAG, IKS_SINGLE, "lonely", 0);
	test ();

	document ("<?xml version='1.0'?><parent><child/><child/>child</parent>");
	element (IKS_TAG, IKS_OPEN, "parent", 0);
	element (IKS_TAG, IKS_SINGLE, "child", 0);
	element (IKS_TAG, IKS_SINGLE, "child", 0);
	element (IKS_CDATA, "child");
	element (IKS_TAG, IKS_CLOSE, "parent");
	test ();

	document ("<mytag abc='123' id=\"XC72\"></mytag>");
	element (IKS_TAG, IKS_OPEN, "mytag", "abc", "123", "id", "XC72", 0);
	element (IKS_TAG, IKS_CLOSE, "mytag");
	test ();

	document ("<body>I&apos;m fixing parser&amp;tester for &quot;&lt;&quot; and &quot;&gt;&quot; chars.</body>");
	element (IKS_TAG, IKS_OPEN, "body", 0);
	element (IKS_CDATA, "I'm fixing parser&tester for \"<\" and \">\" chars.");
	element (IKS_TAG, IKS_CLOSE, "body");
	test ();

	document ("<tag a='1' b='2' c='3' d='4' e='5' f='6' g='7' id='xyz9'><sub></sub></tag>");
	element (IKS_TAG, IKS_OPEN, "tag", "a", "1", "b", "2", "c", "3", "d", "4", "e", "5", "f", "6", "g", "7", "id", "xyz9", 0);
	element (IKS_TAG, IKS_OPEN, "sub", 0);
	element (IKS_TAG, IKS_CLOSE, "sub");
	element (IKS_TAG, IKS_CLOSE, "tag");
	test ();

	document ("<item url='http://jabber.org'><!-- little comment -->Jabber Site</item>");
	element (IKS_TAG, IKS_OPEN, "item", "url", "http://jabber.org", 0);
	element (IKS_CDATA, "Jabber Site");
	element (IKS_TAG, IKS_CLOSE, "item");
	test ();

	document ("<index><!-- <item> - tag has no childs --><item name='lala' page='42'/></index>");
	element (IKS_TAG, IKS_OPEN, "index", 0);
	element (IKS_TAG, IKS_SINGLE, "item", "name", "lala", "page", "42", 0);
	element (IKS_TAG, IKS_CLOSE, "index");
	test ();

	document ("<ka>1234<![CDATA[ <ka> lala ] ]] ]]] ]]>4321</ka>");
	element (IKS_TAG, IKS_OPEN, "ka", 0);
	element (IKS_CDATA, "1234 <ka> lala ] ]] ]]] 4321");
	element (IKS_TAG, IKS_CLOSE, "ka");
	test ();

	document ("<test><standalone be='happy'/>abcd<br/>&lt;escape&gt;</test>");
	element (IKS_TAG, IKS_OPEN, "test", 0);
	element (IKS_TAG, IKS_SINGLE, "standalone", "be", "happy", 0);
	element (IKS_CDATA, "abcd");
	element (IKS_TAG, IKS_SINGLE, "br", 0);
	element (IKS_CDATA, "<escape>");
	element (IKS_TAG, IKS_CLOSE, "test");
	test ();

	document ("<a><b>john&amp;mary<c><d e='f' g='123456' h='madcat' klm='nop'/></c></b></a>");
	element (IKS_TAG, IKS_OPEN, "a", 0);
	element (IKS_TAG, IKS_OPEN, "b", 0);
	element (IKS_CDATA, "john&mary");
	element (IKS_TAG, IKS_OPEN, "c", 0);
	element (IKS_TAG, IKS_SINGLE, "d", "e", "f", "g", "123456", "h", "madcat", "klm", "nop", 0);
	element (IKS_TAG, IKS_CLOSE, "c", 0);
	element (IKS_TAG, IKS_CLOSE, "b", 0);
	element (IKS_TAG, IKS_CLOSE, "a", 0);
	test ();

	document ("<test>\xFF</test>");
	test_bad (6);

	document ("<t\0></t>");
	tester.len = 8;
	test_bad (2);

	document ("<a><b/><c></c/></a>");
	test_bad (13);

	document ("<e><!-- -- --></e>");
	test_bad (10);

	document ("<g><test a='123'/ b='lala'></g>");
	test_bad (17);

	document ("<ha><!-- <lala> --><!- comment -></ha>");
	test_bad (22);

	document ("<lol>&lt;<&gt;</lol>");
	test_bad (16);

	document ("<a>\xC0\x80</a>");
	test_bad (3);

	document ("<\x8F\x85></\x8F\x85>");
	test_bad (1);

	document ("<utf8>\xC1\x80<br/>\xED\x95\x9C\xEA\xB5\xAD\xEC\x96\xB4<err>\xC1\x65</err></utf8>");
	test_bad (28);

	return 0;
}
