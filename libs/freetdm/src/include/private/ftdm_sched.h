/*
 * Copyright (c) 2010, Sangoma Technologies
 * Moises Silva <moy@sangoma.com>
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __FTDM_SCHED_H__
#define __FTDM_SCHED_H__

#include "freetdm.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FTDM_MICROSECONDS_PER_SECOND 1000000

typedef struct ftdm_sched ftdm_sched_t;
typedef void (*ftdm_sched_callback_t)(void *data);
typedef uint64_t ftdm_timer_id_t;

/*! \brief Create a new scheduling context */
FT_DECLARE(ftdm_status_t) ftdm_sched_create(ftdm_sched_t **sched, const char *name);

/*! \brief Run the schedule to find timers that are expired and run its callbacks */
FT_DECLARE(ftdm_status_t) ftdm_sched_run(ftdm_sched_t *sched);

/*! \brief Run the schedule in its own thread. Callbacks will be called in a core thread. You *must* not block there! */
FT_DECLARE(ftdm_status_t) ftdm_sched_free_run(ftdm_sched_t *sched);

/*! 
 * \brief Schedule a new timer 
 * \param sched The scheduling context (required)
 * \param name Timer name, typically unique but is not required to be unique, any null terminated string is fine (required)
 * \param callback The callback to call upon timer expiration (required)
 * \param data Optional data to pass to the callback
 * \param timer Timer id pointer to store the id of the newly created timer. It can be null
 *              if you do not need to know the id, but you need this if you want to be able 
 *              to cancel the timer with ftdm_sched_cancel_timer
 */
FT_DECLARE(ftdm_status_t) ftdm_sched_timer(ftdm_sched_t *sched, const char *name, 
		int ms, ftdm_sched_callback_t callback, void *data, ftdm_timer_id_t *timer);

/*! 
 * \brief Cancel the timer
 *        Note that there is a race between cancelling and triggering a timer.
 *        By the time you call this function the timer may be about to be triggered.
 *        This is specially true with timers in free run schedule.
 * \param sched The scheduling context (required)
 * \param timer The timer to cancel (required)
 */
FT_DECLARE(ftdm_status_t) ftdm_sched_cancel_timer(ftdm_sched_t *sched, ftdm_timer_id_t timer);

/*! \brief Destroy the context and all of the scheduled timers in it */
FT_DECLARE(ftdm_status_t) ftdm_sched_destroy(ftdm_sched_t **sched);

/*! 
 * \brief Calculate the time to the next timer and return it 
 * \param sched The sched context
 * \param timeto The pointer to store the next timer time in milliseconds
 */
FT_DECLARE(ftdm_status_t) ftdm_sched_get_time_to_next_timer(const ftdm_sched_t *sched, int32_t *timeto);

/*! \brief Global initialization, called just once, this is called by FreeTDM core, other users MUST not call it */
FT_DECLARE(ftdm_status_t) ftdm_sched_global_init(void);

/*! \brief Checks if the main scheduling thread is running */
FT_DECLARE(ftdm_bool_t) ftdm_free_sched_running(void);

/*! \brief Stop the main scheduling thread (if running) */
FT_DECLARE(ftdm_bool_t) ftdm_free_sched_stop(void);

#ifdef __cplusplus
} 
#endif

#endif

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
