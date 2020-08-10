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
 * Vishal Abhishek <vishal.abhishek@gslab.com>
 *
 * Reviewer(s):
 *
 * Sagar Joshi <sagar.joshi@gslab.com>
 * Prashanth Regalla <prashanth.regalla@gslab.com>
 *
 * bfcp_media.c -- LIBBFCP ENDPOINT CODE (bfcp code)
 *
 */
#include "mod_bfcp.h"

/*!
  \brief Retrieve media atrributes of BFCP
  \param session Pointer to session of current leg
  \return media attributes of BFCP
 */
static media_attribute_t* bfcp_get_bfcp_attribute(switch_core_session_t *session);

/*!
  \brief Check if all mandatory attributes are present in remote SDP or not
  \param session Pointer to session of current leg
  \param bfcp_media_attributes Pointer of media attributes of BFCP
  \return SWITCH_STATUS_SUCCESS : If all mandatory attributes are present in remote SDP,
          SWITCH_STATUS_FALSE : Otherwise
 */
static switch_status_t bfcp_check_mandatory_attribute(switch_core_session_t *session,
                                                      media_attribute_t *bfcp_media_attribute);

/*!
  \brief Validate BFCP attributes and maintain object of BFCP for current leg
  \param session Pointer to session of current leg
  \param bfcp_media_attribute Pointer to media attributes of BFCP
  \param bfcp_interface_object Pointer to object where we maintain BFCP related data for current leg
  \return SWITCH_STATUS_SUCCESS : If validation of BFCP attributes is successful
          SWITCH_STATUS_FALSE : Otherwise
 */
static switch_status_t bfcp_validate_sdp(switch_core_session_t *session,
                                         media_attribute_t *bfcp_media_attribute,
                                         bfcp_interface bfcp_interface_obj);

/*!
  \brief To validate userID of BFCP and store it in BFCP object
  \param user_id Pointer to BFCP userID buffer
  \return SWITCH_STATUS_SUCCESS : If BFCP userID validated successfully
          SWITCH_STATUS_FALSE : Otherwise
 */
static switch_status_t bfcp_validate_userid(char *user_id);

/*!
  \brief To validate conferenceID of BFCP and store it in BFCP object
  \param conf_id Pointer to BFCP confID buffer
  \return SWITCH_STATUS_SUCCESS : If BFCP conferenceID validated successfully
          SWITCH_STATUS_FALSE : Otherwise
 */
static switch_status_t bfcp_validate_confid(char *conf_id);

/*!
  \brief To validate BFCP floor control and store it in BFCP object
  \param floor_ctrl Pointer to BFCP Floor Control buffer
  \param bfcp_interface_object Pointer to object where we maintain BFCP related data for current leg
  \return SWITCH_STATUS_SUCCESS : If floor control is of type C-only or C-S
          SWITCH_STATUS_FALSE : Otherwise
 */
static switch_status_t bfcp_validate_floorctrl(char *floor_ctrl,
                                               bfcp_interface bfcp_interface_object);

/*!
  \brief To validate BFCP floorID & Storing it in BFCP object
  \param floor_id Pointer to BFCP floorID buffer
  \param bfcp_interface_object Pointer to object where we maintain BFCP related data for current leg
  \return SWITCH_STATUS_SUCCESS : If floorID validated successfully
          SWITCH_STATUS_FALSE : Otherwise
 */
static switch_status_t bfcp_validate_floorid(char *floor_id,
                                             bfcp_interface bfcp_interface_object);

/*!
  \brief To validate BFCP setup and storing it in BFCP Object
  \param setup Pointer to BFCP setup buffer
  \param bfcp_interface_object Pointer to object where we maintain BFCP related data for current leg
  \return SWITCH_STATUS_SUCCESS : If setup is active or actpass
          SWITCH_STATUS_FALSE : Otherwise
 */
static switch_status_t bfcp_validate_setup(char *setup,
                                           bfcp_interface bfcp_interface_object);

