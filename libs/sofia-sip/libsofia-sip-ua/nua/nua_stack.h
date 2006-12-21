/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2005 Nokia Corporation.
 *
 * Contact: Pekka Pessi <pekka.pessi@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef NUA_STACK_H
/** Defined when <nua_stack.h> has been included. */
#define NUA_STACK_H
/**@IFILE nua_stack.h 
 * @brief Sofia-SIP User Agent Engine - internal stack interface
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Kai Vehmanen <Kai.Vehmanen@nokia.com>
 *
 * @date Created: Wed Feb 14 17:09:44 2001 ppessi
 */

#ifndef SU_CONFIG_H
#include <su_config.h>
#endif

#ifndef SU_OS_NW_H
#include <sofia-sip/su_os_nw.h>
#endif
#ifndef SOA_H
#include "sofia-sip/soa.h"
#endif
#ifndef NTA_H
#include <sofia-sip/nta.h>
#endif
#ifndef AUTH_CLIENT_H
#include <sofia-sip/auth_client.h>
#endif
#ifndef NEA_H
#include <sofia-sip/nea.h>
#endif
#ifndef NUA_H
#include <sofia-sip/nua.h>
#endif

#define SU_LOG (nua_log)
#include <sofia-sip/su_debug.h>

#ifndef NUA_DIALOG_H
#define NUA_OWNER_T struct nua_handle_s
#include <nua_dialog.h>
#endif

SOFIA_BEGIN_DECLS

#if HAVE_SIGCOMP
#include <sigcomp.h>
#endif

#ifndef NUA_PARAMS_H
#include <nua_params.h>
#endif

typedef struct event_s event_t;

#define       NONE ((void *)-1)

typedef struct register_usage nua_registration_t;

#define \
  NH_ACTIVE_MEDIA_TAGS(include, soa)					\
  TAG_IF((include) && (soa) && soa_is_audio_active(soa) >= 0,		\
	 SOATAG_ACTIVE_AUDIO(soa_is_audio_active(soa))),		\
  TAG_IF((include) && (soa) && soa_is_video_active(soa) >= 0,		\
	 SOATAG_ACTIVE_VIDEO(soa_is_video_active(soa))),		\
  TAG_IF((include) && (soa) && soa_is_image_active(soa) >= 0,		\
	 SOATAG_ACTIVE_IMAGE(soa_is_image_active(soa))),		\
  TAG_IF((include) && (soa) && soa_is_chat_active(soa) >= 0,		\
	 SOATAG_ACTIVE_CHAT(soa_is_chat_active(soa)))

#define \
  NH_REMOTE_MEDIA_TAGS(include, soa)					\
  TAG_IF((include) && (soa) && soa_is_remote_audio_active(soa) >= 0,	\
	 SOATAG_ACTIVE_AUDIO(soa_is_remote_audio_active(soa))),		\
  TAG_IF((include) && (soa) && soa_is_remote_video_active(soa) >= 0,	\
	 SOATAG_ACTIVE_VIDEO(soa_is_remote_video_active(soa))),		\
  TAG_IF((include) && (soa) && soa_is_remote_image_active(soa) >= 0,	\
	 SOATAG_ACTIVE_IMAGE(soa_is_remote_image_active(soa))),		\
  TAG_IF((include) && (soa) && soa_is_remote_chat_active(soa) >= 0,	\
	 SOATAG_ACTIVE_CHAT(soa_is_remote_chat_active(soa)))

/** NUA handle. 
 *
 */
struct nua_handle_s 
{
  su_home_t       nh_home[1];	/**< Memory home  */
  nua_handle_t   *nh_next;
  nua_handle_t  **nh_prev;

  nua_t        	 *nh_nua;	/**< Pointer to NUA object  */
  void           *nh_valid;
  nua_hmagic_t 	 *nh_magic;	/**< Application context */

  tagi_t         *nh_tags;	/**< Initial tags */
  tagi_t         *nh_ptags;	/**< Initial parameters */

  nua_handle_t   *nh_identity;	/**< Identity */

  nua_handle_preferences_t *nh_prefs; /**< Preferences */

  /* Handle type is determined by special event and flags. */
  nua_event_t     nh_special;	/**< Special event */
  unsigned        nh_has_invite:1;     /**< Has call */
  unsigned        nh_has_subscribe:1;  /**< Has watcher */
  unsigned        nh_has_notify:1;     /**< Has notifier */
  unsigned        nh_has_register:1;   /**< Has registration */

