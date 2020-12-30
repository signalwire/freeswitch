/* iksemel (XML parser for Jabber)
** Copyright (C) 2000-2003 Gurer Ozen <madcat@e-kolay.net>
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU Lesser General Public License.
*/

#include "common.h"
#include "iksemel.h"
#include "perf.h"

#include <sys/stat.h>

#ifdef HAVE_GETOPT_LONG
#include <getopt.h>
#endif

#ifdef HAVE_GETOPT_LONG
static struct option longopts[] = {
	{ "all", 0, 0, 'a' },
	{ "sax", 0, 0, 's' },
	{ "dom", 0, 0, 'd' },
	{ "serialize", 0, 0, 'e' },
	{ "sha1", 0, 0, '1' },
	{ "block", required_argument, 0, 'b' },
	{ "memdbg", 0, 0, 'm' },
	{ "help", 0, 0, 'h' },
	{ "version", 0, 0, 'V' },
	{ 0, 0, 0, 0 }
};
#endif

static char *shortopts = "asde1b:mhV";

static void
print_usage (void)
{
	puts ("Usage: iksperf [OPTIONS] FILE\n"
		"This tool measures the performance of the iksemel library.\n"
		" -a, --all         Make all tests.\n"
		" -s, --sax         Sax test.\n"
		" -d, --dom         Tree generating test.\n"
		" -e, --serialize   Tree serializing test.\n"
		" -1, --sha1        SHA1 hashing test.\n"
		" -b, --block SIZE  Parse the file in SIZE byte blocks.\n"
		" -m, --memdbg      Trace malloc and free calls.\n"
		" -h, --help        Print this text and exit.\n"
		" -V, --version     Print version and exit.\n"
#ifndef HAVE_GETOPT_LONG
		"(long options are not supported on your system)\n"
#endif
		"Report bugs to <iksemel-dev@jabberstudio.org>.");
}

/* if not 0, file is parsed in block_size byte blocks */
int block_size;

char *load_file (const char *fname, int *sizeptr)
{
	FILE *f;
	char *buf;
	struct stat fs;
	size_t size, ret;

	if (stat (fname, &fs) != 0) {
		fprintf (stderr, "Cannot access file '%s'.\n", fname);
		exit (1);
	}
	size = fs.st_size;

	printf ("Test file '%s' (%d bytes):\n", fname, size);

	f = fopen (fname, "rb");
	if (!f) {
		fprintf (stderr, "Cannot open file.\n");
		exit (1);
	}

	buf = malloc (size);
	if (!buf) {
		fclose (f);
		fprintf (stderr, "Cannot allocate %d bytes for buffer.\n", size);
		exit (2);
	}

	ret = fread (buf, 1, size, f);
	if (ret < size) {
		fprintf (stderr, "Read error in file.\n");
		exit (1);
	}

	*sizeptr = size;
	fclose (f);
	return buf;
}

/* stats */
int sax_tag;
int sax_cdata;

int
tagHook (void *udata, char *name, char **atts, int type)
{
	++sax_tag;
	return IKS_OK;
}

int
cdataHook (void *udata, char *data, size_t len)
{
	++sax_cdata;
	return IKS_OK;
}

void
sax_test (char *buf, int len)
{
	unsigned long time;
	iksparser *prs;
	int bs, i, err;

	bs = block_size;
	if (0 == bs) bs = len;
	sax_tag = 0;
	sax_cdata = 0;

	t_reset ();

	prs = iks_sax_new (NULL, tagHook, cdataHook);
	i = 0;
	while (i < len) {
		if (i + bs > len) bs = len - i;
		err = iks_parse (prs, buf + i, bs, 0);
		switch (err) {
			case IKS_OK:
				break;
			case IKS_NOMEM:
				exit (2);
			case IKS_BADXML:
				fprintf (stderr, "Invalid xml at byte %ld, line %ld\n",
					iks_nr_bytes (prs), iks_nr_lines (prs));
				exit (1);
			case IKS_HOOK:
				exit (1);
		}
		i += bs;
	}

	time = t_elapsed ();

	printf ("SAX: parsing took %ld milliseconds.\n", time);
	printf ("SAX: tag hook called %d, cdata hook called %d times.\n", sax_tag, sax_cdata);

	iks_parser_delete (prs);
}

