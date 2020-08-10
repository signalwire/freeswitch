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
 * bfcp_interface.c -- LIBBFCP ENDPOINT CODE (code to tie libbfcp with freeswitch)
 *
 */
#include "mod_bfcp.h"

/* Callback to receive notifications from the underlying library about incoming BFCP messages */
int received_msg(bfcp_arguments *arguments,
                 int is_outgoing)
{
	uint64_t conference_id;
	uint16_t user_id, transaction_id;

	if (!arguments) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Invalid arguments in the received message!\n");
		return -1;
	}

	if (!arguments->entity) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Invalid IDs in the message header!\n");
		bfcp_free_arguments(arguments);
		return -1;
	}
	
	conference_id = arguments->entity->conferenceID;
	user_id = arguments->entity->userID;
	transaction_id = arguments->entity->transactionID;

	if (arguments->primitive <= 16) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "(%lu/%d/%d) %s %s\n", conference_id, transaction_id, user_id, is_outgoing ? "--->" :"<---", bfcp_primitive[arguments->primitive-1].description);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "(%lu/%d/%d) %s %s\n", conference_id, transaction_id, user_id, is_outgoing ? "--->" :"<---", "UNKNOWN PRIMITIVE");
	}

	return 0;
}

/* To print the Floor State of BFCP  */
 switch_status_t print_requests_list(bfcp server,
                                     int index,
                                     int status,
                                     switch_stream_handle_t *stream)
{
    bfcp_queue *list = NULL;
	bfcp_node *node = NULL;
	floor_request_query *query_floorrequest = NULL;
	bfcp_floor *floor = NULL;
	
	if (!server) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "BFCP Server is not up");
        return SWITCH_STATUS_SUCCESS;
    }

	switch (status) {
		case BFCP_PENDING:
			list = server->list_conferences[index].pending;
			break;
		case BFCP_ACCEPTED:
			list = server->list_conferences[index].accepted;
			break;
		case BFCP_GRANTED:
			list = server->list_conferences[index].granted;
			break;
		default:
			break;
	}

	if (list) {
		node = list->head;

		if (node) {
            stream->write_function(stream, "%s LIST\n", bfcp_status[status-1].description);
        }

		while (node) {
			stream->write_function(stream, "FloorRequestID: %u\n\tUserID: %u Priority: %i Queue_position: %u ", node->floorRequestID, node->userID, node->priority, node->queue_position);

			if (node->beneficiaryID) {
                stream->write_function(stream, "BeneficiaryID: %u", node->beneficiaryID);
            }

			stream->write_function(stream, "\n");
			floor = node->floor;

			while (floor) {
				stream->write_function(stream, "FloorID: %u ", floor->floorID);

				if (floor->chair_info) {
                    stream->write_function(stream, "Chair-provided information: %s ", floor->chair_info);
                }

				if (floor->status == BFCP_FLOOR_STATE_WAITING) {
                    stream->write_function(stream, " state: FREE\n");
                } else if (floor->status == BFCP_FLOOR_STATE_ACCEPTED) {
                    stream->write_function(stream, " state: ACCEPTED\n");
                } else if (floor->status >= BFCP_FLOOR_STATE_GRANTED) {
                    stream->write_function(stream, " state: GRANTED\n");
                } else {
                    stream->write_function(stream, " state: ERROR!\n");
                }

				floor = floor->next;
			}

			query_floorrequest = node->floorrequest;

			while (query_floorrequest) {
				stream->write_function(stream, "FloorRequest query: %u\n", query_floorrequest->userID);
				query_floorrequest = query_floorrequest->next;
			}

			node = node->next;
			stream->write_function(stream, "-----------------\n");
		}
	}
    return SWITCH_STATUS_SUCCESS;
}

/* Get new Conference ID */
uint64_t bfcp_get_conf_id()
{
	uint64_t conference_id = (++conference_id_counter) % get_max_val(sizeof(conference_id_counter));

	if (!conference_id) {
		conference_id_counter = 1;
	}

	return conference_id ? conference_id : conference_id_counter;
}

/* Get new User ID */
uint16_t bfcp_get_user_id()
{
	uint16_t user_id = (++user_id_counter) % get_max_val(sizeof(user_id_counter));

	if (!user_id) {
		user_id_counter = 1;
	}

	return (user_id) ? user_id : user_id_counter;
}

