/* Copyright information is at end of file */

#include "xmlrpc_config.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "bool.h"
#include "c_util.h"
#include "mallocvar.h"
#include "stdargx.h"

#include "xmlrpc-c/base.h"
#include "xmlrpc-c/base_int.h"
#include "xmlrpc-c/string_int.h"


/*=========================================================================
**  Creating XML-RPC values.
**=========================================================================
**  Build new XML-RPC values from a format string. This code is heavily
**  inspired by Py_BuildValue from Python 1.5.2. In particular, our
**  particular abuse of the va_list data type is copied from the equivalent
**  Python code in modsupport.c. Since Python is portable, our code should
**  (in theory) also be portable.
*/


static void
getString(xmlrpc_env *    const envP,
          const char **   const formatP,
          va_listx *      const argsP,
          xmlrpc_value ** const valPP) {

    const char * str;
    unsigned int len;
    
    str = (const char*) va_arg(argsP->v, char*);
    if (**formatP == '#') {
        (*formatP)++;
        len = (size_t) va_arg(argsP->v, size_t);
    } else
        len = strlen(str);

    *valPP = xmlrpc_string_new_lp(envP, len, str);
}



static void
getWideString(xmlrpc_env *    const envP ATTR_UNUSED,
              const char **   const formatP ATTR_UNUSED,
              va_listx *      const argsP ATTR_UNUSED,
              xmlrpc_value ** const valPP ATTR_UNUSED) {

#if HAVE_UNICODE_WCHAR
    wchar_t *wcs;
    size_t len;
    
    wcs = (wchar_t*) va_arg(argsP->v, wchar_t*);
    if (**formatP == '#') {
        (*formatP)++;
        len = (size_t) va_arg(argsP->v, size_t);
    } else
        len = wcslen(wcs);

    *valPP = xmlrpc_string_w_new_lp(envP, len, wcs);

#endif /* HAVE_UNICODE_WCHAR */
}



static void
getBase64(xmlrpc_env *    const envP,
          va_listx *      const argsP,
          xmlrpc_value ** const valPP) {

    unsigned char * value;
    size_t          length;
    
    value  = (unsigned char*) va_arg(argsP->v, unsigned char*);
    length = (size_t)         va_arg(argsP->v, size_t);

    *valPP = xmlrpc_base64_new(envP, length, value);
}



static void
getValue(xmlrpc_env *    const envP, 
         const char**    const format, 
         va_listx *      const argsP,
         xmlrpc_value ** const valPP);



static void
getArray(xmlrpc_env *    const envP,
         const char **   const formatP,
         char            const delimiter,
         va_listx *      const argsP,
         xmlrpc_value ** const arrayPP) {

    xmlrpc_value * arrayP;

    arrayP = xmlrpc_array_new(envP);

    /* Add items to the array until we hit our delimiter. */
    
    while (**formatP != delimiter && !envP->fault_occurred) {
        
        xmlrpc_value * itemP;
        
        if (**formatP == '\0')
            xmlrpc_env_set_fault(
                envP, XMLRPC_INTERNAL_ERROR,
                "format string ended before closing ')'.");
        else {
            getValue(envP, formatP, argsP, &itemP);
            if (!envP->fault_occurred) {
                xmlrpc_array_append_item(envP, arrayP, itemP);
                xmlrpc_DECREF(itemP);
            }
        }
    }
    if (envP->fault_occurred)
        xmlrpc_DECREF(arrayP);

    *arrayPP = arrayP;
}



static void
getStructMember(xmlrpc_env *    const envP,
                const char **   const formatP,
                va_listx *      const argsP,
                xmlrpc_value ** const keyPP,
                xmlrpc_value ** const valuePP) {


    /* Get the key */
    getValue(envP, formatP, argsP, keyPP);
    if (!envP->fault_occurred) {
        if (**formatP != ':')
            xmlrpc_env_set_fault(
                envP, XMLRPC_INTERNAL_ERROR,
                "format string does not have ':' after a "
                "structure member key.");
        else {
            /* Skip over colon that separates key from value */
            (*formatP)++;
            
            /* Get the value */
            getValue(envP, formatP, argsP, valuePP);
        }
        if (envP->fault_occurred)
            xmlrpc_DECREF(*keyPP);
    }
}
            
            

static void
getStruct(xmlrpc_env *    const envP,
          const char **   const formatP,
          char            const delimiter,
          va_listx *      const argsP,
          xmlrpc_value ** const structPP) {

    xmlrpc_value * structP;

    structP = xmlrpc_struct_new(envP);
    if (!envP->fault_occurred) {
        while (**formatP != delimiter && !envP->fault_occurred) {
            xmlrpc_value * keyP;
            xmlrpc_value * valueP;
            
            getStructMember(envP, formatP, argsP, &keyP, &valueP);
            
            if (!envP->fault_occurred) {
                if (**formatP == ',')
                    (*formatP)++;  /* Skip over the comma */
                else if (**formatP == delimiter) {
                    /* End of the line */
                } else 
                    xmlrpc_env_set_fault(
                        envP, XMLRPC_INTERNAL_ERROR,
                        "format string does not have ',' or ')' after "
                        "a structure member");
                
                if (!envP->fault_occurred)
                    /* Add the new member to the struct. */
                    xmlrpc_struct_set_value_v(envP, structP, keyP, valueP);
                
                xmlrpc_DECREF(valueP);
                xmlrpc_DECREF(keyP);
            }
        }
        if (envP->fault_occurred)
            xmlrpc_DECREF(structP);
    }
    *structPP = structP;
}



