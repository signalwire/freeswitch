/* iksemel (XML parser for Jabber)
** Copyright (C) 2000-2003 Gurer Ozen <madcat@e-kolay.net>
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU Lesser General Public License.
*/

#include "common.h"
#include "iksemel.h"

struct hash_s;
typedef struct hash_s hash;

hash *hash_new (unsigned int table_size);
char *hash_insert (hash *table, const char *name);
void hash_print (hash *h, char *title_fmt, char *line_fmt);
void hash_delete (hash *table);

#include <sys/stat.h>

#ifdef HAVE_GETOPT_LONG
#include <getopt.h>
#endif

#ifdef HAVE_GETOPT_LONG
static struct option longopts[] = {
	{ "stats", 0, 0, 's' },
	{ "histogram", 0, 0, 't' },
	{ "help", 0, 0, 'h' },
	{ "version", 0, 0, 'V' },
	{ 0, 0, 0, 0 }
};
#endif

static char *shortopts = "sthV";

static void
print_usage (void)
{
	puts ("Usage: ikslint [OPTIONS] FILE\n"
		"This tool checks the well-formedness of an XML document.\n"
		" -s, --stats      Print statistics.\n"
		" -t, --histogram  Print tag histogram.\n"
		" -h, --help       Print this text and exit.\n"
		" -V, --version    Print version and exit.\n"
#ifndef HAVE_GETOPT_LONG
		"(long options are not supported on your system)\n"
#endif
		"Report bugs to <iksemel-dev@jabberstudio.org>.");
}

/* calculate and print statistics */
int lint_pr_stats = 0;

/* print tag histogram */
int lint_pr_hist = 0;

hash *tag_table;

char **tag_list;
int tag_size, tag_pos;

void
tag_push (const char *name)
{
	if (!tag_list) {
		tag_size = 128;
		tag_list = malloc (sizeof (char *) * tag_size);
		if (!tag_list) exit (2);
	}
	tag_list[tag_pos] = hash_insert (tag_table, name);
	if (!tag_list[tag_pos]) exit (2);
	tag_pos++;
	if (tag_pos == tag_size) {
		char **tmp;
		tmp = malloc (sizeof (char *) * tag_size * 2);
		if (!tmp) exit (2);
		memcpy (tmp, tag_list, sizeof (char *) * tag_size);
		free (tag_list);
		tag_list = tmp;
		tag_size *= 2;
	}
}

char *
tag_pull (void)
{
	tag_pos--;
	return tag_list[tag_pos];
}

struct stats {
	unsigned int level;
	unsigned int max_depth;
	unsigned int nr_tags;
	unsigned int nr_stags;
	unsigned int cdata_size;
};

int
tagHook (void *udata, char *name, char **atts, int type)
{
	struct stats *st = (struct stats *) udata;
	char *tmp;

	switch (type) {
		case IKS_OPEN:
			tag_push (name);
			st->level++;
			if (st->level > st->max_depth) st->max_depth = st->level;
			break;
		case IKS_CLOSE:
			tmp = tag_pull ();
			if (iks_strcmp (tmp, name) != 0) {
				fprintf (stderr, "Tag mismatch, expecting '%s', got '%s'.\n",
					tmp, name);
				return IKS_HOOK;
			}
			st->level--;
			st->nr_tags++;
			break;
		case IKS_SINGLE:
			if (NULL == hash_insert (tag_table, name)) exit (2);
			st->nr_stags++;
			break;
	}
	return IKS_OK;
}

int
cdataHook (void *udata, char *data, size_t len)
{
	struct stats *st = (struct stats *) udata;

	st->cdata_size += len;
	return IKS_OK;
}

void
check_file (char *fname)
{
	iksparser *prs;
	struct stats st;
	FILE *f;
	char *buf;
	struct stat fs;
	size_t sz, blk, ret, pos;
	enum ikserror err;
	int done;

	memset (&st, 0, sizeof (struct stats));
	prs = iks_sax_new (&st, tagHook, cdataHook);
	if (NULL == prs) exit (2);

	if (fname) {
		if (stat (fname, &fs) != 0) {
			fprintf (stderr, "Cannot access file '%s'.\n", fname);
			exit (1);
		}
		sz = fs.st_size;
#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
		blk = fs.st_blksize;
#else
		blk = 4096;
#endif
		f = fopen (fname, "r");
		if (!f) {
			fprintf (stderr, "Cannot open file '%s'.\n", fname);
			exit (1);
		}
		buf = malloc (blk);
		if (!buf) {
			fclose (f);
			fprintf (stderr, "Cannot allocate %d bytes.\n", blk);
			exit (2);
		}
	} else {
		f = stdin;
		blk = 4096;
		sz = 0;
		buf = malloc (blk);
		if (!buf) exit (2);
	}

	tag_table = hash_new (367);
	if (!tag_table) exit (2);

	pos = 0;
	done = 0;
	while (0 == done) {
		ret = fread (buf, 1, blk, f);
		pos += ret;
		if (feof (f)) {
			done = 1;
		} else {
			if (ret != blk) {
				if (fname)
					fprintf (stderr, "Read error in file '%s'.\n", fname);
				else
					fprintf (stderr, "Read error in stream.\n");
				exit (1);
			}
		}
		err = iks_parse (prs, buf, ret, done);
		switch (err) {
			case IKS_OK:
				break;
			case IKS_NOMEM:
				exit (2);
			case IKS_BADXML:
				if (fname)
					fprintf (stderr, "Invalid xml at byte %ld, line %ld in file '%s'.\n",
						iks_nr_bytes (prs), iks_nr_lines (prs), fname);
				else
					fprintf (stderr, "Invalid xml at byte %ld, line %ld in stream.\n",
						iks_nr_bytes (prs), iks_nr_lines (prs));
				exit (1);
			case IKS_HOOK:
				if (fname)
					fprintf (stderr, "Byte %ld, line %ld in file '%s'.\n",
						iks_nr_bytes (prs), iks_nr_lines (prs), fname);
				else
					fprintf (stderr, "Byte %ld, line %ld in stream.\n",
						iks_nr_bytes (prs), iks_nr_lines (prs));
				exit (1);
		}
	}

	free (buf);
	if (fname) fclose (f);

	if (fname && (lint_pr_stats || lint_pr_hist)) {
		printf ("File '%s' (%d bytes):\n", fname, sz);
	}
	if (lint_pr_stats) {
		printf ("Tags: %d pairs, %d single, %d max depth.\n", st.nr_tags, st.nr_stags, st.max_depth);
		printf ("Total size of character data: %d bytes.\n", st.cdata_size);
	}
	if (lint_pr_hist) {
		hash_print (tag_table,
			"Histogram of %d unique tags:\n",
			"<%s> %d times.\n");
	}
	hash_delete (tag_table);

	iks_parser_delete (prs);
}

int
main (int argc, char *argv[])
{
	int c;

#ifdef HAVE_GETOPT_LONG
	int i;
	while ((c = getopt_long (argc, argv, shortopts, longopts, &i)) != -1) {
#else
	while ((c = getopt (argc, argv, shortopts)) != -1) {
#endif
		switch (c) {
			case 's':
				lint_pr_stats = 1;
				break;
			case 't':
				lint_pr_hist = 1;
				break;
			case 'h':
				print_usage ();
				exit (0);
			case 'V':
				puts ("ikslint (iksemel) "VERSION);
				exit (0);
		}
	}
	if (!argv[optind]) {
		check_file (NULL);
	} else {
		for (; optind < argc; optind++) {
			check_file (argv[optind]);
		}
	}

	return 0;
}