/* Create a new BFCP Interface */
bfcp_interface create_bfcp_interface(char *p_uuid)
{
	bfcp_interface bfcp_interface_object; 
	
	if (!p_uuid) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Invalid UUID!\n");
		return NULL;
	}

	/* Allocate and setup the new participant */
	bfcp_interface_object = (bfcp_interface) calloc(1, sizeof(bfcp_object_t));

	if (!bfcp_interface_object) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Could not allocate memory to interface object!\n");
		return NULL;
	}

	mod_bfcp_mutex_init(&(bfcp_interface_object->bfcp_count_mutex), NULL);
	mod_bfcp_mutex_lock(&(bfcp_interface_object->bfcp_count_mutex));

	/* Assigning dummy values */
	bfcp_interface_object->m_efloorctrl_mode = FLOOCTRL_MODE_CLIENT;
	bfcp_interface_object->m_uuid = strdup((char *) p_uuid);
	bfcp_interface_object->m_user_id = 0;
	bfcp_interface_object->m_conf_id = 0;
	bfcp_interface_object->m_is_passive = false;
	bfcp_interface_object->m_floor_stream_map = NULL;
	bfcp_interface_object->m_floor_stream_count = 0;

	mod_bfcp_mutex_unlock(&(bfcp_interface_object->bfcp_count_mutex));

	return bfcp_interface_object;
}

/* Add Conference and other details into FCS */
void bfcp_interface_add_conference_to_server(bfcp_interface interface)
{
	uint32_t max_num_floors, max_number_floor_request, chair_wait_request;
	uint16_t floor_id, user_id;
	uint64_t conference_id;
	struct bfcp_server *bfcp_srv;
	floor_stream_mapping_t *floor_stream_map;

	conference_id = bfcp_interface_get_conf_id(interface);
	user_id = bfcp_interface_get_user_id(interface);
	max_num_floors = bfcp_conf_globals.max_floor_per_conf;
	chair_wait_request = bfcp_conf_globals.wait_time_chair_action;

	if (interface->m_transport == BFCP_OVER_UDP) {
		bfcp_srv = bfcp_srv_udp;
		max_number_floor_request = bfcp_conf_globals.max_floor_request_per_floor[1];
	} else {
		bfcp_srv = bfcp_srv_tcp;
		max_number_floor_request = bfcp_conf_globals.max_floor_request_per_floor[0];
	}

	if (bfcp_initialize_conference_server(bfcp_srv, conference_id, max_num_floors, max_number_floor_request, BFCP_AUTOMATED_CHAIR_POLICY, chair_wait_request) < 0) {
		if (bfcp_interface_check_conference_existance(conference_id, interface->m_transport) < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Couldn't add the conference %lu to the FCS!\n", conference_id);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Conference %lu Already Added in FCS!\n", conference_id);
		}
	} else { 
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Conference %lu added successfully.\n", conference_id);
	}


	for (floor_stream_map = bfcp_interface_get_floor_stream_mapping(interface); floor_stream_map; floor_stream_map = floor_stream_map->next) {

		floor_id = floor_stream_map->m_floor_id;

		if (bfcp_add_floor_server(bfcp_srv, conference_id, floor_id, BFCP_CHAIR_MANAGE_FLOOR, BFCP_MAX_FLOOR_GRANT_AT_A_TIME) < 0) {
			if (bfcp_interface_check_floor_existance(conference_id, floor_id, interface->m_transport) < 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Coudln't add the new floor!\n");
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Floor %u already Added!\n", floor_id);
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Floor %d added.\n", floor_id);
		}
	}

	if (bfcp_add_user_server(bfcp_srv, conference_id, user_id, NULL, NULL) < 0) {
		if (bfcp_interface_check_user_existance(conference_id, user_id, interface->m_transport) < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Couldn't add the new user!\n");
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "User %u Already Added!\n", user_id);
			if (bfcp_srv->bfcp_transport == BFCP_OVER_UDP) {
				if (bfcp_set_user_address_server(bfcp_srv, conference_id, user_id, interface->m_client_address, interface->m_client_port) < 0) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Unable to add client address!\n");
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Client address %s::%d mapped with userID %d!\n", interface->m_client_address, interface->m_client_port, user_id);
				}
			}
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "User %d added.\n", user_id);
		if (bfcp_srv->bfcp_transport == BFCP_OVER_UDP) {
			if (bfcp_set_user_address_server(bfcp_srv, conference_id, user_id, interface->m_client_address, interface->m_client_port) < 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Unable to add client address!\n");
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Client address %s::%d mapped with userID %d!\n", interface->m_client_address, interface->m_client_port, user_id);
			}
		}
	}
}

