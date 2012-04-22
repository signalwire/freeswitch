/*
 *  zap_m3ua.h
 *  openzap
 *
 *  Created by Shane Burrell on 4/3/08.
 *  Copyright 2008 Shane Burrell. All rights reserved.
 *
 * Copyright (c) 2007, Anthony Minessale II, Nenad Corbic
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


//#include "m3ua_client.h"
#include "openzap.h"
enum	e_sigboost_event_id_values
{
	SIGBOOST_EVENT_CALL_START			= 0x80, /*128*/
	SIGBOOST_EVENT_CALL_START_ACK			= 0x81, /*129*/
	SIGBOOST_EVENT_CALL_START_NACK			= 0x82, /*130*/
	SIGBOOST_EVENT_CALL_START_NACK_ACK		= 0x83, /*131*/
	SIGBOOST_EVENT_CALL_ANSWERED			= 0x84, /*132*/
	SIGBOOST_EVENT_CALL_STOPPED			= 0x85, /*133*/
	SIGBOOST_EVENT_CALL_STOPPED_ACK			= 0x86, /*134*/
	SIGBOOST_EVENT_SYSTEM_RESTART			= 0x87, /*135*/
	SIGBOOST_EVENT_SYSTEM_RESTART_ACK		= 0x88, /*136*/
	/* Following IDs are ss7boost to sangoma_mgd only. */
	SIGBOOST_EVENT_HEARTBEAT			= 0x89, /*137*/
	SIGBOOST_EVENT_INSERT_CHECK_LOOP		= 0x8a, /*138*/
	SIGBOOST_EVENT_REMOVE_CHECK_LOOP		= 0x8b, /*139*/
	SIGBOOST_EVENT_AUTO_CALL_GAP_ABATE		= 0x8c, /*140*/
};
enum	e_sigboost_release_cause_values
{
	SIGBOOST_RELEASE_CAUSE_UNDEFINED		= 0,
	SIGBOOST_RELEASE_CAUSE_NORMAL			= 16,
	SIGBOOST_RELEASE_CAUSE_BUSY			= 17,
	/* probable elimination */
	//SIGBOOST_RELEASE_CAUSE_BUSY			= 0x91, /* 145 */
	//SIGBOOST_RELEASE_CAUSE_CALLED_NOT_EXIST	= 0x92, /* 146 */
	//SIGBOOST_RELEASE_CAUSE_CIRCUIT_RESET		= 0x93, /* 147 */
	//SIGBOOST_RELEASE_CAUSE_NOANSWER		= 0x94, /* 148 */
};

enum	e_sigboost_call_setup_ack_nack_cause_values
{
	SIGBOOST_CALL_SETUP_NACK_ALL_CKTS_BUSY		= 117, /* unused Q.850 value */
	SIGBOOST_CALL_SETUP_NACK_TEST_CKT_BUSY		= 118, /* unused Q.850 value */
	SIGBOOST_CALL_SETUP_NACK_INVALID_NUMBER		= 28,
	/* probable elimination */
	//SIGBOOST_CALL_SETUP_RESERVED			= 0x00,
	//SIGBOOST_CALL_SETUP_CIRCUIT_RESET		= 0x10,
	//SIGBOOST_CALL_SETUP_NACK_CKT_START_TIMEOUT	= 0x11,
	//SIGBOOST_CALL_SETUP_NACK_AUTO_CALL_GAP	= 0x17,
};
typedef enum {
	M3UA_SPAN_SIGNALING_M3UA,
	M3UA_SPAN_SIGNALING_SS7BOX,
	
} M3UA_TSpanSignaling;
#define M3UA_SPAN_STRINGS "M3UA", "SS7BOX"
ZAP_STR2ENUM_P(m3ua_str2span, m3ua_span2str, M3UA_TSpanSignaling)



typedef enum {
	ZAP_M3UA_RUNNING = (1 << 0)
} zap_m3uat_flag_t;

/*typedef struct m3ua_data {
	m3uac_connection_t mcon;
	m3uac_connection_t pcon;
	zio_signal_cb_t signal_cb;
	uint32_t flags;
} m3ua_data_t;

*/
/*typedef struct mu3a_link {
	ss7bc_connection_t mcon;
	ss7bc_connection_t pcon;
	zio_signal_cb_t signal_cb;
	uint32_t flags;
} zap_m3ua_data_t;
*/

zap_status_t m3ua_init(zap_io_interface_t **zint);
zap_status_t m3ua_destroy(void);
zap_status_t m3ua_start(zap_span_t *span);



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

