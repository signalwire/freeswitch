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


/** Enumeration of commands */
typedef enum {
	MPF_COMMAND_ADD,     /**< add termination to context */
	MPF_COMMAND_MODIFY,  /**< modify termination properties */
	MPF_COMMAND_SUBTRACT,/**< subtract termination from context */ 
	MPF_COMMAND_MOVE     /**< move termination to another context */
} mpf_command_type_e;

/** MPF message declaration */
typedef struct mpf_message_t mpf_message_t;

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
	/** Termination type dependent descriptor */
	void              *descriptor;
};

APT_END_EXTERN_C

#endif /*__MPF_MESSAGE_H__*/
