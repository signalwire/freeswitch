/*!
 * MODULE   : mod_bfcp
 *
 * Owners	: GSLab Pvt Ltd
 * 			: www.gslab.com
 * 			: © Copyright 2020 Great Software Laboratory. All rights reserved.
 *
 * The Original Code is mod_bfcp for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * Contributor(s):
 *
 * Aman Thakral <aman.thakral@gslab.com>
 * Vaibhav Sathe <vaibhav.sathe@gslab.com>
 * Pushp Raj <pushp.raj@gslab.com>
 * Vishal Abhishek <vishal.abhishwk@gslab.com>
 *
 * Reviewer(s):
 *
 * Sagar Joshi <sagar.joshi@gslab.com>
 * Prashanth Regalla <prashanth.regalla@gslab.com>
 *
 * mod_bfcp.h -- LIBBFCP ENDPOINT CODE
 *
 */
#include <switch.h>
#include "switch_stun.h"
/* Increment an attribute pointer to the next attribute in it's packet. */
#define _switch_stun_packet_next_attribute(attribute, end) (attribute && (attribute = (switch_stun_packet_attribute_t *) (attribute->value + ntohs(attribute->length))) && ((void *)attribute < end) && ntohs(attribute->length) && ((void *)(attribute + ntohs(attribute->length)) < end))
/* Obtain the padded length of an attribute's value. */
#define _switch_stun_attribute_padded_length(attribute) ((uint16_t)(ntohs(attribute->length) + (sizeof(uint32_t)-1)) & ~sizeof(uint32_t))
#define BFCPBUFLEN 200 /* BFCP SDP length "m=application 51762 UDP/BFCP *" + attributes */

/* Library for integrating libbfcp with Freeswitch */
#include "bfcp_interface.h"

/*! \brief structure for configuration parameters */
struct bfcp_conf_globals {
    char* ip;
    uint16_t max_conf_per_server[2];         /* index 0 : TCP, 1 : UDP */
    uint16_t max_floor_request_per_floor[2]; /* index 0 : TCP, 1 : UDP */
    uint16_t bfcp_transport_tcp;
    uint16_t bfcp_transport_udp;
    uint16_t bfcp_port;
    uint32_t wait_time_chair_action;
    uint16_t max_floor_per_conf;
};

extern struct bfcp_conf_globals bfcp_conf_globals;

typedef switch_status_t (*bfcp_command_t) (bfcp server, int index, int status, switch_stream_handle_t *stream);

/* Pointer to FLOOR CONTROL SERVER (FCS) instance */
struct bfcp_server *bfcp_srv_udp;
struct bfcp_server *bfcp_srv_tcp;

/*!
  \brief Generate LOCAL SDP for BFCP media which is sent to current/peer leg
  \param session Pointer to session of current leg
  \param sdp_type REQUEST or RESPONSE
 */
SWITCH_DECLARE(void) bfcp_media_gen_local_sdp(switch_core_session_t *session,
                                              switch_sdp_type_t sdp_type);

/*!
  \brief Parse and validate BFCP attributes
  \param session Pointer to sesion of current leg
  \return SWITCH_STATUS_SUCCESS : If validation is successful
          SWITCH_STATUS_FALSE : Otherwise
 */
switch_status_t bfcp_parse(switch_core_session_t *session);