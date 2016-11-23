/* 
 * Cross Platform random/uuid abstraction
 * Copyright(C) 2015 Michael Jerris
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so.
 *
 * This work is provided under this license on an "as is" basis, without warranty of any kind,
 * either expressed or implied, including, without limitation, warranties that the covered code
 * is free of defects, merchantable, fit for a particular purpose or non-infringing. The entire
 * risk as to the quality and performance of the covered code is with you. Should any covered
 * code prove defective in any respect, you (not the initial developer or any other contributor)
 * assume the cost of any necessary servicing, repair or correction. This disclaimer of warranty
 * constitutes an essential part of this license. No use of any covered code is authorized hereunder
 * except under this disclaimer. 
 *
 */

#include "ks.h"

#ifndef _KS_RNG_H
#define _KS_RNG_H

KS_BEGIN_EXTERN_C

#ifdef WIN32
#include <Rpc.h>
typedef UUID uuid_t;
#else
#include <uuid/uuid.h>
#endif

KS_DECLARE(uuid_t *) ks_uuid(uuid_t *uuid);
KS_DECLARE(char *) ks_uuid_str(ks_pool_t *pool, uuid_t *uuid);
KS_DECLARE(ks_status_t) ks_rng_init(void);
KS_DECLARE(ks_status_t) ks_rng_shutdown(void);
KS_DECLARE(size_t) ks_rng_get_data(uint8_t* buffer, size_t length);
KS_DECLARE(size_t) ks_rng_add_entropy(const uint8_t *buffer, size_t length);
KS_DECLARE(size_t) ks_rng_seed_data(uint8_t *seed, size_t length);

KS_END_EXTERN_C

#endif

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

