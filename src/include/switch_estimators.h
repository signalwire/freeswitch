/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2015, Anthony Minessale II <anthm@freeswitch.org>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Dragos Oancea <droancea@yahoo.com>
 *
 * switch_estimators.h -- Estimators for Packet Loss, Jitter, RTT , etc
 *
 */


#ifndef SWITCH_ESTIMATORS_H
#define SWITCH_ESTIMATORS_H


#include <switch.h>


SWITCH_BEGIN_EXTERN_C

struct kalman_estimator_s {
	/* initial values for the Kalman filter  */
	float val_estimate_last ;
	float P_last ;
	/* the noise in the system:
	The amount of noise in your measurements and the state-transitions
	(e.g. the standard deviation of the signal noise, and how 'wrong' your simplified model
	of the state-transitions are) => These are Q and R matrices */
	float Q ; /* the process noise covariance matrix  */
	float R ; /* the measurement noise covariance matrix */
	float K; /*  P_temp * H^T * (H* P_temp * H^T + R)^-1  */
	float P; /*  the Kalman gain (calculated) */
	float val_estimate; /*  x_temp_est + K * (z_measured - H * x_temp_est) */
	float val_measured; /* the 'noisy' value we measured */
};

struct cusum_kalman_detector_s {
	/* initial values for the CUSUM Kalman filter  */
	float val_estimate_last;
	float val_desired_last;
	float P_last;
	float K_last;
	float delta;
	float measurement_noise_e;
	float variance_Re;
	float measurement_noise_v;
	float variance_Rv;
	float g_last;
	/*constants per model*/
	float epsilon;
	float h;
	/* for calculating variance */
	float last_average;
	float last_q;
	float N; /*how many samples we have so far (eg: how many RTCP we received, granted that we can calculate RTT for each one of them)*/
};

typedef struct kalman_estimator_s kalman_estimator_t;
typedef struct cusum_kalman_detector_s cusum_kalman_detector_t;

SWITCH_DECLARE(void) switch_kalman_init(kalman_estimator_t *est, float Q, float R);
SWITCH_DECLARE(switch_bool_t) switch_kalman_cusum_init(cusum_kalman_detector_t *detect_change, float epsilon,float h);
SWITCH_DECLARE(switch_bool_t) switch_kalman_estimate(kalman_estimator_t * est, float measurement, int system_model);
SWITCH_DECLARE (switch_bool_t) switch_kalman_cusum_detect_change(cusum_kalman_detector_t * detector, float measurement, float rtt_avg);
SWITCH_DECLARE(switch_bool_t) switch_kalman_is_slow_link(kalman_estimator_t * est_loss, kalman_estimator_t * est_rtt);

SWITCH_END_EXTERN_C
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
