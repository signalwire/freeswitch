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
 * Vishal Abhishek <vishal.abhishek@gslab.com>
 *
 * Reviewer(s):
 *
 * Sagar Joshi <sagar.joshi@gslab.com>
 * Prashanth Regalla <prashanth.regalla@gslab.com>
 *
 * mod_bfcp.c -- LIBBFCP ENDPOINT CODE
 *
 */
#include "mod_bfcp.h"

struct bfcp_conf_globals bfcp_conf_globals;

/* Prototypes */
SWITCH_MODULE_LOAD_FUNCTION(mod_bfcp_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_bfcp_shutdown);
SWITCH_MODULE_DEFINITION(mod_bfcp, mod_bfcp_load, mod_bfcp_shutdown, NULL);

/* Declaration of endpoint_interface of bfcp which will store function details and help to call from switch core */
switch_endpoint_interface_t *bfcp_endpoint_interface;

static switch_status_t bfcp_receive_message(switch_core_session_t *session, switch_core_session_message_t *msg);

/* State Handler API's called on different channel states to work on BFCP */
static switch_status_t bfcp_on_execute(switch_core_session_t *session);
static switch_status_t bfcp_on_exchange_media(switch_core_session_t *session);
static switch_status_t bfcp_on_consume_media(switch_core_session_t *session);
static switch_status_t bfcp_on_destroy(switch_core_session_t *session);

/*! \brief A table of i/o routines that an endpoint interface of mod_bfcp can implement */
switch_io_routines_t bfcp_io_routines = {
    /*.outgoing_channel */ NULL,
	/*.read_frame */ NULL,
	/*.write_frame */ NULL,
	/*.kill_channel */ NULL,
	/*.send_dtmf */ NULL,
	/*.receive_message */ bfcp_receive_message,
	/*.receive_event */ NULL,
	/*.state_change */ NULL,
	/*.read_video_frame */ NULL,
	/*.write_video_frame */ NULL,
	/*.read_text_frame */ NULL,
	/*.write_text_frame */ NULL,
	/*.state_run*/ NULL,
	/*.get_jb*/ NULL
};

/*! \brief A table of state handlers that an endpoint interface of mod_bfcp can implement */
switch_state_handler_table_t bfcp_event_handlers = {
	/*.on_init */ NULL,
    /*.on_routing */ NULL,
	/*.on_execute */ bfcp_on_execute,
	/*.on_hangup */ NULL,
	/*.on_exchange_media */ bfcp_on_exchange_media,
	/*.on_soft_execute */ NULL,
	/*.on_consume_media */ bfcp_on_consume_media,
	/*.on_hibernate */ NULL,
	/*.on_reset */ NULL,
	/*.on_park */ NULL,
	/*.on_reporting */ NULL,
	/*.on_destroy */ bfcp_on_destroy
};

static switch_status_t bfcp_receive_message(switch_core_session_t *session,
                                            switch_core_session_message_t *msg)
{
    bfcp_interface bfcp_interface_object;
    switch_channel_t *channel = switch_core_session_get_channel(session);

    switch (msg->message_id) {
    case SWITCH_MESSAGE_INDICATE_PROGRESS:
      {
          switch_channel_set_flag(channel, CF_EARLY_MEDIA_BFCP);
          bfcp_media_gen_local_sdp(session, SDP_TYPE_REQUEST);

          if ((bfcp_interface_object = (bfcp_interface) switch_core_session_get_private_class(session, SWITCH_PVT_TERTIARY))) {
              mod_bfcp_mutex_lock(&(bfcp_interface_object->bfcp_count_mutex));
              bfcp_interface_add_conference_to_server(bfcp_interface_object);
              mod_bfcp_mutex_unlock(&(bfcp_interface_object->bfcp_count_mutex));
          }
      }
      break;
    case SWITCH_MESSAGE_INDICATE_CLEAR_PROGRESS:
      {
          switch_channel_clear_flag(channel, CF_EARLY_MEDIA_BFCP);
      }
      break;
    default:
      break;
    }

    return SWITCH_STATUS_SUCCESS;
}

