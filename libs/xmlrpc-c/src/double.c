#include <assert.h>
#include <stdlib.h>
#include <float.h>

#include "xmlrpc-c/util.h"

#include "double.h"

typedef struct {
    char * bytes;
    char * next;
    char * end;
} buffer;


static void
bufferInit(buffer * const bufferP) {

    unsigned int const initialSize = 64;

    bufferP->bytes = malloc(initialSize);

    if (bufferP->bytes) {
        bufferP->next = bufferP->bytes;
        bufferP->end  = bufferP->bytes + initialSize;
    }
}



static void
bufferConcat(buffer * const bufferP,
             char     const newChar) {

    if (bufferP->bytes) {
        if (bufferP->next >= bufferP->end) {
            unsigned int const oldSize = bufferP->end - bufferP->bytes;
            unsigned int const newSize = oldSize + 64;
            bufferP->bytes = realloc(bufferP->bytes, newSize);
            bufferP->next = bufferP->bytes + oldSize;
            bufferP->end  = bufferP->bytes + newSize;
        }

        if (bufferP->bytes)
            *(bufferP->next++) = newChar;
    }
}



static char
digitChar(unsigned int const digitValue) {

    assert(digitValue < 10);

    return '0' + digitValue;
}



static void
floatWhole(double   const value,
           buffer * const formattedP,
           double * const formattedAmountP,
           double * const precisionP) {

    if (value < 1.0) {
        /* No digits to add to the whole part */
        *formattedAmountP = 0;
        *precisionP       = DBL_EPSILON;
    } else {
        double nonLeastAmount;
        double nonLeastPrecision;
        unsigned int leastValue;

        /* Add all digits but the least significant to *formattedP */

        floatWhole(value/10.0, formattedP, &nonLeastAmount,
                   &nonLeastPrecision);

        /* Add the least significant digit to *formattedP */

        if (nonLeastPrecision > 0.1) {
            /* We're down in the noise now; no point in showing any more
               significant digits (and we couldn't if we wanted to, because
               nonLeastPrecision * 10 might be more than 10 less than
               'value').
            */
            leastValue = 0;
        } else
            leastValue = (unsigned int)(value - nonLeastAmount * 10);

        bufferConcat(formattedP, digitChar(leastValue));
        
        *formattedAmountP = nonLeastAmount * 10 + leastValue;
        *precisionP       = nonLeastPrecision * 10;
    }        
}



static void
floatFractionPart(double   const value,
                  double   const wholePrecision,
                  buffer * const formattedP) {
/*----------------------------------------------------------------------------
   Serialize the part that comes after the decimal point, assuming there
   is something (nonzero) before the decimal point that uses up all but
   'wholePrecision' of the available precision.
-----------------------------------------------------------------------------*/
    double precision;
    double d;

    assert(value < 1.0);

    for (d = value, precision = wholePrecision;
         d > precision;
         precision *= 10) {

        unsigned int digitValue;

        d *= 10;
        digitValue = (unsigned int) d;

        d -= digitValue;

        assert(d < 1.0);

        bufferConcat(formattedP, digitChar(digitValue));
    }
}



static void
floatFraction(double   const value,
              buffer * const formattedP) {
/*----------------------------------------------------------------------------
   Serialize the part that comes after the decimal point, assuming there
   is nothing before the decimal point.
-----------------------------------------------------------------------------*/
    double precision;
    double d;

    assert(0.0 < value && value < 1.0);

    /* Do the leading zeroes, which eat no precision */

    for (d = value * 10; d < 1.0; d *= 10)
        bufferConcat(formattedP, '0');

    /* Now the significant digits */

    precision = DBL_EPSILON;

    while (d > precision) {
        unsigned int const digitValue = (unsigned int) d;

        bufferConcat(formattedP, digitChar(digitValue));

        d -= digitValue;

        assert(d < 1.0);

        d *= 10;
        precision *= 10;
    }
}



void
xmlrpc_formatFloat(xmlrpc_env *  const envP,
                   double        const value,
                   const char ** const formattedP) {

    double absvalue;
    buffer formatted;

    bufferInit(&formatted);

    if (value < 0.0) {
        bufferConcat(&formatted, '-');
        absvalue = - value;
    } else
        absvalue = value;

    if (absvalue >= 1.0) {
        double wholePart, fractionPart;
        double wholePrecision;

        floatWhole(absvalue, &formatted, &wholePart, &wholePrecision);

        fractionPart = absvalue - wholePart;

        if (fractionPart > wholePrecision) {
            bufferConcat(&formatted, '.');

            floatFractionPart(fractionPart, wholePrecision, &formatted);
        }    
    } else {
        bufferConcat(&formatted, '0');

        if (absvalue > 0.0) {
            bufferConcat(&formatted, '.');
            floatFraction(absvalue, &formatted);
        }
    }
    bufferConcat(&formatted, '\0');

    if (formatted.bytes == NULL)
        xmlrpc_faultf(envP, "Couldn't allocate memory to format %g", value);
    else
        *formattedP = formatted.bytes;
}
