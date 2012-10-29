#include "xmlrpc_config.h"

#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>

#if HAVE_REGEX
#include <sys/types.h>  /* Missing from regex.h in GNU libc */
#include <regex.h>
#endif

#include "bool.h"

#include "xmlrpc-c/base.h"
#include "xmlrpc-c/base_int.h"
#include "xmlrpc-c/util.h"

#include "parse_datetime.h"



#if HAVE_REGEX

static unsigned int
digitStringValue(const char * const string,
                 regmatch_t   const match) {
/*----------------------------------------------------------------------------
   Return the numerical value of the decimal whole number substring of
   'string' identified by 'match'.  E.g. if 'string' is 'abc34d' and
   'match' says start at 3 and end at 5, we return 34.
-----------------------------------------------------------------------------*/
    unsigned int i;
    unsigned int accum;

    assert(match.rm_so >= 0);
    assert(match.rm_eo >= 0);

    for (i = match.rm_so, accum = 0; i < (unsigned)match.rm_eo; ++i) {
        accum *= 10;
        assert(isdigit(string[i]));
        accum += string[i] - '0';
    }
    return accum;
}
#endif  /* HAVE_REGEX */



#if HAVE_REGEX

static unsigned int
digitStringMillionths(const char * const string,
                      regmatch_t   const match) {
/*----------------------------------------------------------------------------
   Return the number of millionths represented by the digits after the
   decimal point in a decimal string, where thse digits are the substring
   of 'string' identified by 'match'.  E.g. if the substring is
   34, we return 340,000.
-----------------------------------------------------------------------------*/
    unsigned int i;
    unsigned int accum;

    assert(match.rm_so >= 0);
    assert(match.rm_eo >= 0);

    for (i = match.rm_so, accum = 0; i < (unsigned)match.rm_so+6; ++i) {
        accum *= 10;
        if (i < (unsigned)match.rm_eo) {
            assert(isdigit(string[i]));
            accum += string[i] - '0';
        }
    }
    return accum;
}
#endif /* HAVE_REGEX */


#if HAVE_REGEX

static void 
subParseDtRegex_standard(regmatch_t *      const matches,
                         const char *      const datetimeString,
                         xmlrpc_datetime * const dtP) {

    dtP->Y = digitStringValue(datetimeString, matches[1]);
    dtP->M = digitStringValue(datetimeString, matches[2]);
    dtP->D = digitStringValue(datetimeString, matches[3]);
    dtP->h = digitStringValue(datetimeString, matches[4]);
    dtP->m = digitStringValue(datetimeString, matches[5]);
    dtP->s = digitStringValue(datetimeString, matches[6]);
    
    if (matches[7].rm_so == -1)
        dtP->u = 0;
    else
        dtP->u = digitStringMillionths(datetimeString, matches[7]);
}



static void 
subParseDtRegex_standardtzd(regmatch_t *      const matches,
                            const char *      const datetimeString,
                            xmlrpc_datetime * const dtP) {

    dtP->Y = digitStringValue(datetimeString, matches[1]);
    dtP->M = digitStringValue(datetimeString, matches[2]);
    dtP->D = digitStringValue(datetimeString, matches[3]);
    dtP->h = digitStringValue(datetimeString, matches[4]);
    dtP->m = digitStringValue(datetimeString, matches[5]);
    dtP->s = digitStringValue(datetimeString, matches[6]);
}

#endif  /* HAVE_REGEX */


#if HAVE_REGEX

typedef  void (*regparsefunc_t)(regmatch_t *      const matches,
                                const char *      const datetimeString,
                                xmlrpc_datetime * const dtP);


struct regexParser {
    const char * const regex;
    regparsefunc_t func; 
};

static const struct regexParser iso8601Regex[]

    /* Each entry of this table is instructions for recognizing and parsing
       some form of a "dateTime.iso8601" XML element.

       (Note that we recognize far more than just the XML-RPC standard
       dateTime.iso8601).
    */

    = {
           {
               /* Examples:
                  YYYYMMDD[T]HHMMSS
                  YYYY-MM-DD[T]HH:MM:SS
                  YYYY-MM-DD[T]HH:MM:SS.ssss
               */

               "^([0-9]{4})\\-?([0-9]{2})\\-?([0-9]{2})T"
               "([0-9]{2}):?([0-9]{2}):?([0-9]{2})\\.?([0-9]+)?$",
               subParseDtRegex_standard
           },
           
           { 
               /* Examples:
                  YYYYMMDD[T]HHMMSS[Z]
                  YYYYMMDD[T]HHMMSS[+-]hh
                  YYYYMMDD[T]HHMMSS[+-]hhmm
               */

               "^([0-9]{4})\\-?([0-9]{2})\\-?([0-9]{2})T"
               "([0-9]{2}):?([0-9]{2}):?([0-9]{2})[Z\\+\\-]([0-9]{2,4})?$",
               subParseDtRegex_standardtzd
           },
           { NULL, NULL }
    };
