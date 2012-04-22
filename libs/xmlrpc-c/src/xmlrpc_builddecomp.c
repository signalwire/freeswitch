/* Copyright information is at end of file */

#include "xmlrpc_config.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "bool.h"
#include "xmlrpc-c/base.h"
#include "xmlrpc-c/base_int.h"

/* Borrowed from Python 1.5.2.
** MPW pushes 'extended' for float and double types with varargs */
#ifdef MPW
typedef extended va_double;
#else
typedef double va_double;
#endif

/* Borrowed from Python 1.5.2.
** Python copies its va_list objects before using them in certain
** tricky fashions. We don't why Python does this, but since we're
** abusing our va_list objects in a similar fashion, we'll copy them
** too. */
#if VA_LIST_IS_ARRAY
#define VA_LIST_COPY(dest,src) memcpy((dest), (src), sizeof(va_list))
#else
#if __sun
#define VA_LIST_COPY(dest,src) va_copy((dest),(src))
#else
#define VA_LIST_COPY(dest,src) ((dest) = (src))
#endif
#endif

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
          va_list *       const args,
          xmlrpc_value ** const valPP) {

    const char * str;
    unsigned int len;
    
    str = (const char*) va_arg(*args, char*);
    if (**formatP == '#') {
        (*formatP)++;
        len = (size_t) va_arg(*args, size_t);
    } else
        len = strlen(str);

    *valPP = xmlrpc_string_new_lp(envP, len, str);
}



#ifdef HAVE_UNICODE_WCHAR
static void
mkWideString(xmlrpc_env *    const envP,
             wchar_t *       const wcs,
             size_t          const wcs_len,
             xmlrpc_value ** const valPP) {

    xmlrpc_value * valP;
    char *contents;
    wchar_t *wcs_contents;
    int block_is_inited;
    xmlrpc_mem_block *utf8_block;
    char *utf8_contents;
    size_t utf8_len;

    /* Error-handling preconditions. */
    valP = NULL;
    utf8_block = NULL;
    block_is_inited = 0;

    /* Initialize our XML-RPC value. */
    valP = (xmlrpc_value*) malloc(sizeof(xmlrpc_value));
    XMLRPC_FAIL_IF_NULL(valP, envP, XMLRPC_INTERNAL_ERROR,
                        "Could not allocate memory for wide string");
    valP->_refcount = 1;
    valP->_type = XMLRPC_TYPE_STRING;

    /* More error-handling preconditions. */
    valP->_wcs_block = NULL;

    /* Build our wchar_t block first. */
    valP->_wcs_block =
        XMLRPC_TYPED_MEM_BLOCK_NEW(wchar_t, envP, wcs_len + 1);
    XMLRPC_FAIL_IF_FAULT(envP);
    wcs_contents =
        XMLRPC_TYPED_MEM_BLOCK_CONTENTS(wchar_t, valP->_wcs_block);
    memcpy(wcs_contents, wcs, wcs_len * sizeof(wchar_t));
    wcs_contents[wcs_len] = '\0';
    
    /* Convert the wcs block to UTF-8. */
    utf8_block = xmlrpc_wcs_to_utf8(envP, wcs_contents, wcs_len + 1);
    XMLRPC_FAIL_IF_FAULT(envP);
    utf8_contents = XMLRPC_TYPED_MEM_BLOCK_CONTENTS(char, utf8_block);
    utf8_len = XMLRPC_TYPED_MEM_BLOCK_SIZE(char, utf8_block);

    /* XXX - We need an extra memcopy to initialize _block. */
    XMLRPC_TYPED_MEM_BLOCK_INIT(char, envP, &valP->_block, utf8_len);
    XMLRPC_FAIL_IF_FAULT(envP);
    block_is_inited = 1;
    contents = XMLRPC_TYPED_MEM_BLOCK_CONTENTS(char, &valP->_block);
    memcpy(contents, utf8_contents, utf8_len);

 cleanup:
    if (utf8_block)
        xmlrpc_mem_block_free(utf8_block);
    if (envP->fault_occurred) {
        if (valP) {
            if (valP->_wcs_block)
                xmlrpc_mem_block_free(valP->_wcs_block);
            if (block_is_inited)
                xmlrpc_mem_block_clean(&valP->_block);
            free(valP);
        }
    }
    *valPP = valP;
}
#endif /* HAVE_UNICODE_WCHAR */



