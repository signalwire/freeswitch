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

/**@CFILE test_extension.c
 * @brief NUA-12: Test extension methods.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Mon Nov 13 15:37:05 EET 2006
 */

#include "config.h"

#include "test_nua.h"

#include <sofia-sip/su_tag_class.h>

#if HAVE_FUNC
#elif HAVE_FUNCTION
#define __func__ __FUNCTION__
#else
#define __func__ "test_extension"
#endif


int respond_to_extension(CONDITION_PARAMS)
{
  msg_t *with = nua_current_request(nua);

  if (!(check_handle(ep, call, nh, SIP_500_INTERNAL_SERVER_ERROR)))
    return 0;

  save_event_in_list(ctx, event, ep, call);

  switch (event) {
  case nua_i_method:
    RESPOND(ep, call, nh, SIP_200_OK,
	    NUTAG_WITH(with),
	    SIPTAG_SUBJECT_STR("extended"),
	    TAG_END());
    return 1;
  default:
    return 0;
  }
}

int test_extension(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a,  *b = &ctx->b;
  struct call *a_call = a->call, *b_call = b->call;
  struct event *e;
  sip_t const *sip;


/* Test for EXTENSION

   A			B
   |------EXTENSION---->|
   |<--------501--------| (method not recognized)
   |			|
   |------EXTENSION---->|
   |<-------200---------| (method allowed, responded)
   |			|
*/

  if (print_headings)
    printf("TEST NUA-13.1: EXTENSION\n");


  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  /* Test first without NUTAG_METHOD() */
  METHOD(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 TAG_END());

  run_ab_until(ctx, -1, save_until_final_response, -1, NULL);

  /* Client events:
     nua_method(), nua_r_method
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_r_method);
  TEST(e->data->e_status, 900);	/* Internal error */
  TEST_1(!e->data->e_msg);
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);
  nua_handle_destroy(a_call->nh), a_call->nh = NULL;

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  METHOD(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 NUTAG_METHOD("EXTENSION"),
	 TAG_END());

  run_ab_until(ctx, -1, save_until_final_response, -1, NULL);

  /* Client events:
     nua_method(), nua_r_method
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_r_method);
  TEST(e->data->e_status, 501);
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);
  nua_handle_destroy(a_call->nh), a_call->nh = NULL;

  free_events_in_list(ctx, b->events);
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  nua_set_params(b->nua, NUTAG_ALLOW("EXTENSION"), TAG_END());

  run_b_until(ctx, nua_r_set_params, until_final_response);

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  METHOD(a, a_call, a_call->nh,
	 TAG_IF(!ctx->proxy_tests, NUTAG_URL(b->contact->m_url)),
	 NUTAG_METHOD("EXTENSION"),
	 TAG_END());

  run_ab_until(ctx, -1, save_until_final_response, -1, respond_to_extension);

  /* Client events:
     nua_method(), nua_r_method
  */
  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_r_method);
  TEST(e->data->e_status, 200);
  TEST_1(sip = sip_object(e->data->e_msg));
  TEST_1(!e->next);

  /*
   Server events:
   nua_i_method
  */
  TEST_1(e = b->events->head); TEST_E(e->data->e_event, nua_i_method);
  TEST(e->data->e_status, 100);
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);
  nua_handle_destroy(a_call->nh), a_call->nh = NULL;

  free_events_in_list(ctx, b->events);
  nua_handle_destroy(b_call->nh), b_call->nh = NULL;

  nua_set_params(b->nua,
		 SIPTAG_ALLOW(b->allow),
		 NUTAG_APPL_METHOD(NULL),
		 NUTAG_APPL_METHOD(b->appl_method),
		 TAG_END());
  run_b_until(ctx, nua_r_set_params, until_final_response);

  if (print_headings)
    printf("TEST NUA-13.1: PASSED\n");
  END();
}
