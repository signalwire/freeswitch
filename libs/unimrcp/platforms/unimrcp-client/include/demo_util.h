/*
 * Copyright 2008-2015 Arsen Chaloyan
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

#ifndef DEMO_UTIL_H
#define DEMO_UTIL_H

/**
 * @file demo_util.h
 * @brief Demo MRCP Utilities
 */ 

#include "mrcp_application.h"

APT_BEGIN_EXTERN_C

/** Create demo MRCP message (SPEAK request) */
mrcp_message_t* demo_speak_message_create(mrcp_session_t *session, mrcp_channel_t *channel, const apt_dir_layout_t *dir_layout);

/** Create demo MRCP message (DEFINE-GRAMMAR request) */
mrcp_message_t* demo_define_grammar_message_create(mrcp_session_t *session, mrcp_channel_t *channel, const apt_dir_layout_t *dir_layout);
/** Create demo MRCP message (RECOGNIZE request) */
mrcp_message_t* demo_recognize_message_create(mrcp_session_t *session, mrcp_channel_t *channel, const apt_dir_layout_t *dir_layout);

/** Create demo RTP termination descriptor */
mpf_rtp_termination_descriptor_t* demo_rtp_descriptor_create(apr_pool_t *pool);

APT_END_EXTERN_C

#endif /* DEMO_UTIL_H */
