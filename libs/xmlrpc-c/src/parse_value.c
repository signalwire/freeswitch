#include "xmlrpc_config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <float.h>

#include "bool.h"

#include "xmlrpc-c/base.h"
#include "xmlrpc-c/base_int.h"
#include "xmlrpc-c/string_int.h"
#include "xmlrpc-c/util.h"
#include "xmlrpc-c/xmlparser.h"

#include "parse_value.h"



static void
setParseFault(xmlrpc_env * const envP,
              const char * const format,
              ...) {

    va_list args;
    va_start(args, format);
    xmlrpc_set_fault_formatted_v(envP, XMLRPC_PARSE_ERROR, format, args);
    va_end(args);
}



static void
parseArrayDataChild(xmlrpc_env *   const envP,
                    xml_element *  const childP,
                    unsigned int   const maxRecursion,
                    xmlrpc_value * const arrayP) {

    const char * const elemName = xml_element_name(childP);

    if (!xmlrpc_streq(elemName, "value"))
        setParseFault(envP, "<data> element has <%s> child.  "
                      "Only <value> makes sense.", elemName);
    else {
        xmlrpc_value * itemP;

        xmlrpc_parseValue(envP, maxRecursion-1, childP, &itemP);

        if (!envP->fault_occurred) {
            xmlrpc_array_append_item(envP, arrayP, itemP);

            xmlrpc_DECREF(itemP);
        }
    }
}



static void
parseArray(xmlrpc_env *    const envP,
           unsigned int    const maxRecursion,
           xml_element *   const arrayElemP,
           xmlrpc_value ** const arrayPP) {

    xmlrpc_value * arrayP;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT(arrayElemP != NULL);

    arrayP = xmlrpc_array_new(envP);
    if (!envP->fault_occurred) {
        unsigned int const childCount = xml_element_children_size(arrayElemP);

        if (childCount != 1)
            setParseFault(envP,
                          "<array> element has %u children.  Only one <data> "
                          "makes sense.", childCount);
        else {
            xml_element * const dataElemP =
                xml_element_children(arrayElemP)[0];
            const char * const elemName = xml_element_name(dataElemP);

            if (!xmlrpc_streq(elemName, "data"))
                setParseFault(envP,
                              "<array> element has <%s> child.  Only <data> "
                              "makes sense.", elemName);
            else {
                xml_element ** const values = xml_element_children(dataElemP);
                unsigned int const size = xml_element_children_size(dataElemP);

                unsigned int i;

                for (i = 0; i < size && !envP->fault_occurred; ++i)
                    parseArrayDataChild(envP, values[i], maxRecursion, arrayP);
            }
        }
        if (envP->fault_occurred)
            xmlrpc_DECREF(arrayP);
        else
            *arrayPP = arrayP;
    }
}



static void
parseName(xmlrpc_env *    const envP,
          xml_element *   const nameElemP,
          xmlrpc_value ** const valuePP) {

    unsigned int const childCount = xml_element_children_size(nameElemP);

    if (childCount > 0)
        setParseFault(envP, "<name> element has %u children.  "
                      "Should have none.", childCount);
    else {
        const char * const cdata     = xml_element_cdata(nameElemP);
        size_t       const cdataSize = xml_element_cdata_size(nameElemP);

        *valuePP = xmlrpc_string_new_lp(envP, cdataSize, cdata);
    }
}



static void
getNameChild(xmlrpc_env *    const envP,
             xml_element *   const parentP,
             xml_element * * const childPP) {

    xml_element ** const children   = xml_element_children(parentP);
    size_t         const childCount = xml_element_children_size(parentP);

    xml_element * childP;
    unsigned int i;

    for (i = 0, childP = NULL; i < childCount && !childP; ++i) {
        if (xmlrpc_streq(xml_element_name(children[i]), "name"))
            childP = children[i];
    }
    if (!childP)
        xmlrpc_env_set_fault(envP, XMLRPC_PARSE_ERROR,
                             "<member> has no <name> child");
    else
        *childPP = childP;
}



static void
getValueChild(xmlrpc_env *    const envP,
              xml_element *   const parentP,
              xml_element * * const childPP) {

    xml_element ** const children   = xml_element_children(parentP);
    size_t         const childCount = xml_element_children_size(parentP);

    xml_element * childP;
    unsigned int i;

    for (i = 0, childP = NULL; i < childCount && !childP; ++i) {
        if (xmlrpc_streq(xml_element_name(children[i]), "value"))
            childP = children[i];
    }
    if (!childP)
        xmlrpc_env_set_fault(envP, XMLRPC_PARSE_ERROR,
                             "<member> has no <value> child");
    else
        *childPP = childP;
}