#endif  /* HAVE_REGEX */



#if HAVE_REGEX
static void
parseDtRegex(xmlrpc_env *      const envP,
             const char *      const datetimeString,
             xmlrpc_datetime * const dtP) {

    unsigned int i;
    const struct regexParser * parserP;
        /* The parser that matches 'datetimeString'.  Null if no match yet
           found.
        */
    regmatch_t matches[1024];

    for (i = 0, parserP = NULL; iso8601Regex[i].regex && !parserP; ++i) {
        const struct regexParser * const thisParserP = &iso8601Regex[i];

        regex_t re;
        int status;

        status = regcomp(&re, thisParserP->regex, REG_ICASE | REG_EXTENDED);

        /* Our regex is valid, so it must have compiled: */
        assert(status == 0);
        {
            int status;
    
            status = regexec(&re, datetimeString, ARRAY_SIZE(matches), 
                             matches, 0);
    
            if (status == 0) {
                assert(matches[0].rm_so != -1);  /* Match of whole regex */
                
                parserP = thisParserP;
            }
            regfree(&re);
        }
    }

    if (parserP) {
        parserP->func(matches, datetimeString, dtP);
    } else {
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_PARSE_ERROR,
            "value '%s' is not of any form we recognize "
            "for a <dateTime.iso8601> element",
            datetimeString);
    }

}
#endif  /* HAVE_REGEX */



static __inline__ void
parseDtNoRegex(xmlrpc_env *      const envP,
               const char *      const datetimeString,
               xmlrpc_datetime * const dtP) {

    unsigned int const dtStrlen = strlen(datetimeString);

    char year[4+1];
    char month[2+1];
    char day[2+1];
    char hour[2+1];
    char minute[2+1];
    char second[2+1];

    if (dtStrlen < 17 || dtStrlen == 18 || dtStrlen > 24)
        xmlrpc_faultf(envP, "could not parse date, size incompatible: '%d'",
                      dtStrlen);
    else {
        year[0]   = datetimeString[ 0];
        year[1]   = datetimeString[ 1];
        year[2]   = datetimeString[ 2];
        year[3]   = datetimeString[ 3];
        year[4]   = '\0';

        month[0]  = datetimeString[ 4];
        month[1]  = datetimeString[ 5];
        month[2]  = '\0';

        day[0]    = datetimeString[ 6];
        day[1]    = datetimeString[ 7];
        day[2]    = '\0';

        assert(datetimeString[ 8] == 'T');

        hour[0]   = datetimeString[ 9];
        hour[1]   = datetimeString[10];
        hour[2]   = '\0';

        assert(datetimeString[11] == ':');

        minute[0] = datetimeString[12];
        minute[1] = datetimeString[13];
        minute[2] = '\0';

        assert(datetimeString[14] == ':');

        second[0] = datetimeString[15];
        second[1] = datetimeString[16];
        second[2] = '\0';

        if (dtStrlen > 17) {
            unsigned int const pad = 24 - dtStrlen;
            unsigned int i;

            dtP->u = atoi(&datetimeString[18]);
            for (i = 0; i < pad; ++i)
                dtP->u *= 10;
        } else
            dtP->u = 0;

        dtP->Y = atoi(year);
        dtP->M = atoi(month);
        dtP->D = atoi(day);
        dtP->h = atoi(hour);
        dtP->m = atoi(minute);
        dtP->s = atoi(second);
    }
}



static void
validateFirst17(xmlrpc_env * const envP,
                const char * const dt) {
/*----------------------------------------------------------------------------
   Assuming 'dt' is at least 17 characters long, validate that the first
   17 characters are a valid XML-RPC datetime, e.g.
   "20080628T16:35:02"
-----------------------------------------------------------------------------*/
    unsigned int i;

    for (i = 0; i < 8 && !envP->fault_occurred; ++i)
        if (!isdigit(dt[i]))
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_PARSE_ERROR, "Not a digit: '%c'", dt[i]);

    if (dt[8] != 'T')
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_PARSE_ERROR, "9th character is '%c', not 'T'",
            dt[8]);
    if (!isdigit(dt[9]))
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_PARSE_ERROR, "Not a digit: '%c'", dt[9]);
    if (!isdigit(dt[10]))
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_PARSE_ERROR, "Not a digit: '%c'", dt[10]);
    if (dt[11] != ':')
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_PARSE_ERROR, "Not a colon: '%c'", dt[11]);
    if (!isdigit(dt[12]))
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_PARSE_ERROR, "Not a digit: '%c'", dt[12]);
    if (!isdigit(dt[13]))
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_PARSE_ERROR, "Not a digit: '%c'", dt[13]);
    if (dt[14] != ':')
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_PARSE_ERROR, "Not a colon: '%c'", dt[14]);
    if (!isdigit(dt[15]))
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_PARSE_ERROR, "Not a digit: '%c'", dt[15]);
    if (!isdigit(dt[16]))
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_PARSE_ERROR, "Not a digit: '%c'", dt[16]);
}



