/*
 * @brief   Options controlling avmd module.
 *
 * @author Eric des Courtis
 *
 * Contributor(s):
 *
 * Piotr Gregor <piotrek.gregor gmail.com>:
 * Eric des Courtis <eric.des.courtis@benbria.com>
 */


#ifndef __AVMD_OPTIONS_H__
#define __AVMD_OPTIONS_H__


/* define/undefine this to enable/disable printing of avmd
 * intermediate computations to log */
/*#define AVMD_DEBUG*/

/* define/undef this to enable/disable reporting of beep
 * detection status after session ended */
#define AVMD_REPORT_STATUS

/* define/undefine this to enable/disable faster computation
 * of arcus cosine - table will be created mapping floats
 * to integers and returning arc cos values given these integer
 * indices into table */
/* #define AVMD_FAST_MATH */

/* define/undefine this to classify avmd beep detection as valid
 * only when there is required number of consecutive elements
 * in the SMA buffer without reset */
#define AVMD_REQUIRE_CONTINUOUS_STREAK

/* define number of samples to skip starting from the beginning
 * of the frame and after reset */
#define AVMD_SAMLPE_TO_SKIP_N 6

/* define/undefine this to enable/disable simplified estimation
 * of frequency based on approximation of sin(x) with (x)
 * in the range x=[0,PI/2] */
#define AVMD_SIMPLIFIED_ESTIMATION

/* define/undefine to enable/disable avmd on internal channel */
/*#define AVMD_INBOUND_CHANNEL*/

/* define/undefine to enable/disable avmd on external channel */
#define AVMD_OUTBOUND_CHANNEL


#endif /* __AVMD_OPTIONS_H__ */

