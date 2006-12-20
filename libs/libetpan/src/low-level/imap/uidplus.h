#ifndef UIDPLUS_H

#define UIDPLUS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libetpan/libetpan-config.h>
#include <libetpan/mailimap_types.h>
#include <libetpan/uidplus_types.h>

LIBETPAN_EXPORT
extern struct mailimap_extension_api mailimap_extension_uidplus;

LIBETPAN_EXPORT
int mailimap_uid_expunge(mailimap * session, struct mailimap_set * set);

LIBETPAN_EXPORT
int mailimap_uidplus_copy(mailimap * session, struct mailimap_set * set,
    const char * mb,
    uint32_t * uidvalidity_result,
    struct mailimap_set ** source_result,
    struct mailimap_set ** dest_result);

LIBETPAN_EXPORT
int mailimap_uidplus_uid_copy(mailimap * session, struct mailimap_set * set,
    const char * mb,
    uint32_t * uidvalidity_result,
    struct mailimap_set ** source_result,
    struct mailimap_set ** dest_result);

LIBETPAN_EXPORT
int mailimap_uidplus_append(mailimap * session, const char * mailbox,
    struct mailimap_flag_list * flag_list,
    struct mailimap_date_time * date_time,
    const char * literal, size_t literal_size,
    uint32_t * uidvalidity_result,
    uint32_t * uid_result);

LIBETPAN_EXPORT
int mailimap_uidplus_append_simple(mailimap * session, const char * mailbox,
    const char * content, uint32_t size,
    uint32_t * uidvalidity_result,
    uint32_t * uid_result);

LIBETPAN_EXPORT
int mailimap_has_uidplus(mailimap * session);

#ifdef __cplusplus
}
#endif

#endif
