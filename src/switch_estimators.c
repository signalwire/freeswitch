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
 * switch_estimators.c -- Estimators and Detectors (try to read into the future: packet loss, jitter, RTT, etc)
 *
 */

#include <switch.h>

#ifndef _MSC_VER
#include <switch_private.h>
#endif
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#undef PACKAGE_BUGREPORT
#undef VERSION
#undef PACKAGE
#undef inline
#include <switch_types.h>

#define KALMAN_SYSTEM_MODELS 3 /*loss, jitter, rtt*/
#define EST_LOSS 0
#define EST_JITTER 1
#define EST_RTT 2

/* This function initializes the Kalman System Model
 *
 * xk+1 = A*xk + wk
 * zk = H*xk + vk
 * xk = state variable (must exist in physical world - measurable )
 * zk = measurment
 * wk,vk - white noise
 * A = state trasition matrix , (n x n )  matrix
 * H = state-to-measurment matrix , ( n x n ) matrix
 * Noise covariance:
 * Q:  Covariance matrix of wk, ( n x n ) diagonal matrix
 * R:  Covariance matrix of vk , ( m x m ) diagonal matrix
 * R: if you want to be affected less by the measurement and get the estimate with less variation, increase R
 * Q: if you want to be affected more by the measurement and get the estimate with more variation, decrease Q
 *
 * (Phil Kim book)
 *
 */
SWITCH_DECLARE(void) switch_kalman_init(kalman_estimator_t *est, float Q, float R)
{
	est -> val_estimate_last = 0 ;
	est -> P_last = 0;
	est -> Q = Q; /*accuracy of system model */ /* SYSTEM MODEL: TO BE DEDUCTED */
	est -> R = R; /*accuracy of measurement*/ /* SYSTEM MODEL: TO BE DEDUCTED */
	est -> K = 0;
	est -> val_estimate = 0 ;
	est -> val_measured = 0 ; // [0-100 %] or [0-5000] or [0-2sec]
}

/*
CUSUM Kalman functions to detect sudden change over a predefined threshold.

y(t) = sampled value (RTT, packet loss)
x(t) = desired value

Model:
x(t+1) = x(t) + delta(t)*v(t)
y(t) = x(t) + e(t)

Noisy characteristic of the samples captured by measurement noise e(t) with variance Re.
The step changes in the desired value x(t) are modeled as the process noise v(t)
with variance Rv and the discrete variable delta(t).
If a change occurs at time t, then delta(t) = 1 otherwise delta(t) = 0.

e(t) = y(t) - avg(x(t-1))                           (innovation)
K(t) = P(t-1)/(P(t-1) + Re)
avg(x(t)) = avg(x(t-1)) + K(t)*e(t)
P(t) = (1-K(t))P(t-1) + delta(t-1)*Rv
g(t) = max(g(t-1) + e(t) - epsilon, 0)              (upward drift)
g_down(t) = max(g_down(t-1) - e(t) - epsilon, 0)    (downward drift)
if g(t) > h then
	alarm, g(t) = 0
if g_down(t) > h then
	g_down(t) = 0
if g(t) or g_down(t) crossed h then
	delta(t) = 1   (re-inflate P so that avg(x) follows the new level)
else
	delta(t) = 0
endif

Only upward changes are reported to the caller. Downward changes are detected
so that the baseline avg(x) follows a level that has gone down again; without
this the baseline stays at the old, higher level and a later increase of the
same size is never seen.

Re and Rv are the running sample variance of y (Welford's method).
The 'avg' argument is no longer used and is kept for API compatibility.
*/
SWITCH_DECLARE(switch_bool_t) switch_kalman_cusum_init(cusum_kalman_detector_t *detect_change, float epsilon,float h)
{
	cusum_kalman_detector_t *detector_change = detect_change;


	if (epsilon < 0 || h < 0) {
		return FALSE;
	}

	detector_change -> val_estimate_last = 0;
	detector_change -> val_desired_last = 0;
	detector_change -> P_last = 0;
	detector_change -> K_last = 0;
	detector_change -> delta = 0;
	detector_change -> measurement_noise_e = 0;
	detector_change -> variance_Re = 0;
	detector_change -> measurement_noise_v = 0;
	detector_change -> variance_Rv = 0;
	detector_change -> g_last = 0;
	/*per system model*/
	detector_change -> epsilon = epsilon;
	detector_change -> h = h;
	/*variance*/
	detector_change -> last_average = 0;
	detector_change -> last_q = 0;
	detector_change -> N = 0;
	detector_change -> g_down_last = 0;
	return TRUE;
}