/* Parsing BFCP-SDP, generating local SDP and adding BFCP related details to server */
static switch_status_t bfcp_on_execute(switch_core_session_t *session)
{
    bfcp_interface bfcp_interface_object;

    if (bfcp_parse(session) != SWITCH_STATUS_SUCCESS) {
        return SWITCH_STATUS_FALSE;
    }

    bfcp_media_gen_local_sdp(session, SDP_TYPE_REQUEST);

    if ((bfcp_interface_object = (bfcp_interface) switch_core_session_get_private_class(session, SWITCH_PVT_TERTIARY))) {
        mod_bfcp_mutex_lock(&(bfcp_interface_object->bfcp_count_mutex));
        bfcp_interface_add_conference_to_server(bfcp_interface_object);
        mod_bfcp_mutex_unlock(&(bfcp_interface_object->bfcp_count_mutex));
    }

    return SWITCH_STATUS_SUCCESS;
}

/* Parsing BFCP-SDP, generating local SDP and adding BFCP related details to server */
static switch_status_t bfcp_on_exchange_media(switch_core_session_t *session)
{
    switch_channel_t *channel = switch_core_session_get_channel(session);
    bfcp_interface bfcp_interface_object;

    if (bfcp_parse(session) != SWITCH_STATUS_SUCCESS) {
        return SWITCH_STATUS_FALSE;
    }

    bfcp_media_gen_local_sdp(session, SDP_TYPE_RESPONSE);

    if (switch_channel_test_flag(channel, CF_PEER_BFCP) &&
        switch_channel_test_flag(channel, CF_BFCP) &&
        (bfcp_interface_object = (bfcp_interface) switch_core_session_get_private_class(session, SWITCH_PVT_TERTIARY))) {
        mod_bfcp_mutex_lock(&(bfcp_interface_object->bfcp_count_mutex));
        bfcp_interface_add_conference_to_server(bfcp_interface_object);
        mod_bfcp_mutex_unlock(&(bfcp_interface_object->bfcp_count_mutex));
    }

    return SWITCH_STATUS_SUCCESS;
}

/* Parsing BFCP-SDP, generating local SDP and adding BFCP related details to server */
static switch_status_t bfcp_on_consume_media(switch_core_session_t *session)
{
    bfcp_interface bfcp_interface_object;

    if (bfcp_parse(session) != SWITCH_STATUS_SUCCESS) {
        return SWITCH_STATUS_FALSE;
    }

    bfcp_media_gen_local_sdp(session, SDP_TYPE_RESPONSE);

    if ((bfcp_interface_object = (bfcp_interface) switch_core_session_get_private_class(session, SWITCH_PVT_TERTIARY))) {
        mod_bfcp_mutex_lock(&(bfcp_interface_object->bfcp_count_mutex));
        bfcp_interface_add_conference_to_server(bfcp_interface_object);
        mod_bfcp_mutex_unlock(&(bfcp_interface_object->bfcp_count_mutex));
    }

    return SWITCH_STATUS_SUCCESS;
}

/* Destroying BFCP interface object */
static switch_status_t bfcp_on_destroy(switch_core_session_t *session)
{
    bfcp_interface bfcp_interface_object;

    if ((bfcp_interface_object = (bfcp_interface) switch_core_session_get_private_class(session, SWITCH_PVT_TERTIARY))) {
        bfcp_interface_destroy_interface(bfcp_interface_object);
    }

    return SWITCH_STATUS_SUCCESS;
}

/* Initializing dummy values */
SWITCH_DECLARE(void) bfcp_initialize_bfcp_configuration_parameter()
{
    bfcp_conf_globals.ip = NULL;
    memset(bfcp_conf_globals.max_conf_per_server, 0, sizeof(bfcp_conf_globals.max_conf_per_server));
    bfcp_conf_globals.max_floor_per_conf = 0;
    memset(bfcp_conf_globals.max_floor_request_per_floor, 0, sizeof(bfcp_conf_globals.max_floor_request_per_floor));
    bfcp_conf_globals.wait_time_chair_action = 0;
    bfcp_conf_globals.bfcp_transport_tcp = 2;
    bfcp_conf_globals.bfcp_transport_udp = 0;
    bfcp_conf_globals.bfcp_port = 0;
}

