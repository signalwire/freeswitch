/* Must include apr.h first so that we can undefine
    the standard prototypes macros after it messes with
    them. */
#include "fspr.h"

#undef APR_DECLARE
#undef APR_DECLARE_NONSTD
#undef APR_DECLARE_HOOK
#undef APR_POOL_DECLARE_ACCESSOR
#undef APR_DECLARE_DATA

/* Preprocess all of the standard APR headers. */
#include "fspr_allocator.h"
#include "fspr_atomic.h"
#include "fspr_dso.h"
#include "fspr_env.h"
#include "fspr_errno.h"
#include "fspr_file_info.h"
#include "fspr_file_io.h"
#include "fspr_fnmatch.h"
#include "fspr_general.h"
#include "fspr_getopt.h"
#include "fspr_global_mutex.h"
#include "fspr_hash.h"
#include "fspr_inherit.h"
#include "fspr_lib.h"
#include "fspr_mmap.h"
#include "fspr_network_io.h"
#include "fspr_poll.h"
#include "fspr_pools.h"
#include "fspr_portable.h"
#include "fspr_proc_mutex.h"
#include "fspr_ring.h"
#include "fspr_random.h"
#include "fspr_shm.h"
#include "fspr_signal.h"
#include "fspr_strings.h"
#include "fspr_support.h"
#include "fspr_tables.h"
#include "fspr_thread_cond.h"
#include "fspr_thread_mutex.h"
#include "fspr_thread_proc.h"
#include "fspr_thread_rwlock.h"
#include "fspr_time.h"
#include "fspr_user.h"
#include "fspr_version.h"
#include "fspr_want.h"


/* Must include apu.h first so that we can undefine
    the standard prototypes macros after it messes with
    them. */
#include "apu.h"

#undef APU_DECLARE
#undef APU_DECLARE_NONSTD
#undef APU_DECLARE_DATA

/* Preprocess all of the standard APR headers. */
#include "fspr_anylock.h"
#include "fspr_base64.h"
#include "fspr_buckets.h"
#include "fspr_date.h"
#include "fspr_dbd.h"
#include "fspr_dbm.h"
#include "fspr_hooks.h"
#include "fspr_ldap.h"
#include "fspr_ldap_url.h"
#include "fspr_md4.h"
#include "fspr_md5.h"
#include "fspr_optional.h"
#include "fspr_optional_hooks.h"
#include "fspr_queue.h"
#include "fspr_reslist.h"
#include "fspr_rmm.h"
#include "fspr_sdbm.h"
#include "fspr_sha1.h"
#include "fspr_strmatch.h"
#include "fspr_uri.h"
#include "fspr_uuid.h"
#include "fspr_xlate.h"
#include "fspr_xml.h"
#include "apu_version.h"
#include "apu_want.h"

