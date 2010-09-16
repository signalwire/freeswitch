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
uint32_t sngss7_id;
/******************************************************************************/

/* PROTOTYPES *****************************************************************/
uint8_t copy_tknStr_from_sngss7(TknStr str, char *ftdm, TknU8 oddEven);
uint8_t copy_cgPtyNum_from_sngss7(ftdm_caller_data_t *ftdm, SiCgPtyNum *cgPtyNum);
uint8_t copy_cgPtyNum_to_sngss7(ftdm_caller_data_t *ftdm, SiCgPtyNum *cgPtyNum);
uint8_t copy_cdPtyNum_from_sngss7(ftdm_caller_data_t *ftdm, SiCdPtyNum *cdPtyNum);
uint8_t copy_cdPtyNum_to_sngss7(ftdm_caller_data_t *ftdm, SiCdPtyNum *cdPtyNum);

int check_for_state_change(ftdm_channel_t *ftdmchan);
int check_cics_in_range(sngss7_chan_data_t *sngss7_info);
int check_for_reset(sngss7_chan_data_t *sngss7_info);

unsigned long get_unique_id(void);

ftdm_status_t extract_chan_data(uint32_t circuit, sngss7_chan_data_t **sngss7_info, ftdm_channel_t **ftdmchan);

ftdm_status_t check_if_rx_grs_processed(ftdm_span_t *ftdmspan);
ftdm_status_t check_for_res_sus_flag(ftdm_span_t *ftdmspan);
/******************************************************************************/

/* FUNCTIONS ******************************************************************/
uint8_t copy_cgPtyNum_from_sngss7(ftdm_caller_data_t *ftdm, SiCgPtyNum *cgPtyNum)
{

	return 0;
}

/******************************************************************************/
uint8_t copy_cgPtyNum_to_sngss7(ftdm_caller_data_t *ftdm, SiCgPtyNum *cgPtyNum)
{
	int k;
	int j;
	int flag;
	int odd;
	char tmp[2];
	uint8_t lower;
	uint8_t upper;

	/**************************************************************************/
	cgPtyNum->eh.pres		   = PRSNT_NODEF;
	/**************************************************************************/
	cgPtyNum->natAddrInd.pres   = PRSNT_NODEF;
	cgPtyNum->natAddrInd.val	= 0x03;
	/**************************************************************************/
	cgPtyNum->scrnInd.pres	  = PRSNT_NODEF;
	cgPtyNum->scrnInd.val	   = ftdm->screen;
	/**************************************************************************/
	cgPtyNum->presRest.pres	 = PRSNT_NODEF;
	cgPtyNum->presRest.val	  = ftdm->pres;
	/**************************************************************************/
	cgPtyNum->numPlan.pres	  = PRSNT_NODEF;
	cgPtyNum->numPlan.val	   = 0x01;
	/**************************************************************************/
	cgPtyNum->niInd.pres		= PRSNT_NODEF;
	cgPtyNum->niInd.val		 = 0x00;
	/**************************************************************************/
	cgPtyNum->addrSig.pres	  = PRSNT_NODEF;

	/* atoi will search through memory starting from the pointer it is given until
	 * it finds the \0...since tmp is on the stack it will start going through the
	 * possibly causing corruption.  Hard code a \0 to prevent this
	 */
	tmp[1] = '\0';
	k = 0;
	j = 0;
	flag = 0;
	odd = 0;
	upper = 0x0;
	lower = 0x0;

	while (1) {
		/* grab a digit from the ftdm digits */
		tmp[0] = ftdm->cid_num.digits[k];

		/* check if the digit is a number and that is not null */
		while (!(isdigit(tmp[0])) && (tmp[0] != '\0')) {
			/* move on to the next value */
			k++;
			tmp[0] = ftdm->cid_num.digits[k];
		} /* while(!(isdigit(tmp))) */

		/* check if tmp is null or a digit */
		if (tmp[0] != '\0') {
			/* push it into the lower nibble */
			lower = atoi(&tmp[0]);
			/* move to the next digit */
			k++;
			/* grab a digit from the ftdm digits */
			tmp[0] = ftdm->cid_num.digits[k];

			/* check if the digit is a number and that is not null */
			while (!(isdigit(tmp[0])) && (tmp[0] != '\0')) {
				k++;
				tmp[0] = ftdm->cid_num.digits[k];
			} /* while(!(isdigit(tmp))) */

			/* check if tmp is null or a digit */
			if (tmp[0] != '\0') {
				/* push the digit into the upper nibble */
				upper = (atoi(&tmp[0])) << 4;
			} else {
				/* there is no upper ... fill in 0 */
				upper = 0x0;
				/* throw the odd flag */
				odd = 1;
				/* throw the end flag */
				flag = 1;
			} /* if (tmp != '\0') */
		} else {
			/* keep the odd flag down */
			odd = 0;
			/* throw the flag */
			flag = 1;
		}

		/* push the digits into the trillium structure */
		cgPtyNum->addrSig.val[j] = upper | lower;

		/* increment the trillium pointer */
		j++;

		/* if the flag is up we're through all the digits */
		if (flag) break;

		/* move to the next digit */
		k++;
	} /* while(1) */

	cgPtyNum->addrSig.len = j;

	/**************************************************************************/
	cgPtyNum->oddEven.pres	  = PRSNT_NODEF;
	cgPtyNum->oddEven.val	   = odd;
	/**************************************************************************/
	return 0;
}

