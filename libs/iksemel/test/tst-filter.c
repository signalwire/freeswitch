/* iksemel (XML parser for Jabber)
** Copyright (C) 2000-2003 Gurer Ozen <madcat@e-kolay.net>
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU Lesser General Public License.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <assert.h>

#include "iksemel.h"

struct {
	iksfilter *f;
	char *xml;
	int nr;
	iksFilterHook *hook[20];
	int nr_hook;
	iksFilterHook *call[20];
	int nr_call;
} tester;

void
document (char *xml)
{
	tester.nr++;
	tester.xml = xml;
	tester.nr_hook = 0;
}

void
hook (iksFilterHook *hk)
{
	tester.hook[tester.nr_hook++] = hk;
}

void
debug (void)
{
	int i;

	printf ("Filter test %d:\n", tester.nr);
	if (tester.nr_hook) {
		puts ("Expected hook order:");
		for (i = 0; i < tester.nr_hook; i++) {
			printf (" ");
			tester.hook[i] (NULL, NULL);
		}
	} else {
		puts("No hooks expected.");
	}
	if (tester.nr_call) {
		puts ("Hook order:");
		for (i = 0; i < tester.nr_call; i++) {
			printf (" ");
			tester.call[i] (NULL, NULL);
		}
	} else {
		puts("No hooks called.");
	}
	exit (1);
}

void
test (void)
{
	iksparser *prs;
	iks *x;
	int i;

	tester.nr_call = 0;

	prs = iks_dom_new (&x);
	iks_parse (prs, tester.xml, strlen (tester.xml), 1);
	iks_parser_delete (prs);
	iks_filter_packet (tester.f, iks_packet (x));
	iks_delete (x);

	if (tester.nr_call != tester.nr_hook) debug ();
	for (i = 0; i < tester.nr_hook; i++) {
		if (tester.call[i] != tester.hook[i]) debug ();
	}
}

#define DEBUG(x) if (NULL == pak) { puts ( (x) ); return IKS_FILTER_PASS; }

int
on_msg (void *user_data, ikspak *pak)
{
	DEBUG ("on_msg");
	assert (IKS_PAK_MESSAGE == pak->type);
	tester.call[tester.nr_call++] = on_msg;
	return IKS_FILTER_PASS;
}

int
on_iq (void *user_data, ikspak *pak)
{
	DEBUG ("on_iq");
	assert (IKS_PAK_IQ == pak->type);
	tester.call[tester.nr_call++] = on_iq;
	return IKS_FILTER_PASS;
}

int
on_iq_result (void *user_data, ikspak *pak)
{
	DEBUG ("on_iq_result");
	assert (IKS_PAK_IQ == pak->type);
	assert (IKS_TYPE_RESULT == pak->subtype);
	tester.call[tester.nr_call++] = on_iq_result;
	return IKS_FILTER_PASS;
}

int
on_iq_result_id_auth (void *user_data, ikspak *pak)
{
	DEBUG ("on_iq_result_id_auth");
	assert (IKS_PAK_IQ == pak->type);
	assert (IKS_TYPE_RESULT == pak->subtype);
	assert (iks_strcmp (pak->id, "auth") == 0);
	tester.call[tester.nr_call++] = on_iq_result_id_auth;
	return IKS_FILTER_PASS;
}

int
on_id_auth (void *user_data, ikspak *pak)
{
	DEBUG ("on_id_auth");
	assert (iks_strcmp (pak->id, "auth") == 0);
	tester.call[tester.nr_call++] = on_id_auth;
	return IKS_FILTER_PASS;
}

int
on_from_patrician (void *user_data, ikspak *pak)
{
	DEBUG ("on_from_patrician");
	assert (iks_strcmp (pak->from->partial, "patrician@morpork.gov") == 0);
	tester.call[tester.nr_call++] = on_from_patrician;
	return IKS_FILTER_PASS;
}

int
on_msg_chat_from_patrician (void *user_data, ikspak *pak)
{
	DEBUG ("on_msg_chat_from_patrician");
	assert (pak->type == IKS_PAK_MESSAGE);
	assert (pak->subtype == IKS_TYPE_CHAT);
	assert (iks_strcmp (pak->from->partial, "patrician@morpork.gov") == 0);
	tester.call[tester.nr_call++] = on_msg_chat_from_patrician;
	return IKS_FILTER_PASS;
}

int
on_id_albatros (void *user_data, ikspak *pak)
{
	DEBUG ("on_id_albatros");
	assert (iks_strcmp (pak->id, "albatros") == 0);
	tester.call[tester.nr_call++] = on_id_albatros;
	return IKS_FILTER_PASS;
}

int
on_from_dean (void *user_data, ikspak *pak)
{
	DEBUG ("on_from_dean");
	assert (iks_strcmp (pak->from->partial, "dean@unseen.edu") == 0);
	tester.call[tester.nr_call++] = on_from_dean;
	return IKS_FILTER_PASS;
}

int
main (int argc, char *argv[])
{
	tester.f = iks_filter_new ();
	iks_filter_add_rule (tester.f, on_msg, 0,
		IKS_RULE_TYPE, IKS_PAK_MESSAGE,
		IKS_RULE_DONE);
	iks_filter_add_rule (tester.f, on_iq, 0,
		IKS_RULE_TYPE, IKS_PAK_IQ,
		IKS_RULE_DONE);
	iks_filter_add_rule (tester.f, on_iq_result, 0,
		IKS_RULE_TYPE, IKS_PAK_IQ,
		IKS_RULE_SUBTYPE, IKS_TYPE_RESULT,
		IKS_RULE_DONE);
	iks_filter_add_rule (tester.f, on_iq_result_id_auth, 0,
		IKS_RULE_TYPE, IKS_PAK_IQ,
		IKS_RULE_SUBTYPE, IKS_TYPE_RESULT,
		IKS_RULE_ID, "auth",
		IKS_RULE_DONE);
	iks_filter_add_rule (tester.f, on_id_auth, 0,
		IKS_RULE_ID, "auth",
		IKS_RULE_DONE);
	iks_filter_add_rule (tester.f, on_from_dean, 0,
		IKS_RULE_FROM_PARTIAL, "dean@unseen.edu",
		IKS_RULE_DONE);
	iks_filter_add_rule (tester.f, on_from_patrician, 0,
		IKS_RULE_FROM_PARTIAL, "patrician@morpork.gov",
		IKS_RULE_DONE);
	iks_filter_add_rule (tester.f, on_msg_chat_from_patrician, 0,
		IKS_RULE_TYPE, IKS_PAK_MESSAGE,
		IKS_RULE_SUBTYPE, IKS_TYPE_CHAT,
		IKS_RULE_FROM_PARTIAL, "patrician@morpork.gov",
		IKS_RULE_DONE);
	iks_filter_add_rule (tester.f, on_id_albatros, 0,
		IKS_RULE_ID, "albatros",
		IKS_RULE_DONE);

	document ("<message from='dean@unseen.edu' id='1234'><body>Born to Rune.</body></message>");
	hook (on_from_dean);
	hook (on_msg);
	test ();

	document ("<presence from='librarian@unseen.edu' show='away'/>");
	test ();

	document ("<message from='rincewind@unseen.edu' type='chat' id='albatros'><body>yaaargh</body></message>");
	hook (on_id_albatros);
	hook (on_msg);
	test ();

	document ("<iq type='get' from='rincewind@unseen.edu'><query xmlns='jabber:time'/></iq>");
	hook (on_iq);
	test ();

	document ("<message from='patrician@morpork.gov'><body>so you admit it?</body></message>");
	hook (on_from_patrician);
	hook (on_msg);
	test ();

	document ("<iq type='result' from='rincewind@unseen.edu'><query xmlns='jabber:version'><name>cabbar</name><version>1.0</version></query></iq>");
	hook (on_iq_result);
	hook (on_iq);
	test ();

	document ("<presence from='dean@unseen.edu/psi' type='unavailable'/>");
	hook (on_from_dean);
	test ();

	document ("<message from='patrician@morpork.gov' type='chat' id='albatros'><body>hmm</body></message>");
	hook (on_id_albatros);
	hook (on_msg_chat_from_patrician);
	hook (on_from_patrician);
	hook (on_msg);
	test ();

	document ("<iq type='result' id='auth'/>");
	hook (on_iq_result_id_auth);
	hook (on_id_auth);
	hook (on_iq_result);
	hook (on_iq);
	test ();

	iks_filter_delete (tester.f);

	return 0;
}