/*!
  \brief To validate BFCP connection and storing it in BFCP Object
  \param connection Pointer to BFCP connection buffer
  \return SWITCH_STATUS_SUCCESS : If connection is new or existing
          SWITCH_STATUS_FALSE : Otherwise
 */
static switch_status_t bfcp_validate_connection(char *connection);

/* Parsing tokens */
#define SPACE " "
#define TAB   "\011"
#define ALPHA "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define DIGIT "0123456789"
#define TOKEN ALPHA "-!#$%&'*+.^_`{|}~"

static char *token(char **message,
		           const char *sep,
		           const char *legal,
		           const char *strip)
{
    size_t n;
    char *retval = *message;

    if (strip) {
        retval += strspn(retval, strip);
    }

    if (legal) {
        n = strspn(retval, legal);
    } else {
        n = strcspn(retval, sep);
    }

    if (n == 0) {
        return NULL;
    }

    if (retval[n]) {
        retval[n++] = '\0';
        n += strspn(retval + n, sep);
    }

    *message = retval + n;
    if (*retval == '\0') {
        return NULL;
    }

    return retval;
}

/* To get media attributes of BFCP */
static media_attribute_t* bfcp_get_bfcp_attribute(switch_core_session_t *session)
{
    return switch_core_media_get_media_attribute(session, SWITCH_MEDIA_TYPE_APPLICATION);
}

/* To parse and validate BFCP attributes */
switch_status_t bfcp_parse(switch_core_session_t *session)
{
    switch_channel_t *channel;
    media_attribute_t *bfcp_media_attribute;
    bfcp_interface bfcp_interface_obj;
    media_proto_name_t *media_proto;

    channel = switch_core_session_get_channel(session);

    media_proto = switch_core_media_get_media_protocol(session, SWITCH_MEDIA_TYPE_APPLICATION);

    if ((!strcmp(media_proto, "UDP/BFCP"))) {
        if (!udp_server) {
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Sorry UDP socket for BFCP not up!\n");
            switch_channel_clear_flag(channel, CF_BFCP);
            return SWITCH_STATUS_FALSE;
        }
    } else if (!strcmp(media_proto, "TCP/BFCP")) {
        if (!tcp_server || bfcp_conf_globals.bfcp_transport_tcp != BFCP_OVER_TCP) {
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Sorry TCP socket for BFCP not up!\n");
            switch_channel_clear_flag(channel, CF_BFCP);
            return SWITCH_STATUS_FALSE;
        }
    } else if (!strcmp(media_proto, "TCP/TLS/BFCP")) {
        if (!tcp_server || bfcp_conf_globals.bfcp_transport_tcp != BFCP_OVER_TLS) {
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Sorry TCP/TLS socket for BFCP not up!\n");
            switch_channel_clear_flag(channel, CF_BFCP);
            return SWITCH_STATUS_FALSE;
        }
    } else {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Not a BFCP media protocol!\n");
        switch_channel_clear_flag(channel, CF_BFCP);
        return SWITCH_STATUS_FALSE;
    }

    if (!(bfcp_interface_obj = (bfcp_interface ) switch_core_session_get_private_class(session, SWITCH_PVT_TERTIARY)) &&
        !(bfcp_interface_obj = create_bfcp_interface(switch_core_session_get_uuid(session)))) {
        return SWITCH_STATUS_FALSE;
    }

    mod_bfcp_mutex_lock(&(bfcp_interface_obj->bfcp_count_mutex));

    bfcp_media_attribute = bfcp_get_bfcp_attribute(session);

    if (bfcp_validate_sdp(session, bfcp_media_attribute, bfcp_interface_obj) != SWITCH_STATUS_SUCCESS) {
        switch_channel_t *channel = switch_core_session_get_channel(session);

        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "One or more BFCP attributes are not in proper format.\n");

        if (switch_channel_test_flag(channel, CF_BFCP)) {
            switch_channel_clear_flag(channel, CF_BFCP);
        }

        mod_bfcp_mutex_unlock(&(bfcp_interface_obj->bfcp_count_mutex));
        bfcp_interface_destroy_interface(bfcp_interface_obj);
        return SWITCH_STATUS_FALSE;
    }

    if (bfcp_check_mandatory_attribute(session, bfcp_media_attribute) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "All mandatory attributes of BFCP are not present in SDP.\n");
    }

    mod_bfcp_mutex_unlock(&(bfcp_interface_obj->bfcp_count_mutex));
    switch_core_session_set_private_class(session, bfcp_interface_obj, SWITCH_PVT_TERTIARY);

    return SWITCH_STATUS_SUCCESS;
}

