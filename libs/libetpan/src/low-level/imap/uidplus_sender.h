#ifndef UIDPLUS_SENDER_H

#define UIDPLUS_SENDER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libetpan/libetpan-config.h>

#include "mailimap_types.h"
#include "mailstream_types.h"

int
mailimap_uid_expunge_send(mailstream * fd,
    struct mailimap_set * set);

#ifdef __cplusplus
}
#endif

#endif
