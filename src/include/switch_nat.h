/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthm@freeswitch.org>
 * Brian K. West <brian@freeswitch.org>
 *
 *
 * switch_nat.h NAT Traversal via NAT-PMP or uPNP
 *
 */
/*!
  \defgroup nat1 NAT code
  \ingroup core1
  \{
*/
#ifndef FREESWITCH_NAT_H
#define FREESWITCH_NAT_H

SWITCH_BEGIN_EXTERN_C typedef enum {
	SWITCH_NAT_TYPE_NONE,
	SWITCH_NAT_TYPE_PMP,
	SWITCH_NAT_TYPE_UPNP
} switch_nat_type_t;

typedef enum {
	SWITCH_NAT_UDP,
	SWITCH_NAT_TCP
} switch_nat_ip_proto_t;

SWITCH_DECLARE(const char *) switch_nat_get_type(void);

/*! 
  \brief Initilize the NAT Traversal System
  \param pool the memory pool to use for long term allocations
  \note Generally called by the core_init
*/
SWITCH_DECLARE(void) switch_nat_init(switch_memory_pool_t *pool, switch_bool_t mapping);

/*! 
  \brief Initilize the rest of the NAT Traversal System
  \note nat_init is called prior to some other modules being loaded.  
        This method allows us to init the rest of the NAT system.
*/
SWITCH_DECLARE(void) switch_nat_late_init(void);

/*!
 \brief Shuts down the NAT Traversal System
*/
SWITCH_DECLARE(void) switch_nat_shutdown(void);

/*!
 \brief Returns a list of nat mappings and other status info
 \note  caller must free the string
*/
SWITCH_DECLARE(char *) switch_nat_status(void);

/*!
 \brief Republishes the nap mappings
 */
SWITCH_DECLARE(void) switch_nat_republish(void);

/*!
 \brief re-initializes NAT subsystem
*/
SWITCH_DECLARE(void) switch_nat_reinit(void);

/*!
 \brief Update the setting if port mapping will be created
*/
SWITCH_DECLARE(void) switch_nat_set_mapping(switch_bool_t mapping);

/*!
 \brief Maps a port through the NAT Traversal System
 \param port Internal port to map
 \param proto Protocol
 \param external_port [out] Mapped external port 
 \param sticky make the mapping permanent
*/
SWITCH_DECLARE(switch_status_t) switch_nat_add_mapping(switch_port_t port, switch_nat_ip_proto_t proto, switch_port_t *external_port,
													   switch_bool_t sticky);
/*!
 \brief Has the NAT subsystem been initialized
*/
SWITCH_DECLARE(switch_bool_t) switch_nat_is_initialized(void);
/*!
 \brief Deletes a NAT mapping
 \param proto Protocol
*/
SWITCH_DECLARE(switch_status_t) switch_nat_del_mapping(switch_port_t port, switch_nat_ip_proto_t proto);


SWITCH_END_EXTERN_C
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
