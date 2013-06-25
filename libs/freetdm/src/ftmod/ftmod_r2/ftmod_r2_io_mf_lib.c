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

#include <freetdm.h>
#include <private/ftdm_core.h>

#include <openr2.h>

#include "ftmod_r2_io_mf_lib.h"

/* Convert openr2 MF tone enum value to FreeTDM MF tone value 
    1-15 bitwise OR FTDM_MF_DIRECTION_FORWARD/BACKWARD
    0 (stop playing)
   openr2_mf_tone_t defined in r2proto.h
*/
static int ftdm_r2_openr2_mf_tone_to_ftdm_mf_tone(openr2_mf_tone_t 
    openr2_tone_value, int forward_signals) 
{
	int tone;

	switch (openr2_tone_value) {
	case 0: return 0;
#define TONE_FROM_NAME(name) case OR2_MF_TONE_##name: tone = name; break;
	TONE_FROM_NAME(1)
	TONE_FROM_NAME(2)
	TONE_FROM_NAME(3)
	TONE_FROM_NAME(4)
	TONE_FROM_NAME(5)
	TONE_FROM_NAME(6)
	TONE_FROM_NAME(7)
	TONE_FROM_NAME(8)
	TONE_FROM_NAME(9)
	TONE_FROM_NAME(10)
	TONE_FROM_NAME(11)
	TONE_FROM_NAME(12)
	TONE_FROM_NAME(13)
	TONE_FROM_NAME(14)
	TONE_FROM_NAME(15)
#undef TONE_FROM_NAME
	default:
		ftdm_assert(0, "Invalid openr2_tone_value\n");
		return -1;
	}

	/* Add flag corresponding to direction */
	if (forward_signals) {
		tone |= FTDM_MF_DIRECTION_FORWARD;
	} else {
		tone |= FTDM_MF_DIRECTION_BACKWARD;
	}

	return tone;
}

/* MF generation routines (using IO command of a FreeTDM channel)
   write_init stores the direction of the MF to generate */
static void *ftdm_r2_io_mf_write_init(ftdm_r2_mf_write_handle_t *handle, int forward_signals)
{
	ftdm_log_chan(handle->ftdmchan, FTDM_LOG_DEBUG, "ftdm_r2_io_mf_write_init, "
	"forward = %d\n", forward_signals);

	handle->fwd = forward_signals;
	return handle;
}

static int ftdm_r2_io_mf_generate_tone(ftdm_r2_mf_write_handle_t *handle, int16_t buffer[], int samples)
{
	/* Our mf_want_generate implementation always return 0, so mf_generate_tone should never be called */
	ftdm_assert(0, "ftdm_r2_io_mf_generate_tone not implemented\n");
	return 0;
}

/* \brief mf_select_tone starts tone generation or stops current tone
 * \return 0 on success, -1 on error 
 */
static int ftdm_r2_io_mf_select_tone(ftdm_r2_mf_write_handle_t *handle, char signal)
{
	int tone; /*  (0, 1-15) (0 meaning to stop playing) */

	ftdm_log_chan(handle->ftdmchan, FTDM_LOG_DEBUG, "ftdm_r2_io_mf_select_tone, "
			"signal = %c\n", signal);

	if (-1 == (tone = ftdm_r2_openr2_mf_tone_to_ftdm_mf_tone(signal, handle->fwd))) {
		return -1;
	}

	/* Start/stop playback directly here, as select tone is called each time a tone 
	   is started or stopped (called if tone changes, but silence is tone 0, 
	   triggering a tone change) */
	if (tone > 0) {
		ftdm_channel_command(handle->ftdmchan, FTDM_COMMAND_START_MF_PLAYBACK, &tone);
	} else {
		/* tone 0 means to stop current tone */
		ftdm_channel_command(handle->ftdmchan, FTDM_COMMAND_STOP_MF_PLAYBACK, NULL);
	}
	return 0;
}

static int ftdm_r2_io_mf_want_generate(ftdm_r2_mf_write_handle_t *handle, int signal)
{
	/* Return 0, meaning mf_generate_tone doesn't need to be called */
	return 0;
}

/* MF lib interface that generate MF tones via FreeTDM channel IO commands
   MF detection using the default openr2 provider (r2engine) */
static openr2_mflib_interface_t g_mf_ftdm_io_iface = {
	/* .mf_read_init */ (openr2_mf_read_init_func)openr2_mf_rx_init,
	/* .mf_write_init */ (openr2_mf_write_init_func)ftdm_r2_io_mf_write_init,
	/* .mf_detect_tone */ (openr2_mf_detect_tone_func)openr2_mf_rx,
	/* .mf_generate_tone */ (openr2_mf_generate_tone_func)ftdm_r2_io_mf_generate_tone,
	/* .mf_select_tone */ (openr2_mf_select_tone_func)ftdm_r2_io_mf_select_tone,
	/* .mf_want_generate */ (openr2_mf_want_generate_func)ftdm_r2_io_mf_want_generate,
	/* .mf_read_dispose */ NULL,
	/* .mf_write_dispose */ NULL
};

openr2_mflib_interface_t *ftdm_r2_get_native_channel_mf_generation_iface()
{
	return &g_mf_ftdm_io_iface;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet
 */