/******************************************************************************/
uint8_t copy_cdPtyNum_from_sngss7(ftdm_caller_data_t *ftdm, SiCdPtyNum *cdPtyNum)
{

	return 0;
}

/******************************************************************************/
uint8_t copy_cdPtyNum_to_sngss7(ftdm_caller_data_t *ftdm, SiCdPtyNum *cdPtyNum)
{
	int k;
	int j;
	int flag;
	int odd;
	char tmp[2];
	uint8_t lower;
	uint8_t upper;

	/**************************************************************************/
	cdPtyNum->eh.pres		   = PRSNT_NODEF;
	/**************************************************************************/
	cdPtyNum->natAddrInd.pres   = PRSNT_NODEF;
	cdPtyNum->natAddrInd.val	= 0x03;
	/**************************************************************************/
	cdPtyNum->numPlan.pres	  = PRSNT_NODEF;
	cdPtyNum->numPlan.val	   = 0x01;
	/**************************************************************************/
	cdPtyNum->innInd.pres	   = PRSNT_NODEF;
	cdPtyNum->innInd.val		= 0x01;
	/**************************************************************************/
	cdPtyNum->addrSig.pres	  = PRSNT_NODEF;

	/* atoi will search through memory starting from the pointer it is given until
	 * it finds the \0...since tmp is on the stack it will start going through the
	 * possibly causing corruption.  Hard code a \0 to prevent this
	 */ /* dnis */
	tmp[1] = '\0';
	k = 0;
	j = 0;
	flag = 0;
	odd = 0;
	upper = 0x0;
	lower = 0x0;

	while (1) {
		/* grab a digit from the ftdm digits */
		tmp[0] = ftdm->dnis.digits[k];

		/* check if the digit is a number and that is not null */
		while (!(isdigit(tmp[0])) && (tmp[0] != '\0')) {
			/* move on to the next value */
			k++;
			tmp[0] = ftdm->dnis.digits[k];
		} /* while(!(isdigit(tmp))) */

		/* check if tmp is null or a digit */
		if (tmp[0] != '\0') {
			/* push it into the lower nibble */
			lower = atoi(&tmp[0]);
			/* move to the next digit */
			k++;
			/* grab a digit from the ftdm digits */
			tmp[0] = ftdm->dnis.digits[k];

			/* check if the digit is a number and that is not null */
			while (!(isdigit(tmp[0])) && (tmp[0] != '\0')) {
				k++;
				tmp[0] = ftdm->dnis.digits[k];
			} /* while(!(isdigit(tmp))) */

			/* check if tmp is null or a digit */
			if (tmp[0] != '\0') {
				/* push the digit into the upper nibble */
				upper = (atoi(&tmp[0])) << 4;
			} else {
				/* there is no upper ... fill in ST */
				upper = 0xF;
				/* throw the odd flag */
				odd = 1;
				/* throw the end flag */
				flag = 1;
			} /* if (tmp != '\0') */
		} else {
			/* keep the odd flag down */
			odd = 1;
			/* need to add the ST */
			lower = 0xF;
			upper = 0x0;
			/* throw the flag */
			flag = 1;
		}

		/* push the digits into the trillium structure */
		cdPtyNum->addrSig.val[j] = upper | lower;

		/* increment the trillium pointer */
		j++;

		/* if the flag is up we're through all the digits */
		if (flag) break;

		/* move to the next digit */
		k++;
	} /* while(1) */

	cdPtyNum->addrSig.len = j;

	/**************************************************************************/
	cdPtyNum->oddEven.pres	  = PRSNT_NODEF;

	cdPtyNum->oddEven.val	   = odd;

	/**************************************************************************/
	return 0;
}

