#ifndef _KS_SSL_H
#define _KS_SSL_H

#include "ks.h"

#include <openssl/ssl.h>
#include <openssl/engine.h>

KS_BEGIN_EXTERN_C

KS_DECLARE(void) ks_ssl_init_ssl_locks(void);
KS_DECLARE(void) ks_ssl_destroy_ssl_locks(void);
KS_DECLARE(int) ks_gen_cert(const char *dir, const char *file);

KS_END_EXTERN_C

#endif /* defined(_KS_SSL_H) */

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
