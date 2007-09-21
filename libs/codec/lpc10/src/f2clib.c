/*

$Log: f2clib.c,v $
Revision 1.2  2004/05/05 13:25:38  rjongbloed
Fixed clearly incorrect code (dived by zero) found with an MSVC warning

Revision 1.1  2004/05/04 11:16:42  csoutheren
Initial version

Revision 1.3  2004/02/17 09:21:45  csoutheren
Fix for GM bug 134591

Revision 1.2  2002/02/15 03:57:55  yurik
Warnings removed during compilation, patch courtesy of Jehan Bing, jehan@bravobrava.com

Revision 1.1  2000/06/05 04:45:12  robertj
Added LPC-10 2400bps codec

 * Revision 1.1  1996/08/19  22:32:10  jaf
 * Initial revision
 *

*/

/*
 * f2clib.c
 *
 * SCCS ID:  @(#)f2clib.c 1.2 96/05/19
 */

#include "f2c.h"

#ifdef KR_headers
integer pow_ii(ap, bp) integer *ap, *bp;
#else
integer pow_ii(integer *ap, integer *bp)
#endif
{
	integer pow, x, n;
	unsigned long u;

	x = *ap;
	n = *bp;

	if (n <= 0) {
		if (n == 0 || x == 1)
			return 1;
		if (x != -1)
			return x != 0 ? 1/x : 0;
		n = -n;
		}
	u = n;
	for(pow = 1; ; )
		{
		if(u & 01)
			pow *= x;
		if(u >>= 1)
			x *= x;
		else
			break;
		}
	return(pow);
	}



#ifdef KR_headers
double r_sign(a,b) real *a, *b;
#else
double r_sign(real *a, real *b)
#endif
{
double x;
x = (*a >= 0 ? *a : - *a);
return( *b >= 0 ? x : -x);
}



#ifdef KR_headers
double floor();
integer i_nint(x) real *x;
#else
#undef abs
#include "math.h"
integer i_nint(real *x)
#endif
{
return (integer)( (*x)>=0 ?
	floor(*x + .5) : -(floor(.5 - *x)) );
}