static void
validateFractionalSeconds(xmlrpc_env * const envP,
                          const char * const dt) {
/*----------------------------------------------------------------------------
   Validate the fractional seconds part of the XML-RPC datetime string
   'dt', if any.  That's the decimal point and everything following
   it.
-----------------------------------------------------------------------------*/
    if (strlen(dt) > 17) {
        if (dt[17] != '.') {
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_PARSE_ERROR,
                "'%c' where only a period is valid", dt[17]);
        } else {
            if (dt[18] == '\0')
                xmlrpc_env_set_fault_formatted(
                    envP, XMLRPC_PARSE_ERROR, "Nothing after decimal point");
            else {
                unsigned int i;
                for (i = 18; dt[i] != '\0' && !envP->fault_occurred; ++i) {
                    if (!isdigit(dt[i]))
                        xmlrpc_env_set_fault_formatted(
                            envP, XMLRPC_PARSE_ERROR,
                            "Non-digit in fractional seconds: '%c'", dt[i]);
                }
            }
        }
    }
}



static __inline__ void
validateFormatNoRegex(xmlrpc_env * const envP,
                      const char * const dt) {

    if (strlen(dt) < 17)
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_PARSE_ERROR, 
            "Invalid length of %u of datetime.  "
            "Must be at least 17 characters",
            (unsigned)strlen(dt));
    else {
        validateFirst17(envP, dt);

        validateFractionalSeconds(envP, dt);
    }
}



static void
validateXmlrpcDatetimeSome(xmlrpc_env *    const envP,
                           xmlrpc_datetime const dt) {
/*----------------------------------------------------------------------------
  Type xmlrpc_datetime is defined such that it can represent a nonexistent
  datetime such as February 30.

  Validate that 'dt' doesn't have glaring invalidities such as Hour 25.
  We leave the possibility of more subtle invalidity such as February 30.
-----------------------------------------------------------------------------*/

    if (dt.M < 1 || dt.M > 12)
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_PARSE_ERROR,
            "Month of year value %u is not in the range 1-12", dt.M);
    else if (dt.D < 1 || dt.D > 31)
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_PARSE_ERROR,
            "Day of month value %u is not in the range 1-31", dt.D);
    else if (dt.h > 23)
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_PARSE_ERROR,
            "Hour of day value %u is not in the range 0-23", dt.h);
    else if (dt.m > 59)
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_PARSE_ERROR,
            "Minute of hour value %u is not in the range 0-59", dt.m);
    else if (dt.s > 59)
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_PARSE_ERROR,
            "Second of minute value %u is not in the range 0-59", dt.s);
    else if (dt.u > 999999)
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_PARSE_ERROR,
            "Microsecond of second value %u is not in the range 0-1M", dt.u);
}



void
xmlrpc_parseDatetime(xmlrpc_env *    const envP,
                     const char *    const datetimeString,
                     xmlrpc_value ** const valuePP) {
/*----------------------------------------------------------------------------
   Parse the content of a <datetime.iso8601> XML-RPC XML element, e.g. 
   "20000301T00:00:00".

   'str' is that content.

   Example of the format we parse: "19980717T14:08:55"
   Note that this is not quite ISO 8601.  It's a bizarre combination of
   two ISO 8601 formats.

   Note that Xmlrpc-c recognizes various extensions of the XML-RPC
   <datetime.iso8601> element type.

   'str' may not be valid XML-RPC (with extensions).  In that case we fail
   with fault code XMLRPC_PARSE_ERROR.
-----------------------------------------------------------------------------*/
    xmlrpc_datetime dt;

#if HAVE_REGEX
    parseDtRegex(envP, datetimeString, &dt);
#else
    /* Note: validation is not as strong without regex */
    validateFormatNoRegex(envP, datetimeString);
    if (!envP->fault_occurred)
        parseDtNoRegex(envP, datetimeString, &dt);
#endif

    if (!envP->fault_occurred) {
        validateXmlrpcDatetimeSome(envP, dt);

        if (!envP->fault_occurred)
            *valuePP = xmlrpc_datetime_new(envP, dt);
    }
}