SWITCH_DECLARE (switch_bool_t) switch_kalman_cusum_detect_change(cusum_kalman_detector_t * detector, float measurement, float avg)
{
	float K;
	float P;
	float g;
	float g_down;
	float innovation;
	float desired_val;
	float prev_average;
	switch_bool_t increase = SWITCH_FALSE;
	switch_bool_t decrease = SWITCH_FALSE;

	(void)avg;

	/* running sample variance of the measurements (Welford) */
	detector->N++;
	prev_average = detector->last_average;
	detector->last_average = prev_average + (measurement - prev_average) / detector->N;
	detector->last_q += (measurement - prev_average) * (measurement - detector->last_average);

	detector->variance_Re = (detector->N > 1) ? detector->last_q / (detector->N - 1) : 0;
	detector->variance_Rv = detector->variance_Re;

	if (detector->variance_Re <= 0) {
		/* all samples identical so far: nothing has changed */
		return SWITCH_FALSE;
	}

	innovation = measurement - detector->val_desired_last;
	K = detector->P_last / (detector->P_last + detector->variance_Re);
	desired_val = detector->val_desired_last + K * innovation;
	P = (1 - K) * detector->P_last + detector->delta * detector->variance_Rv;
	detector->measurement_noise_e = innovation;

	g = detector->g_last + innovation - detector->epsilon;
	if (g < 0) {
		g = 0;
	}

	g_down = detector->g_down_last - innovation - detector->epsilon;
	if (g_down < 0) {
		g_down = 0;
	}

	if (g > detector->h) {
		increase = SWITCH_TRUE;
		g = 0;
	}

	if (g_down > detector->h) {
		decrease = SWITCH_TRUE;
		g_down = 0;
	}

	detector->delta = (increase || decrease) ? 1 : 0;

	detector->g_last = g;
	detector->g_down_last = g_down;
	detector->P_last = P;
	detector->val_desired_last = desired_val;

	return increase;
}

/* Kalman filter abstract ( measure and estimate 1 single value per system model )
 * Given the measurment and the system model  together with the current state ,
 * the function puts an estimate in the estimator struct */
SWITCH_DECLARE(switch_bool_t) switch_kalman_estimate(kalman_estimator_t * est, float measurement, int system_model)
{
	/*system model can be about: loss, jitter, rtt*/
	float val_estimate;
	float val_temp_est = est->val_estimate_last;
	float P_temp = est->P_last + est->Q;

	if (system_model >= KALMAN_SYSTEM_MODELS) {
		return SWITCH_FALSE ;
	}

	/*sanitize input a little bit, just in case */
	if (system_model == EST_LOSS )  {
		if ((measurement > 100) || (measurement < 0)) {
			return SWITCH_FALSE ;
		}
	}

	if (system_model == EST_JITTER)  {
		if ((measurement > 10000) || (measurement < 0)) {
			return SWITCH_FALSE;
		}
	}

	if (system_model == EST_RTT)  {
		if ((measurement > 2 ) || (measurement < 0)) {
			return SWITCH_FALSE;
		}
	}

	/* calculate the Kalman gain */
	est->K = P_temp * (1.0/(P_temp + est->R));
	/* real life measurement */
	est->val_measured = measurement ;
	val_estimate = val_temp_est + est->K * (est->val_measured - val_temp_est);
	est->P = (1 - est->K) * P_temp;
	/*update lasts*/
	est->P_last = est->P;
	/* save the estimated value (future) */
	est->val_estimate_last = val_estimate;
	return SWITCH_TRUE;
}

SWITCH_DECLARE(switch_bool_t) switch_kalman_is_slow_link(kalman_estimator_t * est_loss, kalman_estimator_t * est_rtt)
{
	float thresh_packet_loss = 5; /* % */
	float thresh_rtt = 0.8 ; /*seconds*/

	if ((est_loss->val_estimate_last > thresh_packet_loss) &&
				(est_rtt->val_estimate_last > thresh_rtt )) {
		return SWITCH_TRUE;
	}

	return SWITCH_FALSE;
}

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

