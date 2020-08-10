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
 * bfcp_interface.h -- LIBBFCP ENDPOINT CODE (code to tie libbfcp with freeswitch)
 *
 */

/* Library for server of libbfcp */
#include "bfcp_server.h"
#include "bfcp_messages.h"
#include <bfcp_strings.h>
#include <stdbool.h>

/* Library for thread of mod_bfcp */
#include "bfcp_thread.h"

#define CLIENT_CONF_ID 1
#define CLIENT_FLOOR_ID 1
#define STREAM_ID 3

#define REMOTE_CLIENT_USER_ID 2
#define LOCAL_CLIENT_USER_ID  3

#define USER_BUF_SIZE 6
#define BFCP_VERSION 1

/*! \brief BFCP server related parameters whose place need to be constant, please don't modify */
#define BFCP_AUTOMATED_CHAIR_POLICY       0  /* Accepting request when chair is missing  (0 Accepting request, 1 don't */
#define BFCP_MAX_FLOOR_GRANT_AT_A_TIME    1  /* Number of users that can be granted same floor at a time */
#define BFCP_CHAIR_MANAGE_FLOOR           0  /* userid of chair who will manage particular floor, if no chair then 0 */
#define BFCP_MAX_CONF                    64  /* Maximum number of allowed concurrent conferences in a server */
#define BFCP_MAX_FLOOR_PER_CONF          64  /* Maximum number of allowed concurrent conferences in a conference */
#define BFCP_MAX_FLOORREQUESTS_PER_FLOOR 64  /* Maximum number of FloorRequests for a floor from a user */
#define BFCP_CHAIR_WAIT_REQUEST         300  /* Default chair wait time */

uint16_t user_id_counter; /* Maintains userID value to assign to new BFCP participant */
uint64_t conference_id_counter; /* Maintains confID value to assign to new BFCP conference */

uint16_t tcp_server, udp_server; /* Maintains server type while creating and destroying BFCP server */

/*! \brief Enumration for BFCP Floor Control mode */
typedef enum {
	FLOOCTRL_MODE_CLIENT = 0 ,
	FLOOCTRL_MODE_SERVER ,
	FLOOCTRL_MODE_CLIENT_AND_SERVER
}e_floorctrl_mode ;

/*! \brief Floor Stream Mapping */
struct floor_stream_mapping_s {
	uint16_t m_floor_id;
	uint16_t m_stream_id;
	struct floor_stream_mapping_s *next;
};

typedef struct floor_stream_mapping_s floor_stream_mapping_t;

/*! \brief BFCP interface */
typedef struct bfcp_object_s {
	e_floorctrl_mode m_efloorctrl_mode;
	char* m_uuid;
	char* m_media_stream_str;
	char* m_client_address;
	uint16_t m_client_port;
	uint64_t m_conf_id;
	uint16_t m_user_id;
	uint16_t m_floor_stream_count;
	uint16_t m_transport;
	floor_stream_mapping_t *m_floor_stream_map;
	bool m_is_passive;
	mod_bfcp_mutex_t bfcp_count_mutex;
} bfcp_object_t;

typedef bfcp_object_t *bfcp_interface;

/*!
  \brief Receive notifications from the underlying library about incoming BFCP messages
  \param arguments BFCP arguments
  \param is_outgoing Direction of message flow (incoming or outgoing)
  \return 0 : After successful completion
  		 -1 : Otherwise
 */
int received_msg(bfcp_arguments *arguments,
                 int is_outgoing);

/*!
  \brief Print floor state of BFCP
  \param server Pointer to FCS instance
  \param index Variable for list_conference index
  \param status BFCP floor request status
  \param stream Pointer of switch_stream_handle
  \return SWITCH_STATUS_SUCCESS
 */
switch_status_t print_requests_list(bfcp server,
                                    int index,
                                    int status,
                                    switch_stream_handle_t *stream);