/* Destroying BFCP Interface */
void bfcp_interface_destroy_interface(bfcp_interface interface)
{
	struct bfcp_server *bfcp_srv;
	char *uuid = bfcp_interface_get_uuid(interface);
	char *media_stream_str = bfcp_interface_get_media_stream_str(interface);
	char *client_address = bfcp_interface_get_client_address(interface);
	int i = 0, conf_count = 0;
	uint64_t conf_id = bfcp_interface_get_conf_id(interface);
	uint16_t user_id = bfcp_interface_get_user_id(interface);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Destroy Interface called for user %u\n", user_id);

	if (interface->m_transport == BFCP_OVER_UDP) {
		bfcp_srv = bfcp_srv_udp;
	} else {
		bfcp_srv = bfcp_srv_tcp;
	}

	conf_count = bfcp_srv->Actual_number_conference;

	for (i = 0; i < conf_count && (bfcp_srv->list_conferences[i].conferenceID != conf_id); i++);

	if (i < conf_count) {
		if (bfcp_delete_user_server(bfcp_srv, conf_id, user_id) == 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Removed user %u\n", bfcp_interface_get_user_id(interface));
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Couldn't remove the user %u from the FCS!\n", user_id);
		}

		/* Removing the conference, no user present in the conference */
		if (!bfcp_srv->list_conferences[i].user->users) {
			if (bfcp_destroy_conference_server(bfcp_srv, bfcp_interface_get_conf_id(interface)) < 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Couldn't remove the conference %lu from the FCS!\n", conf_id);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Conference %lu removed.\n", conf_id);
			}
		}

	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Conference %lu not present or already removed\n", conf_id);
	}

	mod_bfcp_mutex_lock(&(interface->bfcp_count_mutex));

	/* Dealocating the memories */
	free(uuid);
	uuid = NULL;
	free(media_stream_str);
	media_stream_str = NULL;
	free(client_address);
	client_address = NULL;

	bfcp_interface_destroy_floor_stream_mapping(bfcp_interface_get_floor_stream_mapping(interface));

	mod_bfcp_mutex_unlock(&(interface->bfcp_count_mutex));
	mod_bfcp_mutex_destroy(&(interface->bfcp_count_mutex));

	free(interface);
	interface = NULL;
}

/* Destroys Floor Stream Map */
void bfcp_interface_destroy_floor_stream_mapping(floor_stream_mapping_t *floor_stream_map)
{
	floor_stream_mapping_t *temp;

	while(floor_stream_map) {
		temp = floor_stream_map;
		floor_stream_map = floor_stream_map->next;
		free(temp);
	}
}

/* Check whether conferenceID exist in conference list or not */
int bfcp_interface_check_conference_existance(uint64_t conf_id,
                                              uint16_t transport)
{
	int conf_count = 0, i;

	struct bfcp_server *bfcp_srv;

	if (transport == BFCP_OVER_UDP) {
		bfcp_srv = bfcp_srv_udp;
	} else {
		bfcp_srv = bfcp_srv_tcp;
	}

	if (bfcp_srv == NULL || conf_id <= 0) {
		return -1;
	}

	bfcp_mutex_lock(&count_mutex);

	conf_count = bfcp_srv->Actual_number_conference;

	for (i = 0; i < conf_count && (bfcp_srv->list_conferences[i].conferenceID != conf_id); i++);

	if (i < conf_count) {
		bfcp_mutex_unlock(&count_mutex);
		return 0;
	}

	bfcp_mutex_unlock(&count_mutex);
	return -2;
}

