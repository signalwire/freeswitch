/* iksemel (XML parser for Jabber)
** Copyright (C) 2000-2004 Gurer Ozen <madcat@e-kolay.net>
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU Lesser General Public License.
*/

#include "common.h"
#include "iksemel.h"

#ifdef HAVE_GETOPT_LONG
#include <getopt.h>
#endif

#ifdef _WIN32
#include <winsock.h>
#endif

#ifdef HAVE_GETOPT_LONG
static struct option longopts[] = {
	{ "backup", required_argument, 0, 'b' },
	{ "restore", required_argument, 0, 'r' },
	{ "file", required_argument, 0, 'f' },
	{ "timeout", required_argument, 0, 't' },
	{ "secure", 0, 0, 's' },
	{ "sasl", 0, 0, 'a' },
	{ "log", 0, 0, 'l' },
	{ "help", 0, 0, 'h' },
	{ "version", 0, 0, 'V' },
	{ 0, 0, 0, 0 }
};
#endif

static char *shortopts = "b:r:f:t:salhV";

static void
print_usage (void)
{
	puts ("Usage: iksroster [OPTIONS]\n"
		"This is a backup tool for your jabber roster.\n"
		" -b, --backup=JID    Download roster from the server.\n"
		" -r, --restore=JID   Upload roster to the server.\n"
		" -f, --file=FILE     Load/Save roster to this file.\n"
		" -t, --timeout=SECS  Set network timeout.\n"
		" -s, --secure        Use encrypted connection.\n"
		" -a, --sasl          Use SASL authentication.\n"
		" -l, --log           Print exchanged xml data.\n"
		" -h, --help          Print this text and exit.\n"
		" -V, --version       Print version and exit.\n"
#ifndef HAVE_GETOPT_LONG
		"(long options are not supported on your system)\n"
#endif
#ifndef HAVE_GNUTLS
		"(secure connections are not supported on your system)\n"
#endif
		"Report bugs to <iksemel-dev@jabberstudio.org>.");
}

/* stuff we keep per session */
struct session {
	iksparser *prs;
	iksid *acc;
	char *pass;
	int features;
	int authorized;
	int counter;
	int set_roster;
	int job_done;
};

/* precious roster we'll deal with */
iks *my_roster;

/* out packet filter */
iksfilter *my_filter;

/* connection time outs if nothing comes for this much seconds */
int opt_timeout = 30;

/* connection flags */
int opt_use_tls;
int opt_use_sasl;
int opt_log;

void
j_error (char *msg)
{
	fprintf (stderr, "iksroster: %s\n", msg);
	exit (2);
}

int
on_result (struct session *sess, ikspak *pak)
{
	iks *x;

	if (sess->set_roster == 0) {
		x = iks_make_iq (IKS_TYPE_GET, IKS_NS_ROSTER);
		iks_insert_attrib (x, "id", "roster");
		iks_send (sess->prs, x);
		iks_delete (x);
	} else {
		iks_insert_attrib (my_roster, "type", "set");
		iks_send (sess->prs, my_roster);
	}
	return IKS_FILTER_EAT;
}