static void
getWideString(xmlrpc_env *    const envP,
              const char **   const formatP,
              va_list *       const args,
              xmlrpc_value ** const valPP) {
#ifdef HAVE_UNICODE_WCHAR

    wchar_t *wcs;
    size_t len;
    
    wcs = (wchar_t*) va_arg(*args, wchar_t*);
    if (**formatP == '#') {
        (*formatP)++;
        len = (size_t) va_arg(*args, size_t);
    } else
        len = wcslen(wcs);

    mkWideString(envP, wcs, len, valPP);

#endif /* HAVE_UNICODE_WCHAR */
}



static void
getBase64(xmlrpc_env *    const envP,
          va_list *       const args,
          xmlrpc_value ** const valPP) {

    unsigned char * value;
    size_t          length;
    
    value  = (unsigned char*) va_arg(*args, unsigned char*);
    length = (size_t)         va_arg(*args, size_t);

    *valPP = xmlrpc_base64_new(envP, length, value);
}



static void
getValue(xmlrpc_env *    const envP, 
         const char**    const format, 
         va_list *             args,
         xmlrpc_value ** const valPP);



static void
getArray(xmlrpc_env *    const envP,
         const char **   const formatP,
         char            const delimiter,
         va_list *       const args,
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
            getValue(envP, formatP, args, &itemP);
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
                va_list *       const args,
                xmlrpc_value ** const keyPP,
                xmlrpc_value ** const valuePP) {


    /* Get the key */
    getValue(envP, formatP, args, keyPP);
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
            getValue(envP, formatP, args, valuePP);
        }
        if (envP->fault_occurred)
            xmlrpc_DECREF(*keyPP);
    }
}
            
            

