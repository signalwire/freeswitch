/*
 * Copyright 2008 Arsen Chaloyan
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __MPF_MESSAGE_H__
#define __MPF_MESSAGE_H__

/**
 * @file mpf_message.h
 * @brief Media Processing Framework Message Definitions
 */ 

#include "mpf_types.h"

APT_BEGIN_EXTERN_C

/** Max number of messages grouped in a container */
#define MAX_MPF_MESSAGE_COUNT 5

/** Enumeration of MPF message types */
typedef enum {
	MPF_MESSAGE_TYPE_REQUEST,  /**< request message */
	MPF_MESSAGE_TYPE_RESPONSE, /**< response message */
	MPF_MESSAGE_TYPE_EVENT     /**< event message */
} mpf_message_type_e;

/** Enumeration of MPF status codes */
typedef enum {
	MPF_STATUS_CODE_SUCCESS,  /**< indicates success */
	MPF_STATUS_CODE_FAILURE   /**< indicates failure */
} mpf_status_code_e;


/** Enumeration of MPF commands */
typedef enum {
	MPF_ADD_TERMINATION,     /**< add termination to context */
	MPF_MODIFY_TERMINATION,  /**< modify termination properties */
	MPF_SUBTRACT_TERMINATION,/**< subtract termination from context */
	MPF_ADD_ASSOCIATION,     /**< add association between terminations */
	MPF_REMOVE_ASSOCIATION,  /**< remove association between terminations */
	MPF_RESET_ASSOCIATIONS,  /**< reset associations among terminations (also destroy topology) */
	MPF_APPLY_TOPOLOGY,      /**< apply topology based on assigned associations */
	MPF_DESTROY_TOPOLOGY     /**< destroy applied topology */
} mpf_command_type_e;

/** MPF message declaration */
typedef struct mpf_message_t mpf_message_t;
/** MPF message container declaration */
typedef struct mpf_message_container_t mpf_message_container_t;

/** MPF message definition */
struct mpf_message_t {
	/** Message type (request/response/event) */
	mpf_message_type_e message_type;
	/** Command identifier (add, modify, subtract, ...) */
	mpf_command_type_e command_id;
	/** Status code used in responses */
	mpf_status_code_e  status_code;

	/** Context */
	mpf_context_t     *context;
	/** Termination */
	mpf_termination_t *termination;
	/** Associated termination */
	mpf_termination_t *assoc_termination;
	/** Termination type dependent descriptor */
	void              *descriptor;
};

/** MPF message container definition */
struct mpf_message_container_t {
	/** Number of actual messages */
	apr_size_t    count;
	/** Array of messages */
	mpf_message_t messages[MAX_MPF_MESSAGE_COUNT];
};

APT_END_EXTERN_C

#endif /*__MPF_MESSAGE_H__*/