  /* Call status */
  unsigned        nh_active_call:1;
  unsigned        nh_hold_remote:1;

  unsigned        nh_ref_by_stack:1;	/**< Has stack used the handle? */
  unsigned        nh_ref_by_user:1;	/**< Has user used the handle? */
  unsigned        nh_init:1;	        /**< Handle has been initialized */
  unsigned        nh_used_ptags:1;	/**< Ptags has been used */
  unsigned :0;

  nua_dialog_state_t nh_ds[1];

  auth_client_t  *nh_auth;	/**< Authorization objects */

  soa_session_t  *nh_soa;	/**< Media session */

  struct nua_referral {
    nua_handle_t  *ref_handle;	/**< Referring handle */
    sip_event_t   *ref_event;	/**< Event used with NOTIFY */
  } nh_referral[1];

  nea_server_t   *nh_notifier;	/**< SIP notifier */
};

#define NH_IS_VALID(nh) ((nh) && (nh)->nh_valid)

#define NH_STATUS(nh) \
  (nh)->nh_status, \
  (nh)->nh_phrase, \
  SIPTAG_WARNING_STR(nh->nh_warning)

#define NH_IS_DEFAULT(nh) ((nh) == (nh)->nh_nua->nua_handles)

static inline
int nh_is_special(nua_handle_t *nh)
{
  return nh == NULL || nh->nh_special;
}

extern char const nua_internal_error[];

#define NUA_INTERNAL_ERROR 900, nua_internal_error

struct nua_s {
  su_home_t            nua_home[1];

  /* API (client) side */
  su_root_t    	      *nua_api_root;
  su_clone_r   	       nua_clone;
  su_task_r            nua_client;

  su_network_changed_t *nua_nw_changed;

  nua_callback_f       nua_callback;
  nua_magic_t         *nua_magic;

  nua_saved_event_t    nua_current[1];
  nua_saved_event_t    nua_signal[1];

  /* Engine state flags */
  unsigned             nua_shutdown_started:1; /**< Shutdown initiated */
  unsigned             nua_shutdown_final:1; /**< Shutdown is complete */
  unsigned :0;
  
  /**< Used by stop-and-wait args calls */
  tagi_t const        *nua_args;

  /**< Local SIP address. Contents are kept around for ever. */
  sip_from_t          nua_from[1];

  /* Protocol (server) side */

  nua_registration_t *nua_registrations; /**< Active registrations */

  /* Constants */
  sip_accept_t       *nua_invite_accept; /* What we accept for invite */

  su_root_t          *nua_root;
  su_task_r           nua_server;
  nta_agent_t        *nua_nta;
  su_timer_t         *nua_timer;

  void         	      *nua_sip_parser;

  sip_time_t           nua_shutdown;

  /* Route */
  sip_service_route_t *nua_service_route;

  /* User-agent parameters */
  unsigned             nua_media_enable:1;

  unsigned     	       :0;

#if HAVE_SMIME		/* Start NRC Boston */
  sm_object_t          *sm;
#endif                  /* End NRC Boston */

  nua_handle_t        *nua_handles;
  nua_handle_t       **nua_handles_tail;
};

#define nua_dhandle    nua_handles

#if HAVE_FUNC
#define enter (void)SU_DEBUG_9(("nua: %s: entering\n", __func__))
#define nh_enter (void)SU_DEBUG_9(("nua %s(%p): entering\n", __func__, nh))
#elif HAVE_FUNCTION
#define enter (void)SU_DEBUG_9(("nua: %s: entering\n", __FUNCTION__))
#define nh_enter (void)SU_DEBUG_9(("nua %s(%p): entering\n", __FUNCTION__, nh))
#define __func__ __FUNCTION__
#else
#define enter ((void)0)
#define nh_enter ((void)0)
#define __func__ "nua"
#endif

/* Internal prototypes */
int  nua_stack_init(su_root_t *root, nua_t *nua);
void nua_stack_deinit(su_root_t *root, nua_t *nua);
void nua_stack_signal(nua_t *nua, su_msg_r msg, event_t *e);

int nua_stack_init_transport(nua_t *nua, tagi_t const *tags);

int nua_stack_init_registrations(nua_t *nua);

nua_registration_t *nua_registration_by_aor(nua_registration_t const *list,
					    sip_from_t const *aor,
					    url_t const *remote_uri,
					    int only_default);

sip_contact_t const *nua_registration_contact(nua_registration_t const *nr);

int nua_registration_process_request(nua_registration_t *nr,
				     nta_incoming_t *irq,
				     sip_t const *sip);