/* Check whether floorID exist in floor list of particular conference or not */
int bfcp_interface_check_floor_existance(uint64_t conf_id,
                                         uint16_t floor_id,
                                         uint16_t transport)
{
	int conf_count = 0, i = 0, j = 0;

	struct bfcp_server *bfcp_srv;

	if (transport == BFCP_OVER_UDP) {
		bfcp_srv = bfcp_srv_udp;
	} else {
		bfcp_srv = bfcp_srv_tcp;
	}

	if (bfcp_srv == NULL || conf_id <= 0 || floor_id <= 0) {
		return -1;
	}

	bfcp_mutex_lock(&count_mutex);

	conf_count = bfcp_srv->Actual_number_conference;

	for (i = 0; i < conf_count && (bfcp_srv->list_conferences[i].conferenceID != conf_id); i++);

	if (i < conf_count) {
		bfcp_list_floors *list_floors;

		list_floors = bfcp_srv->list_conferences[i].floor;

		if (!list_floors) {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}

		for (j = 0; j < list_floors->actual_number_floors && (list_floors->floors[j].floorID != floor_id); j++);

		if (j < list_floors->actual_number_floors) {
			/* A floor with the same floorID already exists in this conference */
			bfcp_mutex_unlock(&count_mutex);
			return 0;
		}
	}

	bfcp_mutex_unlock(&count_mutex);
	return -1;
}

/* Check whether userID exist in user list of particular conference or not */
int bfcp_interface_check_user_existance(uint64_t conf_id,
                                        uint16_t user_id,
                                        uint16_t transport)
{
	int i, conf_count = 0;
	struct bfcp_server *bfcp_srv;

	if (transport == BFCP_OVER_UDP) {
		bfcp_srv = bfcp_srv_udp;
	} else {
		bfcp_srv = bfcp_srv_tcp;
	}

	if (bfcp_srv == NULL || conf_id <= 0 || user_id <= 0) {
		return -1;
	}

	bfcp_mutex_lock(&count_mutex);

	conf_count = bfcp_srv->Actual_number_conference;

	for (i = 0; i < conf_count && (bfcp_srv->list_conferences[i].conferenceID != conf_id); i++);

	if (i < conf_count) {
		users user = NULL;
		lusers list_users;

		list_users = bfcp_srv->list_conferences[i].user;

		if (list_users == NULL) {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}

		for (user = list_users->users; user && user->userID != user_id; user = user->next);

		if (user) {
			/* This user already exists in the list */
			bfcp_mutex_unlock(&count_mutex);
			return 0;
		}
	}

	bfcp_mutex_unlock(&count_mutex);
	return -1;
}

void bfcp_interface_set_user_id(bfcp_interface interface,
								uint16_t user_id)
{
	interface->m_user_id = user_id;
}

void bfcp_interface_set_conf_id(bfcp_interface interface,
								uint64_t conf_id)
{
	interface->m_conf_id = conf_id;
}

void bfcp_interface_set_floorctrl_mode(bfcp_interface interface,
									   e_floorctrl_mode floorctrl_mode)
{
	interface->m_efloorctrl_mode = floorctrl_mode;
}

void bfcp_interface_set_is_passive(bfcp_interface interface ,
								   bool is_passive)
{
	interface->m_is_passive = is_passive;
}

void bfcp_interface_set_media_stream_str(bfcp_interface interface,
										 char *media_stream_str)
{
	interface->m_media_stream_str = strdup((char *) media_stream_str);
}

void bfcp_interface_set_floor_stream_mapping(bfcp_interface interface,
											 uint16_t floor_id,
											 uint16_t stream_id)
{
	floor_stream_mapping_t *head = interface->m_floor_stream_map, *temp , *new_node;

	if (bfcp_conf_globals.max_floor_per_conf < interface->m_floor_stream_count) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Maximum number of floor has been assigned into list for a conference! No more floor is allowed \n");
		return;
	}

	for (temp = head; temp; temp = (temp->next)) {
		if (temp->m_floor_id == floor_id) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Floor ID %d already present in the list\n", floor_id);
			return;
		}

		if (!temp->next) {
			break;
		}
	}

	new_node = (floor_stream_mapping_t*)malloc(sizeof(floor_stream_mapping_t));

	new_node->m_floor_id = floor_id;
	new_node->m_stream_id = stream_id;
	new_node->next = NULL;

	if(!temp) {
		interface->m_floor_stream_map = new_node;
	} else {
		temp->next = new_node;
	}

	interface->m_floor_stream_count++;
}

