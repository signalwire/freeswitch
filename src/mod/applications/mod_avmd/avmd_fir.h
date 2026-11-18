/*
 * @brief   Filters.
 *
 * Contributor(s):
 *
 * Piotr Gregor <piotrgregor@rsyncme.org>
 *
 * @date 23 Mar 2016
 */


#ifndef __AVMD_FIR_H__
#define __AVMD_FIR_H__


#define AVMD_MAX(a, b) (a) > (b) ? (a) : (b)
#define AVMD_MEDIAN_FILTER(a, b, c) (a) > (b) ? ((a) > (c) ? \
				AVMD_MAX((b), (c)) : a) : ((b) > (c) ? AVMD_MAX((a), (c)) : (b))


#endif