/* Retrieving values from xml file and assigning it to configuration parameters */
static switch_status_t do_config(switch_bool_t reload)
{
    char *cf = "bfcp.conf";
    switch_xml_t cfg =  NULL, xml = NULL, settings, param;
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    switch_event_t *params = NULL;
    char *end;
	bfcp_initialize_bfcp_configuration_parameter();

    if (!(xml = switch_xml_open_cfg(cf, &cfg, params))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
		status = SWITCH_STATUS_FALSE;
		goto done;
	}

    if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *name = (char *) switch_xml_attr_soft(param, "name");
			char *value = (char *) switch_xml_attr_soft(param, "value");

			if (!strcasecmp(name, "ip-address")) {
                struct sockaddr_in sa;

                if (inet_pton(AF_INET, value, &(sa.sin_addr))) {
                    bfcp_conf_globals.ip = value;
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Starting FCS over ip : %s", bfcp_conf_globals.ip);
                } else {
                    bfcp_conf_globals.ip = "0.0.0.0";
                    switch_log_printf(SWITCH_CHANNEL_LOG,
                                      SWITCH_LOG_WARNING,
                                      "Invalid ip address! Using all network interfaces %s\n",
                                      bfcp_conf_globals.ip);
                }

            } else if (!strcasecmp(name, "max-conference-per-server")) {
                int max_conf_per_server;

                max_conf_per_server = strtol(value, &end, 10);

                if (!strcmp(end, "") && max_conf_per_server > 0) {
                    bfcp_conf_globals.max_conf_per_server[0] = max_conf_per_server;
                    bfcp_conf_globals.max_conf_per_server[1] = max_conf_per_server;
                    switch_log_printf(SWITCH_CHANNEL_LOG,
                                      SWITCH_LOG_INFO,
                                      "Maximum concurrent conferences in a server %u\n",
                                      bfcp_conf_globals.max_conf_per_server[0]);
                } else {
                    bfcp_conf_globals.max_conf_per_server[0] = BFCP_MAX_CONF;
                    bfcp_conf_globals.max_conf_per_server[1] = BFCP_MAX_CONF;
                    switch_log_printf(SWITCH_CHANNEL_LOG,
                                      SWITCH_LOG_WARNING,
                                      "Invalid parameter value, Using default value %u for maximum concurrent conferences in a server\n",
                                      bfcp_conf_globals.max_conf_per_server[0]);
                }

            } else if (!strcasecmp(name, "max-floor-per-conference")) {
                int max_floor_per_conf;

                max_floor_per_conf = strtol(value, &end, 10);

                if (!strcmp(end, "") && max_floor_per_conf > 0) {
                    bfcp_conf_globals.max_floor_per_conf = max_floor_per_conf;
                    switch_log_printf(SWITCH_CHANNEL_LOG,
                                      SWITCH_LOG_INFO,
                                      "Maximum concurrent floors in a conference %u\n",
                                      bfcp_conf_globals.max_floor_per_conf);
                } else {
                    bfcp_conf_globals.max_floor_per_conf = BFCP_MAX_FLOOR_PER_CONF;
                    switch_log_printf(SWITCH_CHANNEL_LOG,
                                      SWITCH_LOG_WARNING,
                                      "Invalid parameter value, Using default value %u for maximum concurrent floors in a conference\n",
                                      bfcp_conf_globals.max_floor_per_conf);
                }

            } else if (!strcasecmp(name, "max-floor-request-per-floor")) {
                int max_floor_request_per_floor;

                max_floor_request_per_floor = strtol(value, &end, 10);

                if (!strcmp(end, "") && max_floor_request_per_floor > 0) {
                    bfcp_conf_globals.max_floor_request_per_floor[0] = max_floor_request_per_floor;
                    bfcp_conf_globals.max_floor_request_per_floor[1] = max_floor_request_per_floor;
                    switch_log_printf(SWITCH_CHANNEL_LOG,
                                      SWITCH_LOG_INFO,
                                      "Maximum FloorRequest for a floor %u\n",
                                      bfcp_conf_globals.max_floor_request_per_floor[0]);
                } else {
                    bfcp_conf_globals.max_floor_request_per_floor[0] = BFCP_MAX_FLOORREQUESTS_PER_FLOOR;
                    bfcp_conf_globals.max_floor_request_per_floor[1] = BFCP_MAX_FLOORREQUESTS_PER_FLOOR;
                    switch_log_printf(SWITCH_CHANNEL_LOG,
                                      SWITCH_LOG_WARNING,
                                      "Invalid parameter value, Using default value %u for maximum FloorRequest for a floor\n",
                                      bfcp_conf_globals.max_floor_request_per_floor[0]);
                }

            } else if (!strcasecmp(name, "wait-time-chair-action")) {
                int wait_time_chair_action;

                wait_time_chair_action = strtol(value, &end, 10);

                if (!strcmp(end, "") && wait_time_chair_action > 0) {
                    bfcp_conf_globals.wait_time_chair_action = wait_time_chair_action;
                    switch_log_printf(SWITCH_CHANNEL_LOG,
                                      SWITCH_LOG_INFO,
                                      "Chair waiting time to take action %u seconds\n",
                                      bfcp_conf_globals.wait_time_chair_action);
                } else {
                    bfcp_conf_globals.wait_time_chair_action = BFCP_CHAIR_WAIT_REQUEST;
                    switch_log_printf(SWITCH_CHANNEL_LOG,
                                      SWITCH_LOG_WARNING,
                                      "Invalid parameter value, Using default value %u seconds for chair waiting time\n",
                                      bfcp_conf_globals.wait_time_chair_action);
                }

            } else if (!strcasecmp(name, "bfcp-transport-tcp")) {
                int bfcp_transport_tcp;

                if (strcmp(value, "")) {
                    bfcp_transport_tcp = strtol(value, &end, 10);

                    if (!strcmp(end, "") && (bfcp_transport_tcp == 0 || bfcp_transport_tcp == 1)) {
                        bfcp_conf_globals.bfcp_transport_tcp = bfcp_transport_tcp;
                        switch_log_printf(SWITCH_CHANNEL_LOG,
                                          SWITCH_LOG_INFO,
                                          "FCS is up for %s\n",
                                          (bfcp_conf_globals.bfcp_transport_tcp) ? "TCP/TLS/BFCP" : "TCP/BFCP");
                    } else {
                        bfcp_conf_globals.bfcp_transport_tcp = 2;
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "FCS is down for TCP/TLS/BFCP and TCP/BFCP\n");
                    }
                } else {
                    bfcp_conf_globals.bfcp_transport_tcp = 2;
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "FCS is down for TCP/TLS/BFCP and TCP/BFCP\n");
                }

            } else if (!strcasecmp(name, "bfcp-port")) {
                int bfcp_port;
                
                bfcp_port = strtol(value, &end, 10);

                if (!strcmp(end, "") && bfcp_port > 1024) {
                    bfcp_conf_globals.bfcp_port = bfcp_port;
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "FCS listening on port %u\n", bfcp_conf_globals.bfcp_port);
                } else {
                    bfcp_conf_globals.bfcp_port = BFCP_FCS_DEFAULT_PORT;
                    switch_log_printf(SWITCH_CHANNEL_LOG,
                                      SWITCH_LOG_WARNING,
                                      "Invalid parameter value, Using default value %u for port over which FCS will listen\n",
                                      bfcp_conf_globals.bfcp_port);
                }

            } else if (!strcasecmp(name, "bfcp-transport-udp")) {
                int bfcp_transport_udp;

                bfcp_transport_udp = strtol(value, &end, 10);

                if (!strcmp(end, "") && bfcp_transport_udp == 1) {
                    bfcp_conf_globals.bfcp_transport_udp = 1;
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "FCS is up for UDP/BFCP\n");
                } else {
                    bfcp_conf_globals.bfcp_transport_udp = 0;
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "FCS is down for UDP/BFCP\n");
                }
            }
        }
    }

    if ((!bfcp_conf_globals.bfcp_transport_udp) &&
        (bfcp_conf_globals.bfcp_transport_tcp > 1)) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "FCS is down for all protocols, unloading mod_bfcp\n");
        status = SWITCH_STATUS_FALSE;
        goto done;
    }

    if (!bfcp_conf_globals.max_conf_per_server) {
        bfcp_conf_globals.max_conf_per_server[0] = BFCP_MAX_CONF;
        bfcp_conf_globals.max_conf_per_server[1] = BFCP_MAX_CONF;
    } else if (!bfcp_conf_globals.max_floor_per_conf) {
        bfcp_conf_globals.max_floor_per_conf = BFCP_MAX_FLOOR_PER_CONF;
    } else if (!bfcp_conf_globals.max_floor_request_per_floor) {
        bfcp_conf_globals.max_floor_request_per_floor[0] = BFCP_MAX_FLOORREQUESTS_PER_FLOOR;
        bfcp_conf_globals.max_floor_request_per_floor[1] = BFCP_MAX_FLOORREQUESTS_PER_FLOOR;
    } else if (!bfcp_conf_globals.wait_time_chair_action) {
        bfcp_conf_globals.wait_time_chair_action = BFCP_CHAIR_WAIT_REQUEST;
    } else if(!bfcp_conf_globals.bfcp_port) {
        bfcp_conf_globals.bfcp_port = BFCP_FCS_DEFAULT_PORT;
    }

    done:
	return status;
}