int
on_stream (struct session *sess, int type, iks *node)
{
	sess->counter = opt_timeout;

	switch (type) {
		case IKS_NODE_START:
			if (opt_use_tls && !iks_is_secure (sess->prs)) {
				iks_start_tls (sess->prs);
				break;
			}
			if (!opt_use_sasl) {
				iks *x;

				x = iks_make_auth (sess->acc, sess->pass, iks_find_attrib (node, "id"));
				iks_insert_attrib (x, "id", "auth");
				iks_send (sess->prs, x);
				iks_delete (x);
			}
			break;

		case IKS_NODE_NORMAL:
			if (strcmp ("stream:features", iks_name (node)) == 0) {
				sess->features = iks_stream_features (node);
				if (opt_use_sasl) {
					if (opt_use_tls && !iks_is_secure (sess->prs)) break;
					if (sess->authorized) {
						iks *t;
						if (sess->features & IKS_STREAM_BIND) {
							t = iks_make_resource_bind (sess->acc);
							iks_send (sess->prs, t);
							iks_delete (t);
						}
						if (sess->features & IKS_STREAM_SESSION) {
							t = iks_make_session ();
							iks_insert_attrib (t, "id", "auth");
							iks_send (sess->prs, t);
							iks_delete (t);
						}
					} else {
						if (sess->features & IKS_STREAM_SASL_MD5)
							iks_start_sasl (sess->prs, IKS_SASL_DIGEST_MD5, sess->acc->user, sess->pass);
						else if (sess->features & IKS_STREAM_SASL_PLAIN)
							iks_start_sasl (sess->prs, IKS_SASL_PLAIN, sess->acc->user, sess->pass);
					}
				}
			} else if (strcmp ("failure", iks_name (node)) == 0) {
				j_error ("sasl authentication failed");
			} else if (strcmp ("success", iks_name (node)) == 0) {
				sess->authorized = 1;
				iks_send_header (sess->prs, sess->acc->server);
			} else {
				ikspak *pak;

				pak = iks_packet (node);
				iks_filter_packet (my_filter, pak);
				if (sess->job_done == 1) return IKS_HOOK;
			}
			break;

		case IKS_NODE_STOP:
			j_error ("server disconnected");

		case IKS_NODE_ERROR:
			j_error ("stream error");
	}

	if (node) iks_delete (node);
	return IKS_OK;
}

int
on_error (void *user_data, ikspak *pak)
{
	j_error ("authorization failed");
	return IKS_FILTER_EAT;
}

int
on_roster (struct session *sess, ikspak *pak)
{
	my_roster = pak->x;
	sess->job_done = 1;
	return IKS_FILTER_EAT;
}

void
on_log (struct session *sess, const char *data, size_t size, int is_incoming)
{
	if (iks_is_secure (sess->prs)) fprintf (stderr, "Sec");
	if (is_incoming) fprintf (stderr, "RECV"); else fprintf (stderr, "SEND");
	fprintf (stderr, "[%s]\n", data);
}

void
j_setup_filter (struct session *sess)
{
	if (my_filter) iks_filter_delete (my_filter);
	my_filter = iks_filter_new ();
	iks_filter_add_rule (my_filter, (iksFilterHook *) on_result, sess,
		IKS_RULE_TYPE, IKS_PAK_IQ,
		IKS_RULE_SUBTYPE, IKS_TYPE_RESULT,
		IKS_RULE_ID, "auth",
		IKS_RULE_DONE);
	iks_filter_add_rule (my_filter, on_error, sess,
		IKS_RULE_TYPE, IKS_PAK_IQ,
		IKS_RULE_SUBTYPE, IKS_TYPE_ERROR,
		IKS_RULE_ID, "auth",
		IKS_RULE_DONE);
	iks_filter_add_rule (my_filter, (iksFilterHook *) on_roster, sess,
		IKS_RULE_TYPE, IKS_PAK_IQ,
		IKS_RULE_SUBTYPE, IKS_TYPE_RESULT,
		IKS_RULE_ID, "roster",
		IKS_RULE_DONE);
}

