/*
 * Copyright (c) 2009, Konrad Hammel <konrad@sangoma.com>
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

/* INCLUDE ********************************************************************/
#include "ftmod_sangoma_ss7_main.h"
/******************************************************************************/

/* DEFINES ********************************************************************/
/******************************************************************************/

/* GLOBALS ********************************************************************/

/******************************************************************************/

/* PROTOTYPES *****************************************************************/
void handle_isup_t35(void *userdata);
/******************************************************************************/

/* FUNCTIONS ******************************************************************/
void handle_isup_t35(void *userdata)
{
    SS7_FUNC_TRACE_ENTER(__FUNCTION__);

    sngss7_timer_data_t *timer = userdata;
    sngss7_chan_data_t  *sngss7_info = timer->sngss7_info;
    ftdm_channel_t      *ftdmchan = sngss7_info->ftdmchan;

    /* now that we have the right channel...put a lock on it so no-one else can use it */
    ftdm_mutex_lock(ftdmchan->mutex);

    SS7_ERROR("[Call-Control] Timer 35 expired on CIC = %d\n", sngss7_info->circuit->cic);

    /* set the flag to indicate this hangup is started from the local side */
    sngss7_set_flag(sngss7_info, FLAG_LOCAL_REL);

    /* hang up on timer expiry */
    ftdmchan->caller_data.hangup_cause = 102;

    /* end the call */
    ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_CANCEL);

    /*unlock*/
    ftdm_mutex_unlock(ftdmchan->mutex);

    SS7_FUNC_TRACE_EXIT(__FUNCTION__);
    return;
}
/******************************************************************************/
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
/******************************************************************************/