static void
parseMember(xmlrpc_env *    const envP,
            xml_element *   const memberP,
            unsigned int    const maxRecursion,
            xmlrpc_value ** const keyPP,
            xmlrpc_value ** const valuePP) {

    unsigned int const childCount = xml_element_children_size(memberP);

    if (childCount != 2)
        setParseFault(envP,
                      "<member> element has %u children.  Only one <name> and "
                      "one <value> make sense.", childCount);
    else {
        xml_element * nameElemP = NULL;

        getNameChild(envP, memberP, &nameElemP);

        if (!envP->fault_occurred) {
            parseName(envP, nameElemP, keyPP);

            if (!envP->fault_occurred) {
                xml_element * valueElemP = NULL;

                getValueChild(envP, memberP, &valueElemP);
                
                if (!envP->fault_occurred)
                    xmlrpc_parseValue(envP, maxRecursion-1, valueElemP,
                                      valuePP);

                if (envP->fault_occurred)
                    xmlrpc_DECREF(*keyPP);
            }
        }
    }
}



static void
parseStruct(xmlrpc_env *    const envP,
            unsigned int    const maxRecursion,
            xml_element *   const elemP,
            xmlrpc_value ** const structPP) {
/*----------------------------------------------------------------------------
   Parse the <struct> element 'elemP'.
-----------------------------------------------------------------------------*/
    xmlrpc_value * structP;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT(elemP != NULL);

    structP = xmlrpc_struct_new(envP);
    if (!envP->fault_occurred) {
        /* Iterate over our children, extracting key/value pairs. */

        xml_element ** const members = xml_element_children(elemP);
        unsigned int const size = xml_element_children_size(elemP);

        unsigned int i;

        for (i = 0; i < size && !envP->fault_occurred; ++i) {
            const char * const elemName = xml_element_name(members[i]);

            if (!xmlrpc_streq(elemName, "member"))
                setParseFault(envP, "<%s> element found where only <member> "
                              "makes sense", elemName);
            else {
                xmlrpc_value * keyP = NULL;
                xmlrpc_value * valueP;

                parseMember(envP, members[i], maxRecursion, &keyP, &valueP);

                if (!envP->fault_occurred) {
                    xmlrpc_struct_set_value_v(envP, structP, keyP, valueP);

                    xmlrpc_DECREF(keyP);
                    xmlrpc_DECREF(valueP);
                }
            }
        }
        if (envP->fault_occurred)
            xmlrpc_DECREF(structP);
        else
            *structPP = structP;
    }
}



static void
parseInt(xmlrpc_env *    const envP,
         const char *    const str,
         xmlrpc_value ** const valuePP) {
/*----------------------------------------------------------------------------
   Parse the content of a <int> XML-RPC XML element, e.g. "34".

   'str' is that content.
-----------------------------------------------------------------------------*/
    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_PTR_OK(str);

    if (str[0] == '\0')
        setParseFault(envP, "<int> XML element content is empty");
    else if (isspace(str[0]))
        setParseFault(envP, "<int> content '%s' starts with white space",
                      str);
    else {
        long i;
        char * tail;

        errno = 0;
        i = strtol(str, &tail, 10);

        /* Look for ERANGE. */
        if (errno == ERANGE)
            setParseFault(envP, "<int> XML element value '%s' represents a "
                          "number beyond the range that "
                          "XML-RPC allows (%d - %d)", str,
                          XMLRPC_INT32_MIN, XMLRPC_INT32_MAX);
        else if (errno != 0)
            setParseFault(envP, "unexpected error parsing <int> XML element "
                          "value '%s'.  strtol() failed with errno %d (%s)",
                          str, errno, strerror(errno));
        else {
            /* Look for out-of-range errors which didn't produce ERANGE. */
            if (i < XMLRPC_INT32_MIN)
                setParseFault(envP,
                              "<int> value %d is below the range allowed "
                              "by XML-RPC (minimum is %d)",
                              i, XMLRPC_INT32_MIN);
            else if (i > XMLRPC_INT32_MAX)
                setParseFault(envP,
                              "<int> value %d is above the range allowed "
                              "by XML-RPC (maximum is %d)",
                              i, XMLRPC_INT32_MAX);
            else {
                if (tail[0] != '\0')
                    setParseFault(envP,
                                  "<int> value '%s' contains non-numerical "
                                  "junk: '%s'", str, tail);
                else
                    *valuePP = xmlrpc_int_new(envP, i);
            }
        }
    }
}