/* Validates the SDP parametrs of BFCP and stores atrribute values in BFCPInterface Object */
static switch_status_t bfcp_validate_sdp(switch_core_session_t *session,
                                         media_attribute_t *bfcp_media_attribute,
                                         bfcp_interface bfcp_interface_object)
{
    media_attribute_t *bfcp_attribute = bfcp_media_attribute;
    switch_core_session_t *peer_session;
    switch_status_t status = SWITCH_STATUS_SUCCESS;

    while (bfcp_attribute) {
        if (!strcmp(bfcp_attribute->a_name, "userid")) {
            status = bfcp_validate_userid(bfcp_attribute->a_value);

        } else if (!strcmp(bfcp_attribute->a_name, "confid")) {
            status = bfcp_validate_confid(bfcp_attribute->a_value);

        } else if (!strcmp(bfcp_attribute->a_name, "floorctrl")) {
            status = bfcp_validate_floorctrl(bfcp_attribute->a_value, bfcp_interface_object);

        } else if (!strcmp(bfcp_attribute->a_name, "floorid")) {
            status = bfcp_validate_floorid(bfcp_attribute->a_value, bfcp_interface_object);

        } else if (!strcmp(bfcp_attribute->a_name, "setup")) {
            status = bfcp_validate_setup(bfcp_attribute->a_value, bfcp_interface_object);

        } else if (!strcmp(bfcp_attribute->a_name, "connection")) {
            status = bfcp_validate_connection(bfcp_attribute->a_value);
        }

        if (status != SWITCH_STATUS_SUCCESS)
            return status;

        bfcp_attribute = bfcp_attribute->a_next;
    }

    if (bfcp_interface_get_floor_stream_mapping(bfcp_interface_object) == NULL) {
        bfcp_interface_set_floor_stream_mapping(bfcp_interface_object, CLIENT_FLOOR_ID, STREAM_ID);
        bfcp_interface_set_media_stream_str(bfcp_interface_object, "mstrm");
    }

    if ((bfcp_interface_get_conf_id(bfcp_interface_object) == 0) || (bfcp_interface_get_user_id(bfcp_interface_object) == 0)) {
        switch_core_session_get_partner(session, &peer_session);

        if (peer_session) {
            switch_channel_t *peer_channel = switch_core_session_get_channel(peer_session);

            if (bfcp_interface_get_user_id(bfcp_interface_object) == 0) {
                if (switch_channel_get_variable(peer_channel, SWITCH_PEER_BFCP_USERID)) {
                    bfcp_interface_set_user_id(bfcp_interface_object, atoi(switch_channel_get_variable(peer_channel, SWITCH_PEER_BFCP_USERID)));
                    switch_channel_set_variable(peer_channel, SWITCH_PEER_BFCP_USERID, NULL);
                } else {
                    bfcp_interface_set_user_id(bfcp_interface_object, bfcp_get_user_id());
                }
            }

            if (bfcp_interface_get_conf_id(bfcp_interface_object) == 0) {
                bfcp_interface peer_bfcp_object = switch_core_session_get_private_class(peer_session, SWITCH_PVT_TERTIARY);

                if (peer_bfcp_object) {
                    bfcp_interface_set_conf_id(bfcp_interface_object, bfcp_interface_get_conf_id(peer_bfcp_object));
                } else {
                    bfcp_interface_set_conf_id(bfcp_interface_object, bfcp_get_conf_id());
                }
            }

            switch_core_session_rwunlock(peer_session);
        } else {
            if (bfcp_interface_get_user_id(bfcp_interface_object) == 0) {
                bfcp_interface_set_user_id(bfcp_interface_object, bfcp_get_user_id());
            }

            if (bfcp_interface_get_conf_id(bfcp_interface_object) == 0) {
                bfcp_interface_set_conf_id(bfcp_interface_object, bfcp_get_conf_id());
            }
        }
    }

    return status;
}

/* Check if all mandatory attributes are present or not */
static switch_status_t bfcp_check_mandatory_attribute(switch_core_session_t *session,
                                                      media_attribute_t *bfcp_media_attribute)
{
       uint16_t is_attribute[6] = {0};
       media_attribute_t *bfcp_attribute = bfcp_media_attribute;

       while (bfcp_attribute) {
           if (!strcmp(bfcp_attribute->a_name, "userid")) {
               is_attribute[0]++;
           } else if (!strcmp(bfcp_attribute->a_name, "confid")) {
               is_attribute[1]++;
           } else if (!strcmp(bfcp_attribute->a_name, "floorctrl")) {
               is_attribute[2]++;
           } else if (!strcmp(bfcp_attribute->a_name, "floorid")) {
               is_attribute[3]++;
           } else if (!strcmp(bfcp_attribute->a_name, "setup")) {
               is_attribute[4]++;
           } else if (!strcmp(bfcp_attribute->a_name, "connection")) {
               is_attribute[5]++;
           }

           bfcp_attribute = bfcp_attribute->a_next;
       }

       if (!is_attribute[2]) {
           switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Attribute 'floorctrl' missing, then by default offerer act as client\n");

           bfcp_attribute = (media_attribute_t*) malloc(sizeof(media_attribute_t));

           if (!bfcp_attribute) {
               return SWITCH_STATUS_FALSE;
           } 

           bfcp_attribute->a_size = sizeof (media_attribute_t);
		   bfcp_attribute->a_name = "floorctrl";
		   bfcp_attribute->a_value = "c-only";
		   bfcp_attribute->a_next = bfcp_media_attribute;
           bfcp_media_attribute = bfcp_attribute;
           is_attribute[2]++;

           switch_core_media_set_media_attribute(session, bfcp_media_attribute, SWITCH_MEDIA_TYPE_APPLICATION);
       }

       for (uint16_t check_attribute = 0; check_attribute < 6; check_attribute++) {
           if (!is_attribute[check_attribute]) {
               return SWITCH_STATUS_FALSE;
           }
       }

       return SWITCH_STATUS_SUCCESS;
}