static void
mkArrayFromVal(xmlrpc_env *    const envP, 
               xmlrpc_value *  const value,
               xmlrpc_value ** const valPP) {

    if (xmlrpc_value_type(value) != XMLRPC_TYPE_ARRAY)
        xmlrpc_env_set_fault(envP, XMLRPC_INTERNAL_ERROR,
                             "Array format ('A'), non-array xmlrpc_value");
    else
        xmlrpc_INCREF(value);

    *valPP = value;
}



static void
mkStructFromVal(xmlrpc_env *    const envP, 
                xmlrpc_value *  const value,
                xmlrpc_value ** const valPP) {

    if (xmlrpc_value_type(value) != XMLRPC_TYPE_STRUCT)
        xmlrpc_env_set_fault(envP, XMLRPC_INTERNAL_ERROR,
                             "Struct format ('S'), non-struct xmlrpc_value");
    else
        xmlrpc_INCREF(value);

    *valPP = value;
}



static void
getValue(xmlrpc_env *    const envP, 
         const char**    const formatP,
         va_listx *      const argsP,
         xmlrpc_value ** const valPP) {
/*----------------------------------------------------------------------------
   Get the next value from the list.  *formatP points to the specifier
   for the next value in the format string (i.e. to the type code
   character) and we move *formatP past the whole specifier for the
   next value.  We read the required arguments from 'argsP'.  We return
   the value as *valPP with a reference to it.

   For example, if *formatP points to the "i" in the string "sis",
   we read one argument from 'argsP' and return as *valP an integer whose
   value is the argument we read.  We advance *formatP to point to the
   last 's' and advance 'argsP' to point to the argument that belongs to
   that 's'.
-----------------------------------------------------------------------------*/
    char const formatChar = *(*formatP)++;

    switch (formatChar) {
    case 'i':
        *valPP = 
            xmlrpc_int_new(envP, (xmlrpc_int32) va_arg(argsP->v,
                                                       xmlrpc_int32));
        break;

    case 'b':
        *valPP = 
            xmlrpc_bool_new(envP, (xmlrpc_bool) va_arg(argsP->v, xmlrpc_bool));
        break;

    case 'd':
        *valPP =
            xmlrpc_double_new(envP, (double) va_arg(argsP->v, double));
        break;

    case 's':
        getString(envP, formatP, argsP, valPP);
        break;

    case 'w':
        getWideString(envP, formatP, argsP, valPP);
        break;

    case 't':
        *valPP = xmlrpc_datetime_new_sec(envP, va_arg(argsP->v, time_t));
        break;

    case '8':
        *valPP = xmlrpc_datetime_new_str(envP, va_arg(argsP->v, char*));
        break;

    case '6':
        getBase64(envP, argsP, valPP);
        break;

    case 'n':
        *valPP =
            xmlrpc_nil_new(envP);
        break;      

    case 'I':
        *valPP =
            xmlrpc_i8_new(envP, (xmlrpc_int64) va_arg(argsP->v, xmlrpc_int64));
        break;

    case 'p':
        *valPP = 
            xmlrpc_cptr_new(envP, (void*) va_arg(argsP->v, void*));
        break;      

    case 'A':
        mkArrayFromVal(envP, (xmlrpc_value*) va_arg(argsP->v, xmlrpc_value*),
                       valPP);
        break;

    case 'S':
        mkStructFromVal(envP, (xmlrpc_value*) va_arg(argsP->v, xmlrpc_value*),
                        valPP);
        break;

    case 'V':
        *valPP = (xmlrpc_value*) va_arg(argsP->v, xmlrpc_value*);
        xmlrpc_INCREF(*valPP);
        break;

    case '(':
        getArray(envP, formatP, ')', argsP, valPP);
        if (!envP->fault_occurred) {
            XMLRPC_ASSERT(**formatP == ')');
            (*formatP)++;  /* Skip over closing parenthesis */
        }
        break;

    case '{': 
        getStruct(envP, formatP, '}', argsP, valPP);
        if (!envP->fault_occurred) {
            XMLRPC_ASSERT(**formatP == '}');
            (*formatP)++;  /* Skip over closing brace */
        }
        break;

    default: {
        const char * const badCharacter = xmlrpc_makePrintableChar(formatChar);
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_INTERNAL_ERROR,
            "Unexpected character '%s' in format string", badCharacter);
        xmlrpc_strfree(badCharacter);
        }
    }
}



void
xmlrpc_build_value_va(xmlrpc_env *    const envP,
                      const char *    const format,
                      va_list         const args,
                      xmlrpc_value ** const valPP,
                      const char **   const tailP) {

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT(format != NULL);

    if (strlen(format) == 0)
        xmlrpc_faultf(envP, "Format string is empty.");
    else {
        va_listx currentArgs;
        const char * formatCursor;

        init_va_listx(&currentArgs, args);
        formatCursor = &format[0];
        getValue(envP, &formatCursor, &currentArgs, valPP);
        
        if (!envP->fault_occurred)
            XMLRPC_ASSERT_VALUE_OK(*valPP);
        
        *tailP = formatCursor;
    }
}



xmlrpc_value * 
xmlrpc_build_value(xmlrpc_env * const envP,
                   const char * const format, 
                   ...) {

    va_list args;
    xmlrpc_value * retval;
    const char * suffix;

    va_start(args, format);
    xmlrpc_build_value_va(envP, format, args, &retval, &suffix);
    va_end(args);

    if (!envP->fault_occurred) {
        if (*suffix != '\0')
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_INTERNAL_ERROR, "Junk after the argument "
                "specifier: '%s'.  There must be exactly one arument.",
                suffix);
    
        if (envP->fault_occurred)
            xmlrpc_DECREF(retval);
    }
    return retval;
}


/* Copyright (C) 2001 by First Peer, Inc. All rights reserved.
** Copyright (C) 2001 by Eric Kidd. All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission. 
**  
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
** OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
** HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
** SUCH DAMAGE. */
