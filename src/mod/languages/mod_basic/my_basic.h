/*
** This source file is part of MY-BASIC
**
** For the latest info, see http://code.google.com/p/my-basic/
**
** Copyright (c) 2011 - 2013 Tony & Tony's Toy Game Development Team
**
** Permission is hereby granted, free of charge, to any person obtaining a copy of
** this software and associated documentation files (the "Software"), to deal in
** the Software without restriction, including without limitation the rights to
** use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
** the Software, and to permit persons to whom the Software is furnished to do so,
** subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in all
** copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
** FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
** COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
** IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
** CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef __MY_BASIC_H__
#define __MY_BASIC_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef MBAPI
#	define MBAPI
#endif /* MBAPI */

#ifndef MB_COMPACT_MODE
#	define MB_COMPACT_MODE
#endif /* MB_COMPACT_MODE */

#ifdef MB_COMPACT_MODE
#	pragma pack(1)
#endif /* MB_COMPACT_MODE */

#ifndef true
#	define true (!0)
#endif
#ifndef false
#	define false (0)
#endif

#ifndef bool_t
#	define bool_t int
#endif
#ifndef byte_t
#	define byte_t unsigned char
#endif
#ifndef int_t
#	define int_t int
#endif
#ifndef real_t
#	define real_t float
#endif

#ifndef _MSC_VER
#	ifndef _strcmpi
#		define _strcmpi strcasecmp
#	endif /* _strcmpi */
#endif /* _MSC_VER */

#ifndef mb_assert
#	define mb_assert(__a) do { ((void)(__a)); assert(__a); } while(0)
#endif /* mb_assert */

#ifndef mb_unrefvar
#	define mb_unrefvar(__v) ((void)(__v))
#endif /* mb_unrefvar */

#ifndef MB_CODES
#	define MB_CODES
#	define MB_FUNC_OK 0
#	define MB_FUNC_BYE 1001
#	define MB_FUNC_WARNING 1002
#	define MB_FUNC_ERR 1003
#	define MB_FUNC_END 1004
#	define MB_FUNC_SUSPEND 1005
#	define MB_PARSING_ERR 3001
#	define MB_LOOP_BREAK 5001
#	define MB_LOOP_CONTINUE 5002
#	define MB_SUB_RETURN 5101
#	define MB_EXTENDED_ABORT 9001
#endif /* MB_CODES */

#ifndef mb_check
#	define mb_check(__r) { int __hr = __r; if(__hr != MB_FUNC_OK) { return __hr; } }
#endif /* mb_check */

#ifndef mb_reg_fun
#	define mb_reg_fun(__s, __f) mb_register_func(__s, #__f, __f)
#endif /* mb_reg_fun */
#ifndef mb_rem_fun
#	define mb_rem_fun(__s, __f) mb_remove_func(__s, #__f)
#endif /* mb_rem_fun */

struct mb_interpreter_t;

typedef enum mb_error_e {
	SE_NO_ERR = 0,
	/** Common */
	SE_CM_MB_OPEN_FAILED,
	SE_CM_FUNC_EXISTS,
	SE_CM_FUNC_NOT_EXISTS,
	/** Parsing */
	SE_PS_FILE_OPEN_FAILED,
	SE_PS_SYMBOL_TOO_LONG,
	SE_PS_INVALID_CHAR,
	/** Running */
	SE_RN_NOT_SUPPORTED,
	SE_RN_EMPTY_PROGRAM,
	SE_RN_SYNTAX,
	SE_RN_INVALID_DATA_TYPE,
	SE_RN_TYPE_NOT_MATCH,
	SE_RN_ILLEGAL_BOUND,
	SE_RN_DIMENSION_TOO_MUCH,
	SE_RN_OPERATION_FAILED,
	SE_RN_DIMENSION_OUT_OF_BOUND,
	SE_RN_ARRAY_OUT_OF_BOUND,
	SE_RN_LABEL_NOT_EXISTS,
	SE_RN_NO_RETURN_POINT,
	SE_RN_COLON_EXPECTED,
	SE_RN_COMMA_OR_SEMICOLON_EXPECTED,
	SE_RN_ARRAY_IDENTIFIER_EXPECTED,
	SE_RN_OPEN_BRACKET_EXPECTED,
	SE_RN_CLOSE_BRACKET_EXPECTED,
	SE_RN_ARRAY_SUBSCRIPT_EXPECTED,
	SE_RN_STRUCTURE_NOT_COMPLETED,
	SE_RN_FUNCTION_EXPECTED,
	SE_RN_STRING_EXPECTED,
	SE_RN_VAR_OR_ARRAY_EXPECTED,
	SE_RN_ASSIGN_OPERATOR_EXPECTED,
	SE_RN_INTEGER_EXPECTED,
	SE_RN_ELSE_EXPECTED,
	SE_RN_TO_EXPECTED,
	SE_RN_UNTIL_EXPECTED,
	SE_RN_LOOP_VAR_EXPECTED,
	SE_RN_JUMP_LABEL_EXPECTED,
	SE_RN_INVALID_ID_USAGE,
	SE_RN_CALCULATION_ERROR,
	SE_RN_DIVIDE_BY_ZERO,
	SE_RN_INVALID_EXPRESSION,
	SE_RN_OUT_OF_MEMORY,
	/** Extended abort */
	SE_EA_EXTENDED_ABORT,
} mb_error_e;

