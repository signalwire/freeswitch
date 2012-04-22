/*
libcsv - parse and write csv data
Copyright (C) 2007  Robert Gamble

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#if ___STDC_VERSION__ >= 199901L
#  include <stdint.h>
#else
#  define SIZE_MAX ((size_t)-1) /* C89 doesn't have stdint.h or SIZE_MAX */
#endif

#include "celliax_libcsv.h"

#define VERSION "1.0.0"

#define ROW_NOT_BEGUN           0
#define FIELD_NOT_BEGUN         1
#define FIELD_BEGUN             2
#define FIELD_MIGHT_HAVE_ENDED  3

/*
  Explanation of states
  ROW_NOT_BEGUN    There have not been any fields encountered for this row
  FIELD_NOT_BEGUN  There have been fields but we are currently not in one
  FIELD_BEGUN      We are in a field
  FIELD_MIGHT_HAVE_ENDED
                   We encountered a double quote inside a quoted field, the
                   field is either ended or the quote is literal
*/

#define MEM_BLK_SIZE 128

#define SUBMIT_FIELD(p) \
  do { \
   if (!(p)->quoted) \
     (p)->entry_pos -= (p)->spaces; \
   if (cb1) \
     cb1(p->entry_buf, (p)->entry_pos, data); \
   (p)->pstate = FIELD_NOT_BEGUN; \
   (p)->entry_pos = (p)->quoted = (p)->spaces = 0; \
 } while (0)

#define SUBMIT_ROW(p, c) \
  do { \
    if (cb2) \
      cb2(c, data); \
    (p)->pstate = ROW_NOT_BEGUN; \
    (p)->entry_pos = (p)->quoted = (p)->spaces = 0; \
  } while (0)

#define SUBMIT_CHAR(p, c) ((p)->entry_buf[(p)->entry_pos++] = (c))

static char *csv_errors[] = {"success",
                             "error parsing data while strict checking enabled",
                             "memory exhausted while increasing buffer size",
                             "data size too large",
                             "invalid status code"};

int
csv_error(struct csv_parser *p)
{
  return p->status;
}

char *
csv_strerror(int status)
{
  if (status >= CSV_EINVALID || status < 0)
    return csv_errors[CSV_EINVALID];
  else
    return csv_errors[status];
}

int
csv_opts(struct csv_parser *p, unsigned char options)
{
  if (p == NULL)
    return -1;

  p->options = options;
  return 0;
}

int
csv_init(struct csv_parser **p, unsigned char options)
{
  /* Initialize a csv_parser object returns 0 on success, -1 on error */
  if (p == NULL)
    return -1;

  if ((*p = malloc(sizeof(struct csv_parser))) == NULL)
    return -1;

  if ( ((*p)->entry_buf = malloc(MEM_BLK_SIZE)) == NULL ) {
    free(*p);
    return -1;
  }
  (*p)->pstate = ROW_NOT_BEGUN;
  (*p)->quoted = 0;
  (*p)->spaces = 0;
  (*p)->entry_pos = 0;
  (*p)->entry_size = MEM_BLK_SIZE;
  (*p)->status = 0;
  (*p)->options = options;

  return 0;
}

void
csv_free(struct csv_parser *p)
{
  /* Free the entry_buffer and the csv_parser object */
  if (p == NULL)
    return;

  if (p->entry_buf)
    free(p->entry_buf);

  free(p);
  return;
}

int
csv_fini(struct csv_parser *p, void (*cb1)(char *, size_t, void *), void (*cb2)(char c, void *), void *data)
{
  /* Finalize parsing.  Needed, for example, when file does not end in a newline */
  if (p == NULL)
    return -1;

  switch (p->pstate) {
    case FIELD_MIGHT_HAVE_ENDED:
      p->entry_pos -= p->spaces + 1;  /* get rid of spaces and original quote */
    case FIELD_NOT_BEGUN:
    case FIELD_BEGUN:
      SUBMIT_FIELD(p);
      SUBMIT_ROW(p, 0);
    case ROW_NOT_BEGUN: /* Already ended properly */
      ;
  }

  p->spaces = p->quoted = p->entry_pos = p->status = 0;
  p->pstate = ROW_NOT_BEGUN;

  return 0;
}
  