/******************************************************************************/
uint8_t copy_tknStr_from_sngss7(TknStr str, char *ftdm, TknU8 oddEven)
{
	uint8_t i;
	uint8_t j;

	/* check if the token string is present */

	if (str.pres == 1) {
		j = 0;

		for (i = 0; i < str.len; i++) {
			sprintf(&ftdm[j], "%X", (str.val[i] & 0x0F));
			j++;
			sprintf(&ftdm[j], "%X", ((str.val[i] & 0xF0) >> 4));
			j++;
		}

		/* if the odd flag is up the last digit is a fake "0" */
		if ((oddEven.pres == 1) && (oddEven.val == 1)) {
			ftdm[j-1] = '\0';
		} else {
			ftdm[j] = '\0';
		}

		
	} else {
		SS7_ERROR("Asked to copy tknStr that is not present!\n");
		return 1;
	}

	return 0;
}

/******************************************************************************/
int check_for_state_change(ftdm_channel_t *ftdmchan)
{

	/* check to see if there are any pending state changes on the channel and give them a sec to happen*/
	ftdm_wait_for_flag_cleared(ftdmchan, FTDM_CHANNEL_STATE_CHANGE, 500);

	/* check the flag to confirm it is clear now */

	if (ftdm_test_flag(ftdmchan, FTDM_CHANNEL_STATE_CHANGE)) {
		/* the flag is still up...so we have a problem */
		SS7_DEBUG_CHAN(ftdmchan, "FTDM_CHANNEL_STATE_CHANGE flag set for over 500ms, channel state = %s\n",
									ftdm_channel_state2str (ftdmchan->state));

		return 1;
	}

	return 0;
}

/******************************************************************************/
int check_cics_in_range(sngss7_chan_data_t *sngss7_info)
{


#if 0
	ftdm_channel_t		*tmp_ftdmchan;
	sngss7_chan_data_t  *tmp_sngss7_info;
	int 				i = 0;
	
	/* check all the circuits in the range to see if we are the last ckt to reset */
	for ( i = sngss7_info->grs.circuit; i < ( sngss7_info->grs.range + 1 ); i++ ) {
		if ( g_ftdm_sngss7_data.cfg.isupCircuit[i].siglink == 0 ) {
		
			/* get the ftdmchan and ss7_chan_data from the circuit */
			if (extract_chan_data(g_ftdm_sngss7_data.cfg.isupCircuit[i].id, &tmp_sngss7_info, &tmp_ftdmchan)) {
				SS7_ERROR("Failed to extract channel data for circuit = %d!\n", g_ftdm_sngss7_data.cfg.isupCircuit[i].id);
				return 0;
			}

			/* check if the channel still has the reset flag done is up */
			if (!sngss7_test_flag(tmp_sngss7_info, FLAG_GRP_RESET_RX_DN)) {
				SS7_DEBUG_CHAN(tmp_ftdmchan, "[CIC:%d] Still processing reset...\n", tmp_sngss7_info->circuit->cic);
				return 0;
			}
		} /* if not siglink */
	} /* for */

	SS7_DEBUG("All circuits out of reset: circuit=%d, range=%d\n",
				sngss7_info->grs.circuit,
				sngss7_info->grs.range);
	return 1;

#endif

	return 0;

}