static void
getStruct(xmlrpc_env *    const envP,
          const char **   const formatP,
          char            const delimiter,
          va_list *       const args,
          xmlrpc_value ** const structPP) {

    xmlrpc_value * structP;

    structP = xmlrpc_struct_new(envP);
    if (!envP->fault_occurred) {
        while (**formatP != delimiter && !envP->fault_occurred) {
            xmlrpc_value * keyP;
            xmlrpc_value * valueP;
            
            getStructMember(envP, formatP, args, &keyP, &valueP);
            
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
         va_list *       const args,
         xmlrpc_value ** const valPP) {
/*----------------------------------------------------------------------------
   Get the next value from the list.  *formatP points to the specifier
   for the next value in the format string (i.e. to the type code
   character) and we move *formatP past the whole specifier for the
   next value.  We read the required arguments from 'args'.  We return
   the value as *valPP with a reference to it.

   For example, if *formatP points to the "i" in the string "sis",
   we read one argument from 'args' and return as *valP an integer whose
   value is the argument we read.  We advance *formatP to point to the
   last 's' and advance 'args' to point to the argument that belongs to
   that 's'.
-----------------------------------------------------------------------------*/
    char const formatChar = *(*formatP)++;

    switch (formatChar) {
    case 'i':
        *valPP = 
            xmlrpc_int_new(envP, (xmlrpc_int32) va_arg(*args, xmlrpc_int32));
        break;

    case 'b':
        *valPP = 
            xmlrpc_bool_new(envP, (xmlrpc_bool) va_arg(*args, xmlrpc_bool));
        break;

    case 'd':
        *valPP =
            xmlrpc_double_new(envP, (double) va_arg(*args, va_double));
        break;

    case 's':
        getString(envP, formatP, args, valPP);
        break;

    case 'w':
        getWideString(envP, formatP, args, valPP);
        break;

    /* The code 't' is reserved for a better, time_t based
       implementation of dateTime conversion.  
    */
    case '8':
        *valPP = 
            xmlrpc_datetime_new_str(envP, (char*) va_arg(*args, char*));
        break;

    case '6':
        getBase64(envP, args, valPP);
        break;

    case 'n':
        *valPP =
            xmlrpc_nil_new(envP);
        break;      

    case 'p':
        /* We might someday want to use the code 'p!' to read in a
           cleanup function for this pointer. 
        */
        *valPP = 
            xmlrpc_cptr_new(envP, (void*) va_arg(*args, void*));
        break;      

    case 'A':
        mkArrayFromVal(envP, (xmlrpc_value*) va_arg(*args, xmlrpc_value*),
                       valPP);
        break;

    case 'S':
        mkStructFromVal(envP, (xmlrpc_value*) va_arg(*args, xmlrpc_value*),
                        valPP);
        break;

    case 'V':
        *valPP = (xmlrpc_value*) va_arg(*args, xmlrpc_value*);
        xmlrpc_INCREF(*valPP);
        break;

    case '(':
        getArray(envP, formatP, ')', args, valPP);
        if (!envP->fault_occurred) {
            XMLRPC_ASSERT(**formatP == ')');
            (*formatP)++;  /* Skip over closing parenthesis */
        }
        break;

    case '{': 
        getStruct(envP, formatP, '}', args, valPP);
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
                      va_list               args,
                      xmlrpc_value ** const valPP,
                      const char **   const tailP) {

    const char * formatCursor;
    va_list args_copy;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT(format != NULL);

    if (strlen(format) == 0)
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_INTERNAL_ERROR, "Format string is empty.");
    else {
        formatCursor = &format[0];
        VA_LIST_COPY(args_copy, args);
        getValue(envP, &formatCursor, &args_copy, valPP);
        
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
    xmlrpc_value* retval;
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





/*=========================================================================
**  Parsing XML-RPC values.
**=========================================================================
**  Parse an XML-RPC value based on a format string. This code is heavily
**  inspired by Py_BuildValue from Python 1.5.2.
*/

/* Prototype for recursive invocation: */

static void 
decomposeValue(xmlrpc_env *   const env,
               xmlrpc_value * const val,
               const char **  const format,
               va_list *            args,
               xmlrpc_bool    const oldstyleMemMgmt);



static void 
parsearray(xmlrpc_env *         const env,
           const xmlrpc_value * const array,
           const char **        const format,
           char                 const delimiter,
           va_list *                  args,
           xmlrpc_bool          const oldstyleMemMgmt) {

    int size, i;
    xmlrpc_value *item;

    /* Fetch the array size. */
    size = xmlrpc_array_size(env, array);
    XMLRPC_FAIL_IF_FAULT(env);

    /* Loop over the items in the array. */
    for (i = 0; i < size; i++) {
        /* Bail out if the caller didn't care about the rest of the items. */
        if (**format == '*')
            break;

        item = xmlrpc_array_get_item(env, array, i);
        XMLRPC_FAIL_IF_FAULT(env);

        XMLRPC_ASSERT(**format != '\0');
        if (**format == delimiter)
            XMLRPC_FAIL(env, XMLRPC_INDEX_ERROR, "Too many items in array");
        decomposeValue(env, item, format, args, oldstyleMemMgmt);
        XMLRPC_FAIL_IF_FAULT(env);
    }
    if (**format == '*')
        (*format)++;
    if (**format != delimiter)
        XMLRPC_FAIL(env, XMLRPC_INDEX_ERROR, "Not enough items in array");

           cleanup:
    return;
}



static void 
parsestruct(xmlrpc_env *   const env,
            xmlrpc_value * const strct,
            const char **  const format,
            char           const delimiter,
            va_list *            args,
            xmlrpc_bool    const oldstyleMemMgmt) {

    xmlrpc_value *key, *value;
    char *keystr;
    size_t keylen;

    /* Set up error handling preconditions. */
    key = NULL;

    /* Build the members of our struct. */
    while (**format != '*' && **format != delimiter && **format != '\0') {

        /* Get our key, and skip over the ':' character. Notice the
        ** sudden call to getValue--we're going in the opposite direction. */
        getValue(env, format, args, &key);
        XMLRPC_FAIL_IF_FAULT(env);
        XMLRPC_ASSERT(**format == ':');
        (*format)++;

        /* Look up the value for our key. */
        xmlrpc_parse_value(env, key, "s#", &keystr, &keylen);
        XMLRPC_FAIL_IF_FAULT(env);
        value = xmlrpc_struct_get_value_n(env, strct, keystr, keylen);
        XMLRPC_FAIL_IF_FAULT(env);

        /* Get our value, and skip over the ',' character (if present). */
        decomposeValue(env, value, format, args, oldstyleMemMgmt);
        XMLRPC_FAIL_IF_FAULT(env);
        XMLRPC_ASSERT(**format == ',' || **format == delimiter);
        if (**format == ',')
            (*format)++;

        /* Release our reference, and restore our invariant. */
        xmlrpc_DECREF(key);
        key = NULL;
    }
    if (**format == '*') {
        (*format)++;
        if (**format != delimiter && **format != '\0')
            XMLRPC_FAIL(env, XMLRPC_INTERNAL_ERROR, 
                        "* can appear only at the end "
                        "of a structure format specifier");
    } else {
        /* Here we're supposed to fail if he didn't extract all the
           members.  But we don't know how to determine whether he
           specified all the members, so we always fail.
        */
        XMLRPC_FAIL(env, XMLRPC_INTERNAL_ERROR, "You must specify '*' as the "
                    "last member of a structure in a format specifier "
                    "used for parsing an xmlrpc_value"); 
    }
    XMLRPC_ASSERT(**format == delimiter || **format == '\0');

cleanup:
    if (key)
        xmlrpc_DECREF(key);
}


static void
readString(xmlrpc_env *         const envP,
           const xmlrpc_value * const valueP,
           const char **        const stringValueP,
           xmlrpc_bool          const oldstyleMemMgmt) {

    if (oldstyleMemMgmt) {
        xmlrpc_read_string_old(envP, valueP, stringValueP);
    } else
        xmlrpc_read_string(envP, valueP, stringValueP);
}



static void
readStringLp(xmlrpc_env *         const envP,
             const xmlrpc_value * const valueP,
             size_t *             const lengthP,
             const char **        const stringValueP,
             xmlrpc_bool          const oldstyleMemMgmt) {

    if (oldstyleMemMgmt) {
        xmlrpc_read_string_lp_old(envP, valueP, lengthP, stringValueP);
    } else
        xmlrpc_read_string_lp(envP, valueP, lengthP, stringValueP);
}



#ifdef HAVE_UNICODE_WCHAR
static void
readStringW(xmlrpc_env *     const envP,
            xmlrpc_value *   const valueP,
            const wchar_t ** const stringValueP,
            xmlrpc_bool      const oldstyleMemMgmt) {

    if (oldstyleMemMgmt) {
        xmlrpc_read_string_w_old(envP, valueP, stringValueP);
    } else
        xmlrpc_read_string_w(envP, valueP, stringValueP);
}



static void
readStringWLp(xmlrpc_env *     const envP,
              xmlrpc_value *   const valueP,
              size_t *         const lengthP,
              const wchar_t ** const stringValueP,
              xmlrpc_bool      const oldstyleMemMgmt) {

    if (oldstyleMemMgmt) {
        xmlrpc_read_string_w_lp_old(envP, valueP, lengthP, stringValueP);
    } else
        xmlrpc_read_string_w_lp(envP, valueP, lengthP, stringValueP);
}
#endif


static void
readDatetimeStr(xmlrpc_env *         const envP,
                const xmlrpc_value * const valueP,
                const char **        const stringValueP,
                xmlrpc_bool          const oldstyleMemMgmt) {

    if (oldstyleMemMgmt)
        xmlrpc_read_datetime_str_old(envP, valueP, stringValueP);
    else
        xmlrpc_read_datetime_str(envP, valueP, stringValueP);
}



static void
readBase64(xmlrpc_env *           const envP,
           const xmlrpc_value *   const valueP,
           size_t *               const lengthP,
           const unsigned char ** const byteStringValueP,
           xmlrpc_bool            const oldstyleMemMgmt) {

    if (oldstyleMemMgmt)
        xmlrpc_read_base64_old(envP, valueP, lengthP, byteStringValueP);
    else
        xmlrpc_read_base64(envP, valueP, lengthP, byteStringValueP);
}



static void 
decomposeValue(xmlrpc_env *   const envP,
               xmlrpc_value * const valueP,
               const char **  const format,
               va_list *            args,
               xmlrpc_bool    const oldstyleMemMgmt) {

    char formatSpecChar;

    formatSpecChar = *(*format)++;

    switch (formatSpecChar) {
    case 'i': {
        xmlrpc_int32 * const int32ptr = 
            (xmlrpc_int32*) va_arg(*args, xmlrpc_int32*);
        xmlrpc_read_int(envP, valueP, int32ptr);
        }
    break;

    case 'b': {
        xmlrpc_bool * const boolptr =
            (xmlrpc_bool*) va_arg(*args, xmlrpc_bool*);
        xmlrpc_read_bool(envP, valueP, boolptr);
    }
    break;

    case 'd': {
        double * const doubleptr = (double*) va_arg(*args, double*);
        xmlrpc_read_double(envP, valueP, doubleptr);
    }
    break;

    case '8': {
        /* The code 't' is reserved for a better, time_t based
           implementation of dateTime conversion. 
        */
        const char ** const strptr = (const char**) va_arg(*args, char**);
        readDatetimeStr(envP, valueP, strptr, oldstyleMemMgmt);
    }
    break;

    case 's': {
        const char ** const strptr = (const char**) va_arg(*args, char**);
        if (**format == '#') {
            size_t * const sizeptr = (size_t*) va_arg(*args, size_t**);
            (*format)++;

            readStringLp(envP, valueP, sizeptr, strptr, oldstyleMemMgmt);
        } else
            readString(envP, valueP, strptr, oldstyleMemMgmt);
    }
    break;

    case 'w': {
#ifdef HAVE_UNICODE_WCHAR
        const wchar_t ** const wcsptr = 
            (const wchar_t**) va_arg(*args, wchar_t**);
        if (**format == '#') {
            size_t * const sizeptr = (size_t*) va_arg(*args, size_t**);
            (*format)++;
            readStringWLp(envP, valueP, sizeptr, wcsptr, oldstyleMemMgmt);
        } else
            readStringW(envP, valueP, wcsptr, oldstyleMemMgmt);
#else
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_INTERNAL_ERROR,
            "This XML-RPC For C/C++ library was built without Unicode "
            "wide character capability.  'w' isn't available.");
#endif /* HAVE_UNICODE_WCHAR */
    }
    break;
        
    case '6': {
        const unsigned char ** const binptr =
            (const unsigned char**) va_arg(*args, unsigned char**);
        size_t * const sizeptr = (size_t*) va_arg(*args, size_t**);        
        readBase64(envP, valueP, sizeptr, binptr, oldstyleMemMgmt);
    }
    break;

    case 'n': {
        xmlrpc_read_nil(envP, valueP);
    }
    break;

    case 'p': {
        void ** const voidptrptr = (void**) va_arg(*args, void**);
        xmlrpc_read_cptr(envP, valueP, voidptrptr);
    }
    break;

    case 'V': {
        xmlrpc_value ** const valptr =
            (xmlrpc_value**) va_arg(*args, xmlrpc_value**);
        *valptr = valueP;
        if (!oldstyleMemMgmt)
            xmlrpc_INCREF(valueP);
    }
    break;

    case 'A':
        if (xmlrpc_value_type(valueP) != XMLRPC_TYPE_ARRAY)
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_TYPE_ERROR, "Non-array type supplied for "
                "'A' specifier");
        else {
            xmlrpc_value ** const valptr =
                (xmlrpc_value**) va_arg(*args, xmlrpc_value**);
            *valptr = valueP;
            if (!oldstyleMemMgmt)
                xmlrpc_INCREF(valueP);
        }
        break;

    case 'S':
        if (xmlrpc_value_type(valueP) != XMLRPC_TYPE_STRUCT)
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_TYPE_ERROR, "Non-struct type supplied for "
                "'S' specifier");
        else {
            xmlrpc_value ** const valptr =
                (xmlrpc_value**) va_arg(*args, xmlrpc_value**);
            *valptr = valueP;
            if (!oldstyleMemMgmt)
                xmlrpc_INCREF(valueP);
        }
        break;

    case '(':
        if (xmlrpc_value_type(valueP) != XMLRPC_TYPE_ARRAY)
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_TYPE_ERROR, "Non-array type supplied for "
                "'()' specifier");
        else {
            parsearray(envP, valueP, format, ')', args, oldstyleMemMgmt);
            (*format)++;
        }
        break;

    case '{':
        if (xmlrpc_value_type(valueP) != XMLRPC_TYPE_STRUCT)
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_TYPE_ERROR, "Non-struct type supplied for "
                "'{}' specifier");
        else {
            parsestruct(envP, valueP, format, '}', args, oldstyleMemMgmt);
            (*format)++;
        }
        break;

    default:
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_INTERNAL_ERROR, "Invalid format character '%c'",
            formatSpecChar);
    }
}