size_t
csv_parse(struct csv_parser *p, const char *s, size_t len, void (*cb1)(char *, size_t, void *), void (*cb2)(char c, void *), void *data)
{
  char c;  /* The character we are currently processing */
  size_t pos = 0;  /* The number of characters we have processed in this call */

  while (pos < len) {
    /* Check memory usage */
    if (p->entry_pos == p->entry_size) {
      size_t to_add = MEM_BLK_SIZE;
      void *vp;
      while ( p->entry_size >= SIZE_MAX - to_add )
        to_add /= 2;
      if (!to_add) {
        p->status = CSV_ETOOBIG;
        return pos;
      }
      while ((vp = realloc(p->entry_buf, p->entry_size + to_add)) == NULL) {
        to_add /= 2;
        if (!to_add) {
          p->status = CSV_ENOMEM;
          return pos;
        }
      }
      p->entry_buf = vp;
      p->entry_size += to_add;
    }

    c = s[pos++];
    switch (p->pstate) {
      case ROW_NOT_BEGUN:
      case FIELD_NOT_BEGUN:
        if (c == CSV_SPACE || c == CSV_TAB) { /* Space or Tab */
          continue;
        } else if (c == CSV_CR || c == CSV_LF) { /* Carriage Return or Line Feed */
          if (p->pstate == FIELD_NOT_BEGUN) {
            SUBMIT_FIELD(p);
            SUBMIT_ROW(p, c); 
          } else {  /* ROW_NOT_BEGUN */
            /* Don't submit empty rows by default */
            if (p->options & CSV_REPALL_NL) {
              SUBMIT_ROW(p, c);
            }
          }
          continue;
        } else if ( (!(p->options & CSV_USE_SEMICOLON_SEPARATOR) && (c == CSV_COMMA)) || ((p->options & CSV_USE_SEMICOLON_SEPARATOR) && (c == CSV_SEMICOLON)) ) { /* Comma or SemiColon */
          SUBMIT_FIELD(p);
          break;
        } else if (c == CSV_QUOTE) { /* Quote */
          p->pstate = FIELD_BEGUN;
          p->quoted = 1;
        } else {               /* Anything else */
          p->pstate = FIELD_BEGUN;
          p->quoted = 0;
          SUBMIT_CHAR(p, c);
        }
        break;
      case FIELD_BEGUN:
        if (c == CSV_QUOTE) {         /* Quote */
          if (p->quoted) {
            SUBMIT_CHAR(p, c);
            p->pstate = FIELD_MIGHT_HAVE_ENDED;
          } else {
            /* STRICT ERROR - double quote inside non-quoted field */
            if (p->options & CSV_STRICT) {
              p->status = CSV_EPARSE;
              return pos-1;
            }
            SUBMIT_CHAR(p, c);
            p->spaces = 0;
          }
        } else if ((!(p->options & CSV_USE_SEMICOLON_SEPARATOR) && (c == CSV_COMMA)) || ((p->options & CSV_USE_SEMICOLON_SEPARATOR) && (c == CSV_SEMICOLON))) {  /* Comma or SemiColon */
          if (p->quoted) {
            SUBMIT_CHAR(p, c);
          } else {
            SUBMIT_FIELD(p);
          }
        } else if (c == CSV_CR || c == CSV_LF) {  /* Carriage Return or Line Feed */
          if (!p->quoted) {
            SUBMIT_FIELD(p);
            SUBMIT_ROW(p, c);
          } else {
            SUBMIT_CHAR(p, c);
          }
        } else if (!p->quoted && (c == CSV_SPACE || c == CSV_TAB)) { /* Tab or space for non-quoted field */
            SUBMIT_CHAR(p, c);
            p->spaces++;
        } else {  /* Anything else */
          SUBMIT_CHAR(p, c);
          p->spaces = 0;
        }
        break;
      case FIELD_MIGHT_HAVE_ENDED:
        /* This only happens when a quote character is encountered in a quoted field */
        if ((!(p->options & CSV_USE_SEMICOLON_SEPARATOR) && (c == CSV_COMMA)) || ((p->options & CSV_USE_SEMICOLON_SEPARATOR) && (c == CSV_SEMICOLON))) {  /* Comma or SemiColon */
          p->entry_pos -= p->spaces + 1;  /* get rid of spaces and original quote */
          SUBMIT_FIELD(p);
        } else if (c == CSV_CR || c == CSV_LF) {  /* Carriage Return or Line Feed */
          p->entry_pos -= p->spaces + 1;  /* get rid of spaces and original quote */
          SUBMIT_FIELD(p);
          SUBMIT_ROW(p, c);
        } else if (c == CSV_SPACE || c == CSV_TAB) {  /* Space or Tab */
          SUBMIT_CHAR(p, c);
          p->spaces++;
        } else if (c == CSV_QUOTE) {  /* Quote */
          if (p->spaces) {
            /* STRICT ERROR - unescaped double quote */
            if (p->options & CSV_STRICT) {
              p->status = CSV_EPARSE;
              return pos-1;
            }
            p->spaces = 0;
            SUBMIT_CHAR(p, c);
          } else {
            /* Two quotes in a row */
            p->pstate = FIELD_BEGUN;
          }
        } else {  /* Anything else */
          /* STRICT ERROR - unescaped double quote */
          if (p->options & CSV_STRICT) {
            p->status = CSV_EPARSE;
            return pos-1;
          }
          p->pstate = FIELD_BEGUN;
          p->spaces = 0;
          SUBMIT_CHAR(p, c);
        }
        break;
     default:
       break;
    }
  }
  return pos;
}

size_t
csv_write (char *dest, size_t dest_size, const char *src, size_t src_size)
{
  size_t chars = 0;

  if (src == NULL)
    return 0;

  if (dest == NULL)
    dest_size = 0;

  if (dest_size > 0)
    *dest++ = '"';
  chars++;

  while (src_size) {
    if (*src == '"') {
      if (dest_size > chars)
        *dest++ = '"';
      if (chars < SIZE_MAX) chars++;
    }
    if (dest_size > chars)
      *dest++ = *src;
    if (chars < SIZE_MAX) chars++;
    src_size--;
    src++;
  }

  if (dest_size > chars)
    *dest = '"';
  if (chars < SIZE_MAX) chars++;

  return chars;
}

int
csv_fwrite (FILE *fp, const char *src, size_t src_size)
{
  if (fp == NULL || src == NULL)
    return 0;

  if (fputc('"', fp) == EOF)
    return EOF;

  while (src_size) {
    if (*src == '"') {
      if (fputc('"', fp) == EOF)
        return EOF;
    }
    if (fputc(*src, fp) == EOF)
      return EOF;
    src_size--;
    src++;
  }

  if (fputc('"', fp) == EOF) {
    return EOF;
  }

  return 0;
}
