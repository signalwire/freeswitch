/*
 * Copyright (c) 2009, Sangoma Technologies
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

/*! \brief opaque profile timer structure */
struct profile_timer;
typedef struct profile_timer switch_profile_timer_t;

/*! 
 * \brief create a new profile timer
 * \return profile timer structure previously created with new_profile_timer, NULL on error
 */
SWITCH_DECLARE(switch_profile_timer_t *) switch_new_profile_timer(void);

/*! 
 * \brief provides the percentage of idle system time
 * \param p profile timer structure previously created with new_profile_timer
 * \param pointer to store the percentage of idle time
 * \return false on error true for success
 * \note on error idle_percentage will be set to 100
 */
SWITCH_DECLARE(switch_bool_t) switch_get_system_idle_time(switch_profile_timer_t *p, double *idle_percentage);


/*! 
 * \brief Deletes profile timer
 */
SWITCH_DECLARE(void) switch_delete_profile_timer(switch_profile_timer_t **p);