void 
xmlrpc_decompose_value_va(xmlrpc_env *   const envP,
                          xmlrpc_value * const value,
                          const char *   const format,
                          va_list              args) {

    const char *format_copy;
    va_list args_copy;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_VALUE_OK(value);
    XMLRPC_ASSERT(format != NULL);

    format_copy = format;
    VA_LIST_COPY(args_copy, args);
    decomposeValue(envP, value, &format_copy, &args_copy, FALSE);
    if (!envP->fault_occurred) {
        XMLRPC_ASSERT(*format_copy == '\0');
    }
}



void 
xmlrpc_decompose_value(xmlrpc_env *   const envP,
                       xmlrpc_value * const value,
                       const char *   const format, 
                       ...) {

    va_list args;

    va_start(args, format);
    xmlrpc_decompose_value_va(envP, value, format, args);
    va_end(args);
}



void 
xmlrpc_parse_value_va(xmlrpc_env *   const envP,
                      xmlrpc_value * const value,
                      const char *   const format,
                      va_list              args) {

    const char *format_copy;
    va_list args_copy;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_VALUE_OK(value);
    XMLRPC_ASSERT(format != NULL);

    format_copy = format;
    VA_LIST_COPY(args_copy, args);
    decomposeValue(envP, value, &format_copy, &args_copy, TRUE);
    if (!envP->fault_occurred) {
        XMLRPC_ASSERT(*format_copy == '\0');
    }
}



void 
xmlrpc_parse_value(xmlrpc_env *   const envP,
                   xmlrpc_value * const value,
                   const char *   const format, 
                   ...) {

    va_list args;

    va_start(args, format);
    xmlrpc_parse_value_va(envP, value, format, args);
    va_end(args);
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