static void
parseBoolean(xmlrpc_env *    const envP,
             const char *    const str,
             xmlrpc_value ** const valuePP) {
/*----------------------------------------------------------------------------
   Parse the content of a <boolean> XML-RPC XML element, e.g. "1".

   'str' is that content.
-----------------------------------------------------------------------------*/
    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_PTR_OK(str);

    if (xmlrpc_streq(str, "0") || xmlrpc_streq(str, "1"))
        *valuePP = xmlrpc_bool_new(envP, xmlrpc_streq(str, "1") ? 1 : 0);
    else
        setParseFault(envP, "<boolean> XML element content must be either "
                      "'0' or '1' according to XML-RPC.  This one has '%s'",
                      str);
}



static void
scanAndValidateDoubleString(xmlrpc_env *  const envP,
                            const char *  const string,
                            const char ** const mantissaP,
                            const char ** const mantissaEndP,
                            const char ** const fractionP,
                            const char ** const fractionEndP) {

    const char * mantissa;
    const char * dp;
    const char * p;

    if (string[0] == '-' || string[0] == '+')
        mantissa = &string[1];
    else
        mantissa = &string[0];

    for (p = mantissa, dp = NULL; *p; ++p) {
        char const c = *p;
        if (c == '.') {
            if (dp) {
                setParseFault(envP, "Two decimal points");
                return;
            } else
                dp = p;
        } else if (c < '0' || c > '9') {
            setParseFault(envP, "Garbage (not sign, digit, or period) "
                          "starting at '%s'", p);
            return;
        }
    }
    *mantissaP = mantissa;
    if (dp) {
        *mantissaEndP = dp;
        *fractionP    = dp+1;
        *fractionEndP = p;
    } else {
        *mantissaEndP = p;
        *fractionP    = p;
        *fractionEndP = p;
    }
}



static bool
isInfinite(double const value) {

    return value > DBL_MAX;
}



static void
parseDoubleString(xmlrpc_env *  const envP,
                  const char *  const string,
                  double *      const valueP) {
/*----------------------------------------------------------------------------
   Turn e.g. "4.3" into 4.3 .
-----------------------------------------------------------------------------*/
    /* strtod() is no good for this because it is designed for human
       interfaces; it parses according to locale.  As a practical
       matter that sometimes means that it does not recognize "." as a
       decimal point.  In XML-RPC, "." is a decimal point.

       Design note: in my experiments, using strtod() was 10 times
       slower than using this function.
    */
    const char * mantissa = NULL;
    const char * mantissaEnd = NULL;
    const char * fraction = NULL;
    const char * fractionEnd = NULL;

    scanAndValidateDoubleString(envP, string, &mantissa, &mantissaEnd,
                                &fraction, &fractionEnd);

    if (!envP->fault_occurred) {
        double accum;

        accum = 0.0;

        if (mantissa == mantissaEnd && fraction == fractionEnd) {
            setParseFault(envP, "No digits");
            return;
        }
        {
            /* Add in the whole part */
            const char * p;

            for (p = mantissa; p < mantissaEnd; ++p) {
                accum *= 10;
                accum += (*p - '0');
            }
        }
        {
            /* Add in the fractional part */
            double significance;
            const char * p;
            for (significance = 0.1, p = fraction;
                 p < fractionEnd;
                 ++p, significance *= 0.1) {
                
                accum += (*p - '0') * significance;
            }
        }
        if (isInfinite(accum))
            setParseFault(envP, "Value exceeds the size allowed by XML-RPC");
        else
            *valueP = string[0] == '-' ? (- accum) : accum;
    }
}



static void
parseDoubleStringStrtod(const char * const str,
                        bool *       const failedP,
                        double *     const valueP) {

    if (strlen(str) == 0) {
        /* strtod() happily interprets empty string as 0.0.  We don't think
           the user will appreciate that XML-RPC extension.
        */
        *failedP = true;
    } else {
        char * tail;

        errno = 0;

        *valueP = strtod(str, &tail);
    
        if (errno != 0)
            *failedP = true;
        else {
            if (tail[0] != '\0')
                *failedP = true;
            else
                *failedP = false;
        }
    }
}



