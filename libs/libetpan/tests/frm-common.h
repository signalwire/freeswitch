#ifndef FRM_COMMON_H

#define FRM_COMMON_H

#include <libetpan/libetpan.h>

void get_from_value(struct mailimf_single_fields * fields,
    char ** from, int * is_addr);

void strip_crlf(char * str);

void print_mail_info(char * prefix, mailmessage * msg);

#endif