/* BFCP Server-related Operation */

/*! \brief Start BFCP server instance on parameter taken from configuration file of mod_bfcp */
switch_status_t start_bfcp_server();

/*! \brief Destroy BFCP server instance */
void stop_bfcp_server();

/* BFCP Interface-related operations */

/* Create a new BFCP Interface */
bfcp_interface create_bfcp_interface(char *p_uuid);

/*! \brief Interface API to add conference to BFCP server */
void bfcp_interface_add_conference_to_server(bfcp_interface interface);

/*! \brief API to destroy BFCP interface. Memory pools are returned to the core and utilized memory from the channel is freed*/
void bfcp_interface_destroy_interface(bfcp_interface interface);

/*! \brief Destroying Floor Stream Mapping */
void bfcp_interface_destroy_floor_stream_mapping(floor_stream_mapping_t *floor_stream_map);

/*!
  \brief Check if ConferenceId exists in conference list
  \param conf_id BFCP ConferenceID
  \param tansport server type (UDP or TCP)
  \return 0 : If ConferenceID exists in the conference list
  		 -1 : If Server or ConferenceID is invalid
		 -2 : If conferenceID doesn't exist in the conference list
 */
int bfcp_interface_check_conference_existance(uint64_t conf_id,
											  uint16_t transport);

/*!
  \brief Check if FloorId exists in floor list of particular conference or not
  \param conf_id BFCP ConferenceID
  \param floor_id BFCP FloorID
  \param tansport server type (UDP or TCP)
  \return 0 : If FloorID exists in the floor list
  		 -1 : Otherwise
 */
int bfcp_interface_check_floor_existance(uint64_t conf_id,
										 uint16_t floor_id,
										 uint16_t transport);

/*!
  \brief Check if UserId exists in user list of particular conference or not
  \param conf_id BFCP ConferenceID
  \param user_id BFCP UserID
  \param tansport server type (UDP or TCP)
  \return 0 : If UserID exists in the user list
  		 -1 : Otherwise
 */
int bfcp_interface_check_user_existance(uint64_t conf_id,
										uint16_t user_id,
										uint16_t transport);

/*! \brief Setter API for interface data member */
void bfcp_interface_set_user_id(bfcp_interface interface,
								uint16_t user_id);

void bfcp_interface_set_conf_id(bfcp_interface interface,
								uint64_t conf_id);

void bfcp_interface_set_floor_stream_mapping(bfcp_interface interface,
											 uint16_t foor_id,
											 uint16_t stream_id);

void bfcp_interface_set_floorctrl_mode(bfcp_interface interface,
									   e_floorctrl_mode floorctrl_mode);

void bfcp_interface_set_is_passive(bfcp_interface interface,
								   bool is_passive);

void bfcp_interface_set_media_stream_str(bfcp_interface interface,
										 char* media_stream_str);

/*! \brief Getter API for interface member */
uint16_t bfcp_interface_get_user_id(bfcp_interface interface);
uint16_t bfcp_interface_get_floor_stream_count(bfcp_interface interface);
uint16_t bfcp_interface_get_client_port(bfcp_interface interface);
uint64_t bfcp_interface_get_conf_id(bfcp_interface interface);
floor_stream_mapping_t *bfcp_interface_get_floor_stream_mapping(bfcp_interface interface);
e_floorctrl_mode bfcp_interface_get_floorctrl_mode(bfcp_interface interface);
bool bfcp_interface_get_is_passive(bfcp_interface interface);
char *bfcp_interface_get_uuid(bfcp_interface interface);
char *bfcp_interface_get_media_stream_str(bfcp_interface interface);
char *bfcp_interface_get_client_address(bfcp_interface interface);


/*! \brief API to assign confID and userID */
uint16_t bfcp_get_user_id();
uint64_t bfcp_get_conf_id();

/*! \brief API to get maximum value for any data type */
uint64_t get_max_val(uint32_t);