void
j_connect (char *jabber_id, char *pass, int set_roster)
{
	struct session sess;
	int e;

	memset (&sess, 0, sizeof (sess));
	sess.prs = iks_stream_new (IKS_NS_CLIENT, &sess, (iksStreamHook *) on_stream);
	if (opt_log) iks_set_log_hook (sess.prs, (iksLogHook *) on_log);
	sess.acc = iks_id_new (iks_parser_stack (sess.prs), jabber_id);
	if (NULL == sess.acc->resource) {
		/* user gave no resource name, use the default */
		char *tmp;
		tmp = iks_malloc (strlen (sess.acc->user) + strlen (sess.acc->server) + 9 + 3);
		sprintf (tmp, "%s@%s/%s", sess.acc->user, sess.acc->server, "iksroster");
		sess.acc = iks_id_new (iks_parser_stack (sess.prs), tmp);
		iks_free (tmp);
	}
	sess.pass = pass;
	sess.set_roster = set_roster;

	j_setup_filter (&sess);

	e = iks_connect_tcp (sess.prs, sess.acc->server, IKS_JABBER_PORT);
	switch (e) {
		case IKS_OK:
			break;
		case IKS_NET_NODNS:
			j_error ("hostname lookup failed");
		case IKS_NET_NOCONN:
			j_error ("connection failed");
		default:
			j_error ("io error");
	}

	sess.counter = opt_timeout;
	while (1) {
		e = iks_recv (sess.prs, 1);
		if (IKS_HOOK == e) break;
		if (IKS_NET_TLSFAIL == e) j_error ("tls handshake failed");
		if (IKS_OK != e) j_error ("io error");
		sess.counter--;
		if (sess.counter == 0) j_error ("network timeout");
	}
	iks_parser_delete (sess.prs);
}

int
main (int argc, char *argv[])
{
	char *from = NULL;
	char *to = NULL;
	char *file = NULL;
	char from_pw[128], to_pw[128];
	int c;
#ifdef HAVE_GETOPT_LONG
	int i;

	while ((c = getopt_long (argc, argv, shortopts, longopts, &i)) != -1) {
#else
	while ((c = getopt (argc, argv, shortopts)) != -1) {
#endif
		switch (c) {
			case 'b':
				from = optarg;
				printf ("Password for %s: ", optarg);
				fflush (stdout);
				fgets (from_pw, 127, stdin);
				strtok (from_pw, "\r\n");
				break;
			case 'r':
				to = optarg;
				printf ("Password for %s: ", optarg);
				fflush (stdout);
				fgets (to_pw, 127, stdin);
				strtok (to_pw, "\r\n");
				break;
			case 'f':
				file = strdup (optarg);
				break;
			case 't':
				opt_timeout = atoi (optarg);
				if (opt_timeout < 10) opt_timeout = 10;
				break;
			case 's':
				if (!iks_has_tls ()) {
					puts ("Cannot make encrypted connections.");
					puts ("iksemel library is not compiled with GnuTLS support.");
					exit (1);
				}
				opt_use_tls = 1;
				break;
			case 'a':
				opt_use_sasl = 1;
				break;
			case 'l':
				opt_log = 1;
				break;
			case 'h':
				print_usage ();
				exit (0);
			case 'V':
				puts ("iksroster (iksemel) "VERSION);
				exit (0);
		}
	}
	if (from == NULL && to == NULL) {
		puts ("What I'm supposed to do?");
		print_usage ();
		exit (1);
	}
	if (to && (from == NULL && file == NULL)) {
		puts ("Store which roster?");
		print_usage ();
		exit (1);
	}

#ifdef _WIN32
	WSADATA wsaData;
	WSAStartup (MAKEWORD (1,1), &wsaData);
#endif

	if (from) {
		j_connect (from, from_pw, 0);
		if (file) {
			switch (iks_save (file, my_roster)) {
				case IKS_OK:
					break;
				case IKS_FILE_NOACCESS:
					j_error ("cannot write to file");
				default:
					j_error ("file io error");
			}
		}
	} else {
		switch (iks_load (file, &my_roster)) {
			case IKS_OK:
				break;
			case IKS_FILE_NOFILE:
				j_error ("file not found");
			case IKS_FILE_NOACCESS:
				j_error ("cannot read file");
			default:
				j_error ("file io error");
		}
	}
	if (to) {
		j_connect (to, to_pw, 1);
	}

#ifdef _WIN32
	WSACleanup ();
#endif

	return 0;
}
