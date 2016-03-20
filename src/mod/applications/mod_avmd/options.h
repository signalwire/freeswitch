/*
 * @brief   Options controlling avmd module.
 *
 * @author Eric des Courtis
 * @par    Modifications: Piotr Gregor < piotrek.gregor gmail.com >
 */


#ifndef __OPTIONS_H__
#define __OPTIONS_H__


/* #define AVMD_DEBUG 1 */

/* define/undef this to enable/disable reporting of beep
 * detection status after session ended */
#define AVMD_REPORT_STATUS 1

/* define/undefine this to enable/disable faster computation
 * of arcus cosine - table will be created mapping floats
 * to integers and returning arc cos values given these integer
 * indexes into table */
/* #define AVMD_FAST_MATH */

/* define/undefine this to classify avmd beep detection as valid
 * only when there is required number of consecutive elements
 * in the SMA buffer without reset */
#define AVMD_REQUIRE_CONTINUOUS_STREAK 1


#endif

