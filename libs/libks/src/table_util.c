/*
 * Hash table utility program.
 *
 * Copyright 2000 by Gray Watson
 *
 * This file is part of the table package.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose and without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies,
 * and that the name of Gray Watson not be used in advertising or
 * publicity pertaining to distribution of the document or software
 * without specific, written prior permission.
 *
 * Gray Watson makes no representations about the suitability of the
 * software described herein for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * The author may be reached via http://256.com/gray/
 *
 * $Id: table_util.c,v 1.5 2000/03/09 03:30:42 gray Exp $
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "table.h"

static	char	*rcs_id =
  "$Id: table_util.c,v 1.5 2000/03/09 03:30:42 gray Exp $";

#define WRITE_MODE	0640			/* mode to write out table */
#define SPECIAL_CHARS	"e\033^^\"\"''\\\\n\nr\rt\tb\bf\fa\007"

/*
 * expand_chars
 *
 * DESCRIPTION:
 *
 * Copies a buffer into a output buffer while translates
 * non-printables into %03o octal values.  If it can, it will also
 * translate certain \ characters (\r, \n, etc.) into \\%c.  The
 * routine is useful for printing out binary values.
 *
 * NOTE: It does _not_ add a \0 at the end of the output buffer.
 *
 * RETURNS:
 *
 * Returns the number of characters added to the output buffer.
 *
 * ARGUMENTS:
 *
 * buf - the buffer to convert.
 *
 * buf_size - size of the buffer.  If < 0 then it will expand till it
 * sees a \0 character.
 *
 * out - destination buffer for the convertion.
 *
 * out_size - size of the output buffer.
 */
int	expand_chars(const void *buf, const int buf_size,
		     char *out, const int out_size)
{
  int			buf_c;
  const unsigned char	*buf_p, *spec_p;
  char	 		*max_p, *out_p = out;
  
  /* setup our max pointer */
  max_p = out + out_size;
  
  /* run through the input buffer, counting the characters as we go */
  for (buf_c = 0, buf_p = (const unsigned char *)buf;; buf_c++, buf_p++) {
    
    /* did we reach the end of the buffer? */
    if (buf_size < 0) {
      if (*buf_p == '\0') {
	break;
      }
    }
    else {
      if (buf_c >= buf_size) {
	break;
      }
    }
    
    /* search for special characters */
    for (spec_p = (unsigned char *)SPECIAL_CHARS + 1;
	 *(spec_p - 1) != '\0';
	 spec_p += 2) {
      if (*spec_p == *buf_p) {
	break;
      }
    }
    
    /* did we find one? */
    if (*(spec_p - 1) != '\0') {
      if (out_p + 2 >= max_p) {
	break;
      }
      (void)sprintf(out_p, "\\%c", *(spec_p - 1));
      out_p += 2;
      continue;
    }
    
    /* print out any 7-bit printable characters */
    if (*buf_p < 128 && isprint(*buf_p)) {
      if (out_p + 1 >= max_p) {
	break;
      }
      *out_p = *(char *)buf_p;
      out_p += 1;
    }
    else {
      if (out_p + 4 >= max_p) {
	break;
      }
      (void)sprintf(out_p, "\\%03o", *buf_p);
      out_p += 4;
    }
  }
  
  return out_p - out;
}

/*
 * dump_table
 *
 * DESCRIPTION:
 *
 * Dump a table file to the screen.
 *
 * RETURNS:
 *
 * None.
 *
 * ARGUMENTS:
 *
 * tab_p - a table pointer that we are dumping.
 */