static void
parseDouble(xmlrpc_env *    const envP,
            const char *    const str,
            xmlrpc_value ** const valuePP) {
/*----------------------------------------------------------------------------
   Parse the content of a <double> XML-RPC XML element, e.g. "34.5".

   'str' is that content.
-----------------------------------------------------------------------------*/
    xmlrpc_env parseEnv;
    double valueDouble = 0;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_PTR_OK(str);

    xmlrpc_env_init(&parseEnv);

    parseDoubleString(&parseEnv, str, &valueDouble);

    if (parseEnv.fault_occurred) {
        /* As an alternative, try a strtod() parsing.  strtod()
           accepts other forms, e.g. "3.4E6"; "3,4"; " 3.4".  These
           are not permitted by XML-RPC, but an almost-XML-RPC partner
           might use one.  In fact, for many years, Xmlrpc-c generated
           such alternatives (by mistake).
        */
        bool failed;
        parseDoubleStringStrtod(str, &failed, &valueDouble);
        if (failed)
            setParseFault(envP, "<double> element value '%s' is not a valid "
                          "floating point number.  %s",
                          str, parseEnv.fault_string);
    }
    
    if (!envP->fault_occurred)
        *valuePP = xmlrpc_double_new(envP, valueDouble);

    xmlrpc_env_clean(&parseEnv);
}



static void
parseBase64(xmlrpc_env *    const envP,
            const char *    const str,
            size_t          const strLength,
            xmlrpc_value ** const valuePP) {
/*----------------------------------------------------------------------------
   Parse the content of a <base64> XML-RPC XML element, e.g. "FD32YY".

   'str' is that content.
-----------------------------------------------------------------------------*/
    xmlrpc_mem_block * decoded;
    
    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_PTR_OK(str);

    decoded = xmlrpc_base64_decode(envP, str, strLength);
    if (!envP->fault_occurred) {
        unsigned char * const bytes =
            XMLRPC_MEMBLOCK_CONTENTS(unsigned char, decoded);
        size_t const byteCount =
            XMLRPC_MEMBLOCK_SIZE(unsigned char, decoded);

        *valuePP = xmlrpc_base64_new(envP, byteCount, bytes);
        
        XMLRPC_MEMBLOCK_FREE(unsigned char, decoded);
    }
}



static void
parseI8(xmlrpc_env *    const envP,
        const char *    const str,
        xmlrpc_value ** const valuePP) {
/*----------------------------------------------------------------------------
   Parse the content of a <i8> XML-RPC XML element, e.g. "34".

   'str' is that content.
-----------------------------------------------------------------------------*/
    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_PTR_OK(str);

    if (str[0] == '\0')
        setParseFault(envP, "<i8> XML element content is empty");
    else if (isspace(str[0]))
        setParseFault(envP,
                      "<i8> content '%s' starts with white space", str);
    else {
        xmlrpc_int64 i;
        char * tail;

        errno = 0;
        i = strtoll(str, &tail, 10);

        if (errno == ERANGE)
            setParseFault(envP, "<i8> XML element value '%s' represents a "
                          "number beyond the range that "
                          "XML-RPC allows (%d - %d)", str,
                          XMLRPC_INT64_MIN, XMLRPC_INT64_MAX);
        else if (errno != 0)
            setParseFault(envP, "unexpected error parsing <i8> XML element "
                          "value '%s'.  strtoll() failed with errno %d (%s)",
                          str, errno, strerror(errno));
        else {
            /* Look for out-of-range errors which didn't produce ERANGE. */
            if (i < XMLRPC_INT64_MIN)
                setParseFault(envP, "<i8> value %d is below the range allowed "
                           "by XML-RPC (minimum is %d)",
                           i, XMLRPC_INT64_MIN);
            else if (i > XMLRPC_INT64_MAX)
                setParseFault(envP, "<i8> value %d is above the range allowed "
                              "by XML-RPC (maximum is %d)",
                              i, XMLRPC_INT64_MAX);
            else {
                if (tail[0] != '\0')
                    setParseFault(envP,
                                  "<i8> value '%s' contains non-numerical "
                                  "junk: '%s'", str, tail);
                else
                    *valuePP = xmlrpc_i8_new(envP, i);
            }
        }
    }
}



