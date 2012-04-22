#ifndef LIBCSV_H__
#define LIBCSV_H__
#include <stdlib.h>
#include <stdio.h>

/* Error Codes */
#define CSV_SUCCESS 0
#define CSV_EPARSE 1   /* Parse error in strict mode */
#define CSV_ENOMEM 2   /* Out of memory while increasing buffer size */
#define CSV_ETOOBIG 3  /* Buffer larger than SIZE_MAX needed */
#define CSV_EINVALID 4 /* Invalid code, should never receive this from csv_error */

/* parser options */
#define CSV_STRICT 1    /* enable strict mode */
#define CSV_REPALL_NL 2 /* report all unquoted carriage returns and linefeeds */
#define CSV_USE_SEMICOLON_SEPARATOR 4 /* use CSV_SEMICOLON as separator instead of CSV_COMMA */

/* Character values */
#define CSV_TAB    0x09
#define CSV_SPACE  0x20
#define CSV_CR     0x0d
#define CSV_LF     0x0a
#define CSV_COMMA  0x2c
#define CSV_SEMICOLON  0x3b /* ; */
#define CSV_QUOTE  0x22

struct csv_parser {
  int pstate;         /* Parser state */
  int quoted;         /* Is the current field a quoted field? */
  size_t spaces;      /* Number of continious spaces after quote or in a non-quoted field */
  char * entry_buf;   /* Entry buffer */
  size_t entry_pos;   /* Current position in entry_buf (and current size of entry) */
  size_t entry_size;  /* Size of buffer */
  int status;         /* Operation status */
  unsigned char options;
};

int csv_init(struct csv_parser **p, unsigned char options);
int csv_fini(struct csv_parser *p, void (*cb1)(char *, size_t, void *), void (*cb2)(char, void *), void *data);
void csv_free(struct csv_parser *p);
int csv_error(struct csv_parser *p);
char * csv_strerror(int error);
size_t csv_parse(struct csv_parser *p, const char *s, size_t len, void (*cb1)(char *, size_t, void *), void (*cb2)(char, void *), void *data);
size_t csv_write(char *dest, size_t dest_size, const char *src, size_t src_size);
int csv_fwrite(FILE *fp, const char *src, size_t src_size);
int csv_opts(struct csv_parser *p, unsigned char options);

#endif