void nua_stack_post_signal(nua_handle_t *nh, nua_event_t event, 
			   tag_type_t tag, tag_value_t value, ...);

typedef int nua_stack_signal_handler(nua_t *, 
				     nua_handle_t *, 
				     nua_event_t, 
				     tagi_t const *);

nua_stack_signal_handler 
  nua_stack_set_params, nua_stack_get_params,
  nua_stack_register, 
  nua_stack_invite, nua_stack_ack, nua_stack_cancel, 
  nua_stack_bye, nua_stack_info, nua_stack_update, 
  nua_stack_options, nua_stack_publish, nua_stack_message, 
  nua_stack_subscribe, nua_stack_notify, nua_stack_refer,
  nua_stack_method;

#define UA_EVENT1(e, statusphrase) \
  nua_stack_event(nua, nh, NULL, e, statusphrase, TAG_END())

#define UA_EVENT2(e, status, phrase)			\
  nua_stack_event(nua, nh, NULL, e, status, phrase, TAG_END())

#define UA_EVENT3(e, status, phrase, tag)			\
  nua_stack_event(nua, nh, NULL, e, status, phrase, tag, TAG_END())

int nua_stack_event(nua_t *nua, nua_handle_t *nh, msg_t *msg,
		    nua_event_t event, int status, char const *phrase,
		    tag_type_t tag, tag_value_t value, ...);

nua_handle_t *nh_create_handle(nua_t *nua, nua_hmagic_t *hmagic,
			       tagi_t *tags);

nua_handle_t *nua_stack_incoming_handle(nua_t *nua,
					nta_incoming_t *irq,
					sip_t const *sip,
					int create_dialog);

enum { create_dialog = 1 };

int nua_stack_init_handle(nua_t *nua, nua_handle_t *nh, 
			  tag_type_t tag, tag_value_t value, ...);

enum nh_kind {
  nh_has_nothing,
  nh_has_invite,
  nh_has_subscribe,
  nh_has_notify,
  nh_has_register,
  nh_has_streaming
};

int nua_stack_set_handle_special(nua_handle_t *nh,
				 enum nh_kind kind,
				 nua_event_t special);

int nua_handle_save_tags(nua_handle_t *h, tagi_t *tags);

void nh_destroy(nua_t *nua, nua_handle_t *nh);

nua_handle_t *nh_validate(nua_t *nua, nua_handle_t *maybe);

sip_replaces_t *nua_stack_handle_make_replaces(nua_handle_t *handle, 
					       su_home_t *home,
					       int early_only);

nua_handle_t *nua_stack_handle_by_replaces(nua_t *nua,
					   sip_replaces_t const *r);

/* ---------------------------------------------------------------------- */

int nua_stack_set_defaults(nua_handle_t *nh, nua_handle_preferences_t *nhp);

int nua_stack_set_from(nua_t *, int initial, tagi_t const *tags);

int nua_stack_init_instance(nua_handle_t *nh, tagi_t const *tags);

int nua_stack_process_request(nua_handle_t *nh,
			      nta_leg_t *leg,
			      nta_incoming_t *irq,
			      sip_t const *sip);

int nua_stack_process_response(nua_handle_t *nh,
			       nua_client_request_t *cr,
			       nta_outgoing_t *orq,
			       sip_t const *sip,
			       tag_type_t tag, tag_value_t value, ...);

int nua_stack_launch_network_change_detector(nua_t *nua);

msg_t *nua_creq_msg(nua_t *nua, nua_handle_t *nh,
		    nua_client_request_t *cr,
		    int restart, 
		    sip_method_t method, char const *name,
		    tag_type_t tag, tag_value_t value, ...);

int nua_tagis_have_contact_tag(tagi_t const *t);

int nua_creq_check_restart(nua_handle_t *nh,
			   nua_client_request_t *cr,
			   nta_outgoing_t *orq,
			   sip_t const *sip,
			   nua_creq_restart_f *f);

int nua_creq_restart_with(nua_handle_t *nh,
			  nua_client_request_t *cr,
			  nta_outgoing_t *orq,
			  int status, char const *phrase,
			  nua_creq_restart_f *f, 
			  tag_type_t tag, tag_value_t value, ...);

int nua_creq_save_restart(nua_handle_t *nh,
			  nua_client_request_t *cr,
			  nta_outgoing_t *orq,
			  int status, char const *phrase,
			  nua_creq_restart_f *f);