static void
parseSimpleValueCdata(xmlrpc_env *    const envP,
                      const char *    const elementName,
                      const char *    const cdata,
                      size_t          const cdataLength,
                      xmlrpc_value ** const valuePP) {
/*----------------------------------------------------------------------------
   Parse an XML element is supposedly a data type element such as
   <string>.  Its name is 'elementName', and it has no children, but
   contains cdata 'cdata', which is 'dataLength' characters long.
-----------------------------------------------------------------------------*/
    /* We need to straighten out the whole character set / encoding thing
       some day.  What is 'cdata', and what should it be?  Does it have
       embedded NUL?  Some of the code here assumes it doesn't.  Is it
       text?

       The <string> parser assumes it's UTF 8 with embedded NULs.
       But the <int> parser will get terribly confused if there are any
       UTF-8 multibyte sequences or NUL characters.  So will most of the
       others.

       The "ex.XXX" element names are what the Apache XML-RPC facility
       uses: http://ws.apache.org/xmlrpc/types.html.  i1 and i2 are just
       from my imagination.
    */

    if (xmlrpc_streq(elementName, "int")   ||
        xmlrpc_streq(elementName, "i4")    ||
        xmlrpc_streq(elementName, "i1")    ||
        xmlrpc_streq(elementName, "i2")    ||
        xmlrpc_streq(elementName, "ex.i1") ||
        xmlrpc_streq(elementName, "ex.i2"))
        parseInt(envP, cdata, valuePP);
    else if (xmlrpc_streq(elementName, "boolean"))
        parseBoolean(envP, cdata, valuePP);
    else if (xmlrpc_streq(elementName, "double"))
        parseDouble(envP, cdata, valuePP);
    else if (xmlrpc_streq(elementName, "dateTime.iso8601"))
        *valuePP = xmlrpc_datetime_new_str(envP, cdata);
    else if (xmlrpc_streq(elementName, "string"))
        *valuePP = xmlrpc_string_new_lp(envP, cdataLength, cdata);
    else if (xmlrpc_streq(elementName, "base64"))
        parseBase64(envP, cdata, cdataLength, valuePP);
    else if (xmlrpc_streq(elementName, "nil") ||
             xmlrpc_streq(elementName, "ex.nil"))
        *valuePP = xmlrpc_nil_new(envP);
    else if (xmlrpc_streq(elementName, "i8") ||
             xmlrpc_streq(elementName, "ex.i8"))
        parseI8(envP, cdata, valuePP);
    else
        setParseFault(envP, "Unknown value type -- XML element is named "
                      "<%s>", elementName);
}



static void
parseSimpleValue(xmlrpc_env *    const envP,
                 xml_element *   const elemP,
                 xmlrpc_value ** const valuePP) {
    
    unsigned int const childCount = xml_element_children_size(elemP);
                    
    if (childCount > 0)
        setParseFault(envP, "The child of a <value> element "
                      "is neither <array> nor <struct>, "
                      "but has %u child elements of its own.",
                      childCount);
    else {
        const char * const elemName  = xml_element_name(elemP);
        const char * const cdata     = xml_element_cdata(elemP);
        size_t       const cdataSize = xml_element_cdata_size(elemP);
                    
        parseSimpleValueCdata(envP, elemName, cdata, cdataSize, valuePP);
    }
}



void
xmlrpc_parseValue(xmlrpc_env *    const envP,
                  unsigned int    const maxRecursion,
                  xml_element *   const elemP,
                  xmlrpc_value ** const valuePP) {
/*----------------------------------------------------------------------------
   Compute the xmlrpc_value represented by the XML <value> element 'elem'.
   Return that xmlrpc_value.

   We call convert_array() and convert_struct(), which may ultimately
   call us recursively.  Don't recurse any more than 'maxRecursion'
   times.
-----------------------------------------------------------------------------*/
    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT(elemP != NULL);

    /* Assume we'll need to recurse, make sure we're allowed */
    if (maxRecursion < 1) 
        xmlrpc_env_set_fault(envP, XMLRPC_PARSE_ERROR,
                             "Nested data structure too deep.");
    else {
        if (!xmlrpc_streq(xml_element_name(elemP), "value"))
            setParseFault(envP,
                          "<%s> element where <value> expected",
                          xml_element_name(elemP));
        else {
            unsigned int const childCount = xml_element_children_size(elemP);

            if (childCount == 0) {
                /* We have no type element, so treat the value as a string. */
                char * const cdata      = xml_element_cdata(elemP);
                size_t const cdata_size = xml_element_cdata_size(elemP);
                *valuePP = xmlrpc_string_new_lp(envP, cdata_size, cdata);
            } else if (childCount > 1)
                setParseFault(envP, "<value> has %u child elements.  "
                              "Only zero or one make sense.", childCount);
            else {
                /* We should have a type tag inside our value tag. */
                xml_element * const childP = xml_element_children(elemP)[0];
                const char * const childName = xml_element_name(childP);

                if (xmlrpc_streq(childName, "struct"))
                    parseStruct(envP, maxRecursion, childP, valuePP);
                else if (xmlrpc_streq(childName, "array"))
                    parseArray(envP, maxRecursion, childP, valuePP);
                else
                    parseSimpleValue(envP, childP, valuePP);
            }
        }
    }
}