void dom_test (char *buf, int len)
{
	int bs, i, err;
	iksparser *prs;
	unsigned long time;
	iks *x;
	size_t allocated, used;

	bs = block_size;
	if (0 == bs) bs = len;

	t_reset ();

	prs = iks_dom_new (&x);
	iks_set_size_hint (prs, len);
	i = 0;
	while (i < len) {
		if (i + bs > len) bs = len - i;
		err = iks_parse (prs, buf + i, bs, 0);
		switch (err) {
			case IKS_OK:
				break;
			case IKS_NOMEM:
				exit (2);
			case IKS_BADXML:
				fprintf (stderr, "Invalid xml at byte %ld, line %ld\n",
					iks_nr_bytes (prs), iks_nr_lines (prs));
				exit (1);
			case IKS_HOOK:
				exit (1);
		}
		i += bs;
	}

	time = t_elapsed ();
	iks_stack_stat (iks_stack (x), &allocated, &used);

	printf ("DOM: parsing and building the tree took %ld milliseconds.\n", time);
	printf ("DOM: ikstack: %d bytes allocated, %d bytes used.\n", allocated, used);

	t_reset ();
	iks_delete (x);
	time = t_elapsed ();
	printf ("DOM: deleting the tree took %ld milliseconds.\n", time);

	iks_parser_delete (prs);
}

void
serialize_test (char *buf, int len)
{
	unsigned long time;
	iks *x;
	iksparser *prs;
	int err;

	prs = iks_dom_new (&x);
	err = iks_parse (prs, buf, len, 1);
	switch (err) {
		case IKS_OK:
			break;
		case IKS_NOMEM:
			exit (2);
		case IKS_BADXML:
			fprintf (stderr, "Invalid xml at byte %ld, line %ld\n",
				iks_nr_bytes (prs), iks_nr_lines (prs));
			exit (1);
		case IKS_HOOK:
			exit (1);
	}
	iks_parser_delete (prs);

	t_reset ();

	iks_string (iks_stack (x), x);

	time = t_elapsed ();

	printf ("Serialize: serializing the tree took %ld milliseconds.\n", time);

	iks_delete (x);
}

void
sha_test (char *buf, int len)
{
	unsigned long time;
	iksha *s;
	char out[41];

	t_reset ();

	s = iks_sha_new ();
	iks_sha_hash (s, buf, len, 1);
	iks_sha_print (s, out);
	out[40] = '\0';
	iks_sha_delete (s);

	time = t_elapsed ();

	printf ("SHA: hashing took %ld milliseconds.\n", time);
	printf ("SHA: hash [%s]\n", out);
}

int
main (int argc, char *argv[])
{
	int test_type = 0;
	int c;

#ifdef HAVE_GETOPT_LONG
	int i;
	while ((c = getopt_long (argc, argv, shortopts, longopts, &i)) != -1) {
#else
	while ((c = getopt (argc, argv, shortopts)) != -1) {
#endif
		switch (c) {
			case 'a':
				test_type = 0xffff;
				break;
			case 's':
				test_type |= 1;
				break;
			case 'd':
				test_type |= 2;
				break;
			case 'e':
				test_type |= 4;
				break;
			case '1':
				test_type |= 8;
				break;
			case 'b':
				block_size = atoi (optarg);
				break;
			case 'm':
				m_trace ();
				break;
			case 'h':
				print_usage ();
				exit (0);
			case 'V':
				puts ("iksperf (iksemel) "VERSION);
				exit (0);
		}
	}
	for (; optind < argc; optind++) {
		char *buf;
		int len;

		buf = load_file (argv[optind], &len);
		if (test_type & 1) sax_test (buf, len);
		if (test_type == 0 || test_type & 2) dom_test (buf, len);
		if (test_type & 4) serialize_test (buf, len);
		if (test_type & 8) sha_test (buf, len);
		free (buf);
	}

	return 0;
}