SWITCH_STANDARD_API(bfcp_function)
{
    char *argv[1024] = { 0 };
	int argc = 0;
    uint32_t max_request = 0, max_conf = 0;
    char *mycmd = NULL;
    bfcp_conference *lconferences = NULL;
    bfcp_list_floors *list_floor = NULL;
    bfcp_list_users *list_user = NULL;
	bfcp_user *user = NULL;
	floor_query *query = NULL;
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    struct bfcp_server *bfcp_srv;

    static const char bfcp_welcome[] = "                            BFCP CONFERENCE SERVER \n"
		"**********************************************************************************\n"
        "* bfcp help                                                                      *\n"
        "* bfcp <tcp|udp> 1.Change_max_number_of_conference [1-65535]                     *\n"
        "* bfcp <tcp|udp> 2.Change_max_number_of_requests_for_same_floor [1-65535]        *\n"
        "* bfcp <tcp|udp> 3.Show_the_conferences_in_the_BFCP_Server                       *\n"
        "* bfcp <tcp|udp> 4.Show_configuration_parameters                                 *\n"
		"**********************************************************************************\n";

    if (zstr(cmd)) {
        /* Display options for both tcp & udp server */
		stream->write_function(stream, "%s", bfcp_welcome);
        goto done;
	}

    if (!(mycmd = strdup(cmd))) {
        /* Memory Error */
		status = SWITCH_STATUS_MEMERR;
        goto done;
	}

    if (!(argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) || !argv[0]) {
		stream->write_function(stream, "%s", bfcp_welcome);
        goto done;
	}

    if (!strcasecmp(argv[0], "udp")) {
        bfcp_srv = bfcp_srv_udp;
    } else if (!strcasecmp(argv[0], "tcp")) {
        bfcp_srv = bfcp_srv_tcp;
    } else if (!strcasecmp(argv[0], "help")) {
        stream->write_function(stream, "%s", bfcp_welcome);
        goto done;
    } else {
         stream->write_function(stream, "-ERR Usage: Invalid option. Please refer help option.\n");
        goto done;
    }

    if (argc > 1) {
        if (!strcasecmp(argv[1], "1.Change_max_number_of_conferences")) {
            if (argc == 3) {
                char *end;
                max_conf = strtoul(argv[2], &end, 10);
                if (max_conf < 1 || max_conf > 65535 || strcmp(end, "")) {
                    stream->write_function(stream, "-ERR Usage: Please enter value in range [1-65535]\n");
                    goto done;
                }

                if (bfcp_change_number_bfcp_conferences_server(bfcp_srv, max_conf) < 0) {
                    stream->write_function(stream, "Couldn't change maximum allowed conferences...\n");
                } else {
                    int i = (!strcasecmp(argv[0], "tcp")) ? 0 : 1;
                    stream->write_function(stream, "Maximum allowed number of conferences changed to %i \n", max_conf);
                    bfcp_conf_globals.max_conf_per_server[i] = max_conf;
                }
            } else {
                stream->write_function(stream, "-ERR Usage: bfcp %s 1.Change_max_number_of_conferences [1-65535]\n", !strcmp(argv[0], "udp") ? "udp" : "tcp");
            }
            goto done;

        } else if (!strcasecmp(argv[1], "2.Change_max_number_of_requests_for_floor")) {
            if (argc == 3) {
                char *end;
                max_request = strtoul(argv[2], &end, 10);
                if(max_request < 1 || max_request > 65535 || strcmp(end, "")) {
                    stream->write_function(stream, "-ERR Usage: Please enter value in range [1-65535]\n");
                    goto done;                    
                }
                if (bfcp_change_user_req_floors_server(bfcp_srv, max_request) < 0) {
                    stream->write_function(stream, "Couldn't change the maximum number of requests...\n");
                } else {
                    int i = (!strcasecmp(argv[0], "tcp")) ? 0 : 1;
                    stream->write_function(stream, "Maximum number of requests changed to %i\n", max_request);
                    bfcp_conf_globals.max_floor_request_per_floor[i] = max_request;
                }
            } else { 
                stream->write_function(stream, "-ERR Usage: bfcp %s 2.Change_max_number_of_requests_for_floor [1-65535]\n", !strcmp(argv[0], "udp") ? "udp" : "tcp");
            }
            goto done;

        } else if (!strcasecmp(argv[1], "3.Show_conferences_in_the_BFCP_Server")) {
            if (bfcp_srv == NULL) {
                stream->write_function(stream, "The Floor Control Server is not up\n");
                goto done;
		    }
            lconferences = bfcp_srv->list_conferences;

            if (lconferences == NULL || bfcp_srv->Actual_number_conference == 0) {
                stream->write_function(stream, "There is no conference in the FCS\n");
                goto done;
            }

		    for (int i = 0; i < bfcp_srv->Actual_number_conference; i++) {
                stream->write_function(stream, "CONFERENCE:\n");
                stream->write_function(stream, "ConferenceID: %u\n", bfcp_srv->list_conferences[i].conferenceID);
                stream->write_function(stream, "\n");
                /* Print the list of floors */
                list_floor = bfcp_srv->list_conferences[i].floor;

                if (list_floor) {
                    stream->write_function(stream, "Maximum number of floors in the conference: %i\n", list_floor->number_floors + 1);
                    stream->write_function(stream, "FLOORS\n");

                    for (int j = 0; j < list_floor->actual_number_floors; j++) {
                        stream->write_function(stream, "FloorID: %u, ", list_floor->floors[j].floorID);
                        stream->write_function(stream, "ChairID: %u, " , list_floor->floors[j].chairID);

                        if (list_floor->floors[j].floorState == BFCP_FLOOR_STATE_WAITING) {
                            stream->write_function(stream, "state: FREE\n");
                        } else if (list_floor->floors[j].floorState == BFCP_FLOOR_STATE_ACCEPTED) {
                            stream->write_function(stream, "state: ACCEPTED\n");
                        } else if (list_floor->floors[j].floorState >= BFCP_FLOOR_STATE_GRANTED) {
                            stream->write_function(stream, "state: GRANTED\n");
                        } else {
						    stream->write_function(stream, "state: ERROR!\n");
                        }

                        stream->write_function(stream, "Number of simultaneous granted users:% i\n", list_floor->floors[j].limit_granted_floor-1);

                        query = list_floor->floors[j].floorquery;

                        if (query) {
                            stream->write_function(stream, "QUERY LIST\n");
                        }

                        while (query) {
							stream->write_function(stream, "User: %u\n", query->userID);
						    query = query->next;
					    }
				    }
			    }

                /* Print the list of users */
			    if ((list_user = bfcp_srv->list_conferences[i].user)) {
                    stream->write_function(stream, "Maximum number of request per floors in the conference: %i\n", list_user->max_number_floor_request);
                    stream->write_function(stream, "USERS\n");

                    user = list_user->users;
                    while (user) {
                        stream->write_function(stream, "UserID: %u\n", user->userID);
                        user = user->next;
                    }
			    }

		        /* Print the list of Pending requests */
                print_requests_list(bfcp_srv, i, BFCP_PENDING, stream);
		        /* Print the list of Accepted requests */
			    print_requests_list(bfcp_srv, i, BFCP_ACCEPTED, stream);
			    /* Print the list of Granted requests */
			    print_requests_list(bfcp_srv, i, BFCP_GRANTED, stream);
		    }

            goto done;
        } else if (!strcasecmp(argv[1], "4.Show_configuration_parameters")) {
            int i = (!strcasecmp(argv[0], "tcp")) ? 0 : 1;
            stream->write_function(stream, "Maximum number of conference in a server:%i\n", bfcp_conf_globals.max_conf_per_server[i]);
            stream->write_function(stream, "Maximum number of request per floor:%i\n", bfcp_conf_globals.max_floor_request_per_floor[i]);
            stream->write_function(stream, "Maximum number of floor in a conference:%i\n", bfcp_conf_globals.max_floor_per_conf);
            if (bfcp_conf_globals.bfcp_transport_tcp == 1) {
                stream->write_function(stream, "TCP/TLS/BFCP socket is up.\n");
            } else if (bfcp_conf_globals.bfcp_transport_tcp == 0) {
                stream->write_function(stream, "TCP/BFCP socket is up.\n");
            } else {
                stream->write_function(stream, "TCP/BFCP or TCP/TLS/BFCP socket is down.\n");
            }

            if (bfcp_conf_globals.bfcp_transport_udp == 1) {
                stream->write_function(stream, "UDP/BFCP socket is up.\n");
            } else {
                stream->write_function(stream, "UDP/BFCP socket is down.\n");
            }

            stream->write_function(stream, "BFPC server is running on port:%i\n", bfcp_conf_globals.bfcp_port);
            goto done;
        }
    } else {
        stream->write_function(stream, "-ERR Usage: Refer help option");
    }

	done:
	switch_safe_free(mycmd);
	return status;
}

