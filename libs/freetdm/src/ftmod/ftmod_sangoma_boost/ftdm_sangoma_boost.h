/*
 * Copyright (c) 2007, Anthony Minessale II
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FTDM_SANGOMA_BOOST_H
#define FTDM_SANGOMA_BOOST_H
#include "sangoma_boost_client.h"
#include "freetdm.h"

#define MAX_CHANS_PER_TRUNKGROUP 1024

typedef enum {
	FTDM_SANGOMA_BOOST_RUNNING = (1 << 0),
	FTDM_SANGOMA_BOOST_RESTARTING = (1 << 1)
} ftdm_sangoma_boost_flag_t;

typedef struct ftdm_sangoma_boost_data {
	sangomabc_connection_t mcon;
	sangomabc_connection_t pcon;
	int iteration;
	uint32_t flags;
	boost_sigmod_interface_t *sigmod;
	ftdm_queue_t *boost_queue;	
} ftdm_sangoma_boost_data_t;

typedef struct ftdm_sangoma_boost_trunkgroup {
	ftdm_mutex_t *mutex;
	ftdm_size_t size;			/* Number of b-channels in group */	
	unsigned int last_used_index; /* index of last b-channel used */
	ftdm_channel_t* ftdmchans[MAX_CHANS_PER_TRUNKGROUP];
	//DAVIDY need to merge congestion timeouts to this struct
} ftdm_sangoma_boost_trunkgroup_t;
#endif

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */

