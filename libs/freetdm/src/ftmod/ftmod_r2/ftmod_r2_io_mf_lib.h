/*
 * Copyright (c) 2011 Sebastien Trottier
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
 *
 */

#ifndef _FTMOD_R2_IO_MFLIB_H_
#define _FTMOD_R2_IO_MFLIB_H_ 

#include <ftdm_declare.h>

#include <openr2.h>

#if defined(__cplusplus)
extern "C" {
#endif

/* MFC/R2 tone generator handle (mf_write_handle) */
typedef struct {
	/*! FTDM channel performing the MF generation */
	ftdm_channel_t *ftdmchan;
	/*! 1 if generating forward tones, otherwise generating reverse tones. */
	int fwd;
} ftdm_r2_mf_write_handle_t;

/* MF lib interface that generate MF tones via FreeTDM channel IO commands
   MF detection using the default openr2 provider (r2engine) */   
openr2_mflib_interface_t *ftdm_r2_get_native_channel_mf_generation_iface(void);

#if defined(__cplusplus)
} /* endif extern "C" */
#endif

#endif /* endif defined _FTMOD_R2_IO_MFLIB_H_ */

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