/* Macro expands to: switch_status_t mod_bfcp_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool) */
SWITCH_MODULE_LOAD_FUNCTION(mod_bfcp_load)
{
	/* indicate that the module should continue to be loaded */
    switch_api_interface_t *api_interface;

    if (do_config(SWITCH_FALSE) != SWITCH_STATUS_SUCCESS) {
        return SWITCH_STATUS_FALSE;
    }

    /* Pointer to FCS instance */
    bfcp_srv_tcp = NULL;
    bfcp_srv_udp = NULL;

    /* Flag to maintain server type */
    udp_server = 0;
    tcp_server = 0;

    if (start_bfcp_server() != SWITCH_STATUS_SUCCESS) {
        return SWITCH_STATUS_FALSE;
    }

    /* connect my internal structure to the blank pointer passed to me */
    *module_interface = switch_loadable_module_create_module_interface(pool, modname);

    bfcp_endpoint_interface = (switch_endpoint_interface_t *) switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE);
	bfcp_endpoint_interface->interface_name = "bfcp";
	bfcp_endpoint_interface->state_handler = &bfcp_event_handlers;
    bfcp_endpoint_interface->io_routines = &bfcp_io_routines;

    conference_id_counter = 0;
    user_id_counter = 0;

    SWITCH_ADD_API(api_interface, "bfcp", "BFCP API", bfcp_function, "syntax");


    switch_console_set_complete("add bfcp udp");
    switch_console_set_complete("add bfcp udp 1.Change_max_number_of_conferences");
    switch_console_set_complete("add bfcp udp 2.Change_max_number_of_requests_for_floor");
    switch_console_set_complete("add bfcp udp 3.Show_conferences_in_the_BFCP_Server");
    switch_console_set_complete("add bfcp udp 4.Show_configuration_parameters");
    switch_console_set_complete("add bfcp tcp");
    switch_console_set_complete("add bfcp tcp 1.Change_max_number_of_conferences");
    switch_console_set_complete("add bfcp tcp 2.Change_max_number_of_requests_for_floor");
    switch_console_set_complete("add bfcp tcp 3.Show_conferences_in_the_BFCP_Server");
    switch_console_set_complete("add bfcp tcp 4.Show_configuration_parameters");
    switch_console_set_complete("add bfcp help");

	return SWITCH_STATUS_SUCCESS;
}

/*
  Called when the system shuts down
  Macro expands to: switch_status_t mod_bfcp_shutdown() */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_bfcp_shutdown)
{
    /* Condtion to check whether BFCP supported SIP call running or not */
    if ((bfcp_srv_tcp && bfcp_srv_tcp->Actual_number_conference != 0) || (bfcp_srv_udp && bfcp_srv_udp->Actual_number_conference != 0)) {
        return SWITCH_STATUS_NOUNLOAD;
    }

    stop_bfcp_server();
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Shutting Down BFCP\n");

	return SWITCH_STATUS_UNLOAD;
}