/* Generates LOCAL SDP for BFCP media which is sent to current/peer leg */
SWITCH_DECLARE(void) bfcp_media_gen_local_sdp(switch_core_session_t *session,
                                              switch_sdp_type_t sdp_type)
{
    switch_core_session_t *peer_session;
    switch_channel_t *channel, *peer_channel;
    payload_map_t *pmap;
    char *buf;
    bfcp_interface bfcp_interface_object;
    media_proto_name_t *media_proto;

    channel = switch_core_session_get_channel(session);
    pmap = switch_core_media_get_payload_map(session, SWITCH_MEDIA_TYPE_APPLICATION);
    media_proto = switch_core_media_get_media_protocol(session, SWITCH_MEDIA_TYPE_APPLICATION);

    switch_zmalloc(buf, BFCPBUFLEN);

    switch_core_session_get_partner(session, &peer_session);

    bfcp_interface_object = (bfcp_interface) switch_core_session_get_private_class(session, SWITCH_PVT_TERTIARY);

    mod_bfcp_mutex_lock(&(bfcp_interface_object->bfcp_count_mutex));

    bfcp_interface_object->m_client_port = pmap->remote_sdp_port;
    bfcp_interface_object->m_client_address = strdup((char*) pmap->remote_sdp_ip);

    if (!strcmp(media_proto, "UDP/BFCP")) {
        bfcp_interface_object->m_transport = 2;
    } else if (!strcmp(media_proto, "TCP/BFCP")) {
        bfcp_interface_object->m_transport = 1;
    } else {
        bfcp_interface_object->m_transport = 0;
    }

    switch_snprintf(buf + strlen(buf), BFCPBUFLEN - strlen(buf), "m=application %d %s *\r\n", bfcp_conf_globals.bfcp_port, media_proto);

    /* LOCAL SDP generation for BFCP media */
    switch_snprintf(buf + strlen(buf), BFCPBUFLEN - strlen(buf), "a=floorctrl:s-only\r\n");
    switch_snprintf(buf + strlen(buf), BFCPBUFLEN - strlen(buf), "a=confid:%d\r\n", bfcp_interface_get_conf_id(bfcp_interface_object));

    /* For userid to be set in LOCAL SDP of BFCP as per request/response to be send */
    if (!switch_channel_test_flag(channel, CF_EARLY_MEDIA_BFCP)) {
        if (peer_session) {
            bfcp_interface peer_bfcp_interface = (bfcp_interface) switch_core_session_get_private_class(peer_session, SWITCH_PVT_TERTIARY);

            if (peer_bfcp_interface) {
                switch_snprintf(buf + strlen(buf), BFCPBUFLEN - strlen(buf), "a=userid:%d\r\n", bfcp_interface_get_user_id(peer_bfcp_interface));
            } else {
                char userid_buf[USER_BUF_SIZE];
                uint16_t user_id = bfcp_get_user_id();
                switch_snprintf(buf + strlen(buf), BFCPBUFLEN - strlen(buf), "a=userid:%d\r\n", user_id);
                sprintf(userid_buf, "%d", user_id);
                switch_channel_set_variable(channel, SWITCH_PEER_BFCP_USERID, userid_buf);
            }
        } else {
            char userid_buf[USER_BUF_SIZE];
            uint16_t user_id = bfcp_get_user_id();
            switch_snprintf(buf + strlen(buf), BFCPBUFLEN - strlen(buf), "a=userid:%d\r\n", user_id);
            sprintf(userid_buf, "%d", user_id);
            switch_channel_set_variable(channel, SWITCH_PEER_BFCP_USERID, userid_buf);
        }
    } else {
        switch_snprintf(buf + strlen(buf), BFCPBUFLEN - strlen(buf), "a=userid:%d\r\n", bfcp_interface_get_user_id(bfcp_interface_object));
    }

    for (floor_stream_mapping_t *floor_stream_map = bfcp_interface_get_floor_stream_mapping(bfcp_interface_object);
         floor_stream_map;
         floor_stream_map = floor_stream_map->next) {
        switch_snprintf(buf + strlen(buf), BFCPBUFLEN - strlen(buf), "a=floorid:%d %s:%d\r\n", floor_stream_map->m_floor_id, bfcp_interface_get_media_stream_str(bfcp_interface_object), floor_stream_map->m_stream_id);
    }

    switch_snprintf(buf + strlen(buf), BFCPBUFLEN - strlen(buf), "a=setup:passive\r\n");
    switch_snprintf(buf + strlen(buf), BFCPBUFLEN - strlen(buf), "a=connection:new\r\n");
    switch_snprintf(buf + strlen(buf), BFCPBUFLEN - strlen(buf), "a=bfcpver:%d\r\n", BFCP_VERSION);

    switch_channel_set_variable(channel, SWITCH_BFCP_LOCAL_SDP, buf);

	if (peer_session) {
        peer_channel = switch_core_session_get_channel(peer_session);

        if (sdp_type == SDP_TYPE_REQUEST) {
            switch_channel_set_flag(peer_channel, CF_PEER_BFCP);
			switch_channel_set_variable(peer_channel, SWITCH_PEER_BFCP_LOCAL_SDP, buf);
        }

        /* Set channel flag and variable in peer_channel only if peer_channel supports BFCP media */
        if (switch_channel_test_flag(channel, CF_PEER_BFCP)) {
            switch_channel_set_flag(peer_channel, CF_PEER_BFCP);
            switch_channel_set_variable(peer_channel, SWITCH_PEER_BFCP_LOCAL_SDP, buf);
        }

        switch_core_session_rwunlock(peer_session);
	}

    mod_bfcp_mutex_unlock(&(bfcp_interface_object->bfcp_count_mutex));

    switch_safe_free(buf);
}