uint16_t bfcp_interface_get_user_id(bfcp_interface interface)
{
	return interface->m_user_id;
}

uint16_t bfcp_interface_floor_stream_count(bfcp_interface interface)
{
	return interface->m_floor_stream_count;
}

uint64_t bfcp_interface_get_conf_id(bfcp_interface interface)
{
	return interface->m_conf_id;
}

uint16_t bfcp_interface_get_client_port(bfcp_interface interface)
{
	return interface->m_client_port;
}

floor_stream_mapping_t *bfcp_interface_get_floor_stream_mapping(bfcp_interface interface)
{
	return interface->m_floor_stream_map;
}

e_floorctrl_mode bfcp_interface_get_floorctrl_mode(bfcp_interface interface)
{
	return interface->m_efloorctrl_mode;
}

bool bfcp_interface_get_is_passive(bfcp_interface interface)
{
	return interface->m_is_passive;
}

char *bfcp_interface_get_uuid(bfcp_interface interface)
{
	return interface->m_uuid;
}

char *bfcp_interface_get_media_stream_str(bfcp_interface interface)
{
	return interface->m_media_stream_str;
}

char *bfcp_interface_get_client_address(bfcp_interface interface)
{
	return interface->m_client_address;
}


/* Start BFCP server instance on parameters taken from configuration file of mod_bfcp */
switch_status_t start_bfcp_server()
{
	/* bfcp_conf_globals.bfcp_transport_tcp == 0 , start BFCP server over TCP socket
	   bfcp_conf_globals.bfcp_transport_tcp == 1, start BFCP server over TCP/TLS socket */
	if (bfcp_conf_globals.bfcp_transport_tcp == 0 || bfcp_conf_globals.bfcp_transport_tcp == 1) {
		bfcp_srv_tcp = bfcp_initialize_bfcp_server(bfcp_conf_globals.max_conf_per_server[0],
                                                   bfcp_conf_globals.bfcp_port,
                                                   received_msg,
                                                   bfcp_conf_globals.bfcp_transport_tcp,
                                                   NULL,
                                                   NULL,
                                                   "0.0.0.0");

		if (bfcp_srv_tcp) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,"%s FCS created.\n", (bfcp_conf_globals.bfcp_transport_tcp) ? "TCP/TLS/BFCP" : "TCP/BFCP");
			tcp_server = 1;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,"Couldn't create the FCS over TCP, is the port already taken?\n");
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,"TCP/BFCP and TCP/TLS/BFCP FCS are down.\n");
	}

	if (bfcp_conf_globals.bfcp_transport_udp) {
		bfcp_srv_udp = bfcp_initialize_bfcp_server(bfcp_conf_globals.max_conf_per_server[1],
                                                   bfcp_conf_globals.bfcp_port,
                                                   received_msg,
                                                   BFCP_OVER_UDP,
                                                   NULL,
                                                   NULL,
                                                   "0.0.0.0");

		if (bfcp_srv_udp) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,"UDP/BFCP FCS created.\n");
			udp_server = 1;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,"Couldn't create the FCS over UDP, is the port already taken?\n");
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,"UDP FCS server is down.\n");
	}

	if (!bfcp_srv_udp && !bfcp_srv_tcp) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to open socket for UDP and TCP\n");
		return SWITCH_STATUS_FALSE;
	}

	return SWITCH_STATUS_SUCCESS;
}

/* Destroy BFCP server instance */
void stop_bfcp_server()
{
	int error;

	if (bfcp_srv_tcp) {
		error = bfcp_destroy_bfcp_server(&bfcp_srv_tcp);

		if (error >= 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,"TCP FCS Stopped.\n");
			tcp_server = 0;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,"Couldn't destroy TCP FCS!\n");
		}
	}

	if (bfcp_srv_udp) {
		error = bfcp_destroy_bfcp_server(&bfcp_srv_udp);

		if (error >= 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,"UDP FCS Stopped.\n");
			udp_server = 0;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,"Couldn't destroy UDP FCS !\n");
		}
	}
}

/* To calculate maximum value for confID & userID*/
uint64_t get_max_val(uint32_t bytes)
{
    int bits = 8 * bytes;
    
    uint64_t max = (1LU << (bits - 1)) + ((1LU << (bits - 1)) - 1);
    
    return max;
}