/******************************************************************************/
ftdm_status_t extract_chan_data(uint32_t circuit, sngss7_chan_data_t **sngss7_info, ftdm_channel_t **ftdmchan)
{
	/*SS7_FUNC_TRACE_ENTER(__FUNCTION__);*/

	if (g_ftdm_sngss7_data.cfg.isupCkt[circuit].obj == NULL) {
		SS7_ERROR("sngss7_info is Null for circuit #%d\n", circuit);
		return FTDM_FAIL;
	}

	ftdm_assert_return(g_ftdm_sngss7_data.cfg.isupCkt[circuit].obj, FTDM_FAIL, "received message on signalling link or non-configured cic\n");

	*sngss7_info = g_ftdm_sngss7_data.cfg.isupCkt[circuit].obj;

	ftdm_assert_return((*sngss7_info)->ftdmchan, FTDM_FAIL, "received message on signalling link or non-configured cic\n");
	*ftdmchan = (*sngss7_info)->ftdmchan;

	/*SS7_FUNC_TRACE_EXIT(__FUNCTION__);*/
	return FTDM_SUCCESS;
}

/******************************************************************************/
int check_for_reset(sngss7_chan_data_t *sngss7_info)
{

	if (sngss7_test_flag(sngss7_info,FLAG_RESET_RX)) {
		return 1;
	}
	
	if (sngss7_test_flag(sngss7_info,FLAG_RESET_TX)) {
		return 1;
	}
	
	if (sngss7_test_flag(sngss7_info,FLAG_GRP_RESET_RX)) {
		return 1;
	}
	
	if (sngss7_test_flag(sngss7_info,FLAG_GRP_RESET_TX)) {
		return 1;
	}

	return 0;
	
}

/******************************************************************************/
unsigned long get_unique_id(void)
{

	if (sngss7_id < 420000000) {
		sngss7_id++;
	} else {
		sngss7_id = 1;
	}

	return(sngss7_id);
}

/******************************************************************************/
ftdm_status_t check_if_rx_grs_processed(ftdm_span_t *ftdmspan)
{
	ftdm_channel_t 		*ftdmchan = NULL;
	sngss7_chan_data_t  *sngss7_info = NULL;
	sngss7_span_data_t	*sngss7_span = (sngss7_span_data_t *)ftdmspan->mod_data;
	int 				i;
	int					byte = 0;
	int					bit = 0;


	ftdm_log(FTDM_LOG_DEBUG, "Found Rx GRS on span %d...checking circuits\n", ftdmspan->span_id);

	/* check all the circuits in the range to see if they are done resetting */
	for ( i = sngss7_span->rx_grs.circuit; i < (sngss7_span->rx_grs.circuit + sngss7_span->rx_grs.range + 1); i++) {

		/* extract the channel in question */
		if (extract_chan_data(i, &sngss7_info, &ftdmchan)) {
			SS7_ERROR("Failed to extract channel data for circuit = %d!\n", i);
			SS7_ASSERT;
		}

		/* lock the channel */
		ftdm_mutex_lock(ftdmchan->mutex);

		/* check if there is a state change pending on the channel */
		if (!ftdm_test_flag(ftdmchan, FTDM_CHANNEL_STATE_CHANGE)) {
			/* check the state to the GRP_RESET_RX_DN flag */
			if (!sngss7_test_flag(sngss7_info, FLAG_GRP_RESET_RX_DN)) {
				/* this channel is still resetting...do nothing */
					goto GRS_UNLOCK_ALL;
				} /* if (!sngss7_test_flag(sngss7_info, FLAG_GRP_RESET_RX_DN)) */
			} else {
				/* state change pending */
				goto GRS_UNLOCK_ALL;
			}
		} /* for ( i = circuit; i < (circuit + range + 1); i++) */

		SS7_DEBUG("All circuits out of reset for GRS: circuit=%d, range=%d\n",
						sngss7_span->rx_grs.circuit,
						sngss7_span->rx_grs.range);

		/* check all the circuits in the range to see if they are done resetting */
		for ( i = sngss7_span->rx_grs.circuit; i < (sngss7_span->rx_grs.circuit + sngss7_span->rx_grs.range + 1); i++) {

			/* extract the channel in question */
			if (extract_chan_data(i, &sngss7_info, &ftdmchan)) {
				SS7_ERROR("Failed to extract channel data for circuit = %d!\n",i);
				SS7_ASSERT;
			}

			/* throw the GRP reset flag complete flag */
			sngss7_set_flag(sngss7_info, FLAG_GRP_RESET_RX_CMPLT);

			/* move the channel to the down state */
			ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DOWN);

			/* update the status map if the ckt is in blocked state */
			if ((sngss7_test_flag(sngss7_info, FLAG_CKT_MN_BLOCK_RX)) ||
				(sngss7_test_flag(sngss7_info, FLAG_CKT_MN_BLOCK_TX)) ||
				(sngss7_test_flag(sngss7_info, FLAG_GRP_MN_BLOCK_RX)) ||
				(sngss7_test_flag(sngss7_info, FLAG_GRP_MN_BLOCK_RX))) {
			
				sngss7_span->rx_grs.status[byte] = (sngss7_span->rx_grs.status[byte] | (1 << bit));
			} /* if blocked */
			
			/* update the bit and byte counter*/
			bit ++;
			if (bit == 8) {
				byte++;
				bit = 0;
			}
		} /* for ( i = circuit; i < (circuit + range + 1); i++) */