typedef enum mb_data_e {
	MB_DT_NIL = -1,
	MB_DT_INT = 0,
	MB_DT_REAL,
	MB_DT_STRING,
} mb_data_e;

typedef union mb_value_u {
	int_t integer;
	real_t float_point;
	char* string;
} mb_value_u;

typedef struct mb_value_t {
	mb_data_e type;
	mb_value_u value;
} mb_value_t;

typedef void (* mb_error_handler_t)(struct mb_interpreter_t*, enum mb_error_e, char*, int, unsigned short, unsigned short, int);
typedef int (* mb_func_t)(struct mb_interpreter_t*, void**);
typedef int (* mb_print_func_t)(const char*, ...);

typedef struct mb_interpreter_t {
	void* local_func_dict;
	void* global_func_dict;
	void* global_var_dict;
	void* ast;
	void* parsing_context;
	void* running_context;
	mb_error_e last_error;
	int last_error_pos;
	unsigned short last_error_row;
	unsigned short last_error_col;
	mb_error_handler_t error_handler;
	mb_print_func_t printer;
	void* userdata;
} mb_interpreter_t;

MBAPI unsigned int mb_ver(void);
MBAPI const char* mb_ver_string(void);

MBAPI int mb_init(void);
MBAPI int mb_dispose(void);
MBAPI int mb_open(mb_interpreter_t** s);
MBAPI int mb_close(mb_interpreter_t** s);
MBAPI int mb_reset(mb_interpreter_t** s, bool_t clrf);

MBAPI int mb_register_func(mb_interpreter_t* s, const char* n, mb_func_t f);
MBAPI int mb_remove_func(mb_interpreter_t* s, const char* n);

MBAPI int mb_attempt_func_begin(mb_interpreter_t* s, void** l);
MBAPI int mb_attempt_func_end(mb_interpreter_t* s, void** l);
MBAPI int mb_attempt_open_bracket(mb_interpreter_t* s, void** l);
MBAPI int mb_attempt_close_bracket(mb_interpreter_t* s, void** l);
MBAPI int mb_pop_int(mb_interpreter_t* s, void** l, int_t* val);
MBAPI int mb_pop_real(mb_interpreter_t* s, void** l, real_t* val);
MBAPI int mb_pop_string(mb_interpreter_t* s, void** l, char** val);
MBAPI int mb_pop_value(mb_interpreter_t* s, void** l, mb_value_t* val);
MBAPI int mb_push_int(mb_interpreter_t* s, void** l, int_t val);
MBAPI int mb_push_real(mb_interpreter_t* s, void** l, real_t val);
MBAPI int mb_push_string(mb_interpreter_t* s, void** l, char* val);
MBAPI int mb_push_value(mb_interpreter_t* s, void** l, mb_value_t val);

MBAPI int mb_load_string(mb_interpreter_t* s, const char* l);
MBAPI int mb_load_file(mb_interpreter_t* s, const char* f);
MBAPI int mb_run(mb_interpreter_t* s);
MBAPI int mb_suspend(mb_interpreter_t* s, void** l);

MBAPI mb_error_e mb_get_last_error(mb_interpreter_t* s);
MBAPI const char* mb_get_error_desc(mb_error_e err);
MBAPI int mb_set_error_handler(mb_interpreter_t* s, mb_error_handler_t h);
MBAPI int mb_set_printer(mb_interpreter_t* s, mb_print_func_t p);

MBAPI void mb_set_user_data(mb_interpreter_t* s, void *ptr);
MBAPI void *mb_get_user_data(mb_interpreter_t* s);
	       
#ifdef MB_COMPACT_MODE
#	pragma pack()
#endif /* MB_COMPACT_MODE */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __MY_BASIC_H__ */