int nua_creq_restart(nua_handle_t *nh,
		     nua_client_request_t *cr,
		     nta_response_f *cb,
		     tagi_t *tags);

void nua_creq_deinit(nua_client_request_t *cr, nta_outgoing_t *orq);

sip_contact_t const *nua_stack_get_contact(nua_registration_t const *nr);

int nua_registration_add_contact_to_request(nua_handle_t *nh,
					    msg_t *msg, 
					    sip_t *sip,
					    int add_contact,
					    int add_service_route);

int nua_registration_add_contact_to_response(nua_handle_t *nh,
					     msg_t *msg,
					     sip_t *sip,
					     sip_record_route_t const *,
					     sip_contact_t const *remote);

msg_t *nh_make_response(nua_t *nua, nua_handle_t *nh, 
			nta_incoming_t *irq,
			int status, char const *phrase,
			tag_type_t tag, tag_value_t value, ...);


typedef int nua_stack_process_request_t(nua_t *nua,
					nua_handle_t *nh,
					nta_incoming_t *irq,
					sip_t const *sip);

nua_stack_process_request_t nua_stack_process_invite;
nua_stack_process_request_t nua_stack_process_info;
nua_stack_process_request_t nua_stack_process_update;
nua_stack_process_request_t nua_stack_process_bye;
nua_stack_process_request_t nua_stack_process_message;
nua_stack_process_request_t nua_stack_process_options;
nua_stack_process_request_t nua_stack_process_publish;
nua_stack_process_request_t nua_stack_process_subscribe;
nua_stack_process_request_t nua_stack_process_notify;
nua_stack_process_request_t nua_stack_process_refer;
nua_stack_process_request_t nua_stack_process_unknown;
nua_stack_process_request_t nua_stack_process_register;
nua_stack_process_request_t nua_stack_process_method;

nua_client_request_t
  *nua_client_request_pending(nua_client_request_t const *),
  *nua_client_request_restarting(nua_client_request_t const *),
  *nua_client_request_by_orq(nua_client_request_t const *cr,
			     nta_outgoing_t const *orq);

nua_server_request_t *nua_server_request(nua_t *nua,
					 nua_handle_t *nh,
					 nta_incoming_t *irq,
					 sip_t const *sip,
					 nua_server_request_t *sr,
					 size_t size,
					 nua_server_respond_f *respond,
					 int create_dialog);

int nua_stack_server_event(nua_t *nua,
			   nua_server_request_t *sr,
			   nua_event_t event,
			   tag_type_t tag, tag_value_t value, ...);

/* ---------------------------------------------------------------------- */

#ifndef SDP_MIME_TYPE
#define SDP_MIME_TYPE nua_application_sdp
#endif

extern char const nua_application_sdp[];

/* ---------------------------------------------------------------------- */

#define SIP_METHOD_UNKNOWN sip_method_unknown, NULL

/* Private tags */
#define NUTAG_ADD_CONTACT(v) _nutag_add_contact, tag_bool_v(v)
extern tag_typedef_t _nutag_add_contact;

#define NUTAG_ADD_CONTACT_REF(v) _nutag_add_contact_ref, tag_bool_vr(&v)
extern tag_typedef_t _nutag_add_contact_ref;

#define NUTAG_COPY(v) _nutag_copy, tag_bool_v(v)
extern tag_typedef_t _nutag_copy;

#define NUTAG_COPY_REF(v) _nutag_copy_ref, tag_bool_vr(&v)
extern tag_typedef_t _nutag_copy_ref;

/* ---------------------------------------------------------------------- */

typedef unsigned longlong ull;

#define SET_STATUS(_status, _phrase) status = _status, phrase = _phrase

#define SET_STATUS2(_status, _phrase) status = _status, phrase = _phrase

/* This is an "interesting" macro:
 * x is a define expanding to <i>num, str</i>.
 * @a num is assigned to variable status, @a str to variable phrase.
 * Macro SET_STATUS1 expands to two comma-separated expressions that are
 * also usable as function arguments.
 */
#define SET_STATUS1(x) ((status = x), status), (phrase = ((void)x))

/* ---------------------------------------------------------------------- */
/* Application side prototypes */

void nua_signal(nua_t *nua, nua_handle_t *nh, msg_t *msg, int always,
		nua_event_t event, int status, char const *phrase,
		tag_type_t tag, tag_value_t value, ...);

void nua_event(nua_t *root_magic, su_msg_r sumsg, event_t *e);

SOFIA_END_DECLS

#endif /* NUA_STACK_H */