GRS_UNLOCK_ALL:
		for ( i = sngss7_span->rx_grs.circuit; i < (sngss7_span->rx_grs.circuit + sngss7_span->rx_grs.range + 1); i++) {
			/* extract the channel in question */
			if (extract_chan_data(i, &sngss7_info, &ftdmchan)) {
				SS7_ERROR("Failed to extract channel data for circuit = %d!\n", i);
				SS7_ASSERT;
			}

			/* unlock the channel */
			ftdm_mutex_unlock(ftdmchan->mutex);
		}

	return FTDM_SUCCESS;
}

/******************************************************************************/
ftdm_status_t check_for_res_sus_flag(ftdm_span_t *ftdmspan)
{
	ftdm_channel_t		*ftdmchan = NULL;
	sngss7_chan_data_t	*sngss7_info = NULL;
	ftdm_sigmsg_t 		sigev;
	int 				x;

	for (x = 1; x < (ftdmspan->chan_count + 1); x++) {

		/* extract the channel structure and sngss7 channel data */
		ftdmchan = ftdmspan->channels[x];
		
		/* if the call data is NULL move on */
		if (ftdmchan->call_data == NULL) continue;

		sngss7_info = ftdmchan->call_data;

		/* lock the channel */
		ftdm_mutex_lock(ftdmchan->mutex);

		memset (&sigev, 0, sizeof (sigev));

		sigev.chan_id = ftdmchan->chan_id;
		sigev.span_id = ftdmchan->span_id;
		sigev.channel = ftdmchan;

		if ((sngss7_test_flag(sngss7_info, FLAG_INFID_PAUSED)) &&
			(ftdm_test_flag(ftdmchan, FTDM_CHANNEL_SIG_UP))) {
			
			/* bring the sig status down */
			sigev.event_id = FTDM_SIGEVENT_SIGSTATUS_CHANGED;
			sigev.sigstatus = FTDM_SIG_STATE_DOWN;
			ftdm_span_send_signal(ftdmchan->span, &sigev);	
		}

		if ((sngss7_test_flag(sngss7_info, FLAG_INFID_RESUME)) &&
			!(ftdm_test_flag(ftdmchan, FTDM_CHANNEL_SIG_UP))) {
			
			/* bring the sig status back up */
			sigev.event_id = FTDM_SIGEVENT_SIGSTATUS_CHANGED;
			sigev.sigstatus = FTDM_SIG_STATE_UP;
			ftdm_span_send_signal(ftdmchan->span, &sigev);

			sngss7_clear_flag(sngss7_info, FLAG_INFID_RESUME);
		}

		/* unlock the channel */
		ftdm_mutex_unlock(ftdmchan->mutex);

	} /* for (x = 1; x < (span->chan_count + 1); x++) */

	/* signal the core that sig events are queued for processing */
	ftdm_span_trigger_signals(ftdmspan);

	return FTDM_SUCCESS;
}

/******************************************************************************/


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