static	void	dump_table(table_t *tab_p)
{
  char		buf[10240];
  void		*key_p, *data_p;
  int		ret, key_size, data_size, len, entry_c;
  
  for (ret = table_first(tab_p, (void **)&key_p, &key_size,
			 (void **)&data_p, &data_size), entry_c = 0;
       ret == TABLE_ERROR_NONE;
       ret = table_next(tab_p, (void **)&key_p, &key_size,
			(void **)&data_p, &data_size), entry_c++) {
    /* expand the key */
    len = expand_chars(key_p, key_size, buf, sizeof(buf));
    (void)printf("%d: key '%.*s' (%d), ", entry_c, len, buf, len);
    /* now dump the data */
    len = expand_chars(data_p, data_size, buf, sizeof(buf));
    (void)printf("data '%.*s' (%d)\n", len, buf, len);
  }
}

/*
 * usage
 *
 * DESCRIPTION:
 *
 * Print the usage message to stderr.
 *
 * RETURNS:
 *
 * None.
 *
 * ARGUMENTS:
 *
 * tab_p - a table pointer that we are dumping.
 */
static	void	usage(void)
{
  (void)fprintf(stderr,
		"Usage: table_util\n"
		"  [-b number]   or --buckets    num buckets to adjust table\n"
		"  [-o file]     or --out-file   output filename\n"
		"  [-v]          or --verbose    verbose messages\n"
		"  file                          input table filename\n");
  exit(1);      
}

int	main(int argc, char **argv)
{
  table_t	*tab_p;
  char		do_write = 0, verbose = 0;
  char		*out_file = NULL, *in_file;
  int		ret, entry_n, bucket_n, num_buckets = 0;
  
  /* process the args */
  for (argc--, argv++; argc > 0 && **argv == '-'; argc--, argv++) {
    
    switch (*(*argv + 1)) {
      
    case 'b':
      argc--, argv++;
      if (argc == 0) {
	usage();
      }
      num_buckets = atoi(*argv);
      break;
      
    case 'o':
      argc--, argv++;
      if (argc == 0) {
	usage();
      }
      out_file = *argv;
      break;
      
    case 'v':
      verbose = 1;
      break;
      
    default:
      usage();
      break;
    }
  }
  
  if (argc != 1) {
    usage();
  }
  
  /* take the last argument as the input file */
  in_file = *argv;
  
  /* read in the table from disk */
  tab_p = table_read(in_file, &ret);
  if (tab_p == NULL) {
    (void)fprintf(stderr, "table_util: unable to table_read from '%s': %s\n",
		  in_file, table_strerror(ret));
    exit(1);
  }
  
  /* get info about the table */
  ret = table_info(tab_p, &bucket_n, &entry_n);
  if (ret != TABLE_ERROR_NONE) {
    (void)fprintf(stderr,
		  "table_util: unable to get info on table in '%s': %s\n",
		  in_file, table_strerror(ret));
    exit(1);
  }
  
  (void)printf("Read table of %d buckets and %d entries from '%s'\n",
	       bucket_n, entry_n, in_file);
  
  if (verbose) {
    dump_table(tab_p);
  }
  
  if (num_buckets > 0) {
    /* adjust the table's buckets */
    ret = table_adjust(tab_p, num_buckets);
    if (ret != TABLE_ERROR_NONE) {
      (void)fprintf(stderr,
		    "table_util: unable to adjust table to %d buckets: %s\n",
		    num_buckets, table_strerror(ret));
      exit(1);
    }
    do_write = 1;
  }
  
  /* did we modify the table at all */
  if (do_write) {
    if (out_file == NULL) {
      out_file = in_file;
    }
    
    /* write out our table */
    ret = table_write(tab_p, out_file, WRITE_MODE);
    if (ret != TABLE_ERROR_NONE) {
      (void)fprintf(stderr, "table_util: unable to write table to '%s': %s\n",
		    out_file, table_strerror(ret));
      exit(1);
    }
    
    (void)printf("Wrote table to '%s'\n", out_file);
  }
  
  /* free the table */
  ret = table_free(tab_p);
  if (ret != TABLE_ERROR_NONE) {
    (void)fprintf(stderr, "table_util: unable to free table: %s\n",
		  table_strerror(ret));
    /* NOTE: not a critical error */
  }
  
  exit(0);
}