/* To validate userID of BFCP */
static switch_status_t bfcp_validate_userid(char *user_id)
{
    unsigned long user;
    char *value;

    user = strtoul(user_id, &value, 10);
    if (!user || strcmp(value, "")) {
        return SWITCH_STATUS_FALSE;
    }

    return SWITCH_STATUS_SUCCESS;
}

/* To validate conferenceID of BFCP */
static switch_status_t bfcp_validate_confid(char *conf_id)
{
    unsigned long conference;
    char *value;

    conference = strtoul(conf_id, &value, 10);
    if (!conference || strcmp(value, "")) {
        return SWITCH_STATUS_FALSE;
    }

    return SWITCH_STATUS_SUCCESS;
}

/* To validate BFCP floor control */
static switch_status_t bfcp_validate_floorctrl(char *floor_ctrl,
                                               bfcp_interface bfcp_interface_object)
{
    if (!strcmp(floor_ctrl, "c-only")) {
        bfcp_interface_set_floorctrl_mode(bfcp_interface_object, FLOOCTRL_MODE_CLIENT);
    } else if (!strcmp(floor_ctrl, "c-s")) {
        bfcp_interface_set_floorctrl_mode(bfcp_interface_object, FLOOCTRL_MODE_CLIENT_AND_SERVER);
    } else {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, " %s : Invalid value of Floor_ctrl\n", floor_ctrl);
        return SWITCH_STATUS_FALSE;
    }

    return SWITCH_STATUS_SUCCESS;
}

/* To validate BFCP floorID */
static switch_status_t bfcp_validate_floorid(char *floor_id,
                                             bfcp_interface bfcp_interface_object)
{
    unsigned long floor, stream_id;
    char *value, *mstrm;

    floor = strtoul(floor_id, &value, 10);
    if (!floor) {
        return SWITCH_STATUS_FALSE;
    }

    value++;

    mstrm = token(&value, SPACE, TOKEN, ":");
    if (!mstrm && strcmp(mstrm, "m-stream") && strcmp(mstrm, "mstrm")) {
        return SWITCH_STATUS_FALSE;
    }

    stream_id = strtoul(value, &value, 10);
    if (!stream_id || strcmp(value, "")) {
        return SWITCH_STATUS_FALSE;
    }

    bfcp_interface_set_floor_stream_mapping(bfcp_interface_object, floor, stream_id);
    bfcp_interface_set_media_stream_str(bfcp_interface_object, mstrm);

    return SWITCH_STATUS_SUCCESS;
}

/* To validate BFCP setup */
static switch_status_t bfcp_validate_setup(char *setup,
                                           bfcp_interface bfcp_interface_object)
{
    if (!strcmp(setup, "active") || !strcmp(setup, "actpass")) {
        bfcp_interface_set_is_passive(bfcp_interface_object, false);
        return SWITCH_STATUS_SUCCESS;
    } else {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Invalid setup mode.\n");
        return SWITCH_STATUS_FALSE;
    }
}

/* To validate BFCP connection */
static switch_status_t bfcp_validate_connection(char *connection)
{
    if (strcmp(connection, "new") && strcmp(connection, "existing")) {
        return SWITCH_STATUS_FALSE;
    }

    return SWITCH_STATUS_SUCCESS;
}