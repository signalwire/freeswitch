/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthmct@yahoo.com>
 *
 * switch_sqlite.h -- Sqlite Header
 *
 */
/*! \file switch_sqlite.h
    \brief Sqlite Header
*/
#ifndef SWITCH_SQLITE_H
#define SWITCH_SQLITE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sqlite3.h>

/**
 * @defgroup switch_sqlite_top Brought To You By SQLite
 * @ingroup FREESWITCH 
 * @{
 */

/**
 * @defgroup switch_sqlite Database Routines
 * @ingroup switch_sqlite_top 
 * @{
 */

/**
 * Each open sqlite database is represented by an instance of the
 * following opaque structure.
*/
typedef sqlite3 switch_core_db;

#define switch_core_db_aggregate_context sqlite3_aggregate_context
#define switch_core_db_aggregate_count sqlite3_aggregate_count
#define switch_core_db_bind_blob sqlite3_bind_blob
#define switch_core_db_bind_double sqlite3_bind_double
#define switch_core_db_bind_int sqlite3_bind_int
#define switch_core_db_bind_int64 sqlite3_bind_int64
#define switch_core_db_bind_null sqlite3_bind_null
#define switch_core_db_bind_parameter_count sqlite3_bind_parameter_count
#define switch_core_db_bind_parameter_index sqlite3_bind_parameter_index
#define switch_core_db_bind_parameter_name sqlite3_bind_parameter_name
#define switch_core_db_bind_text sqlite3_bind_text
#define switch_core_db_bind_text16 sqlite3_bind_text16
#define switch_core_db_btree_trace sqlite3_btree_trace
#define switch_core_db_busy_handler sqlite3_busy_handler
#define switch_core_db_busy_timeout sqlite3_busy_timeout
#define switch_core_db_changes sqlite3_changes
#define switch_core_db_close sqlite3_close
#define switch_core_db_collation_needed sqlite3_collation_needed
#define switch_core_db_collation_needed16 sqlite3_collation_needed16
#define switch_core_db_column_blob sqlite3_column_blob
#define switch_core_db_column_bytes sqlite3_column_bytes
#define switch_core_db_column_bytes16 sqlite3_column_bytes16
#define switch_core_db_column_count sqlite3_column_count
#define switch_core_db_column_decltype sqlite3_column_decltype
#define switch_core_db_column_decltype16 sqlite3_column_decltype16
#define switch_core_db_column_double sqlite3_column_double
#define switch_core_db_column_int sqlite3_column_int
#define switch_core_db_column_int64 sqlite3_column_int64
#define switch_core_db_column_name sqlite3_column_name
#define switch_core_db_column_name16 sqlite3_column_name16
#define switch_core_db_column_text sqlite3_column_text
#define switch_core_db_column_text16 sqlite3_column_text16
#define switch_core_db_column_type sqlite3_column_type
#define switch_core_db_commit_hook sqlite3_commit_hook
#define switch_core_db_complete sqlite3_complete
#define switch_core_db_complete16 sqlite3_complete16
#define switch_core_db_create_collation sqlite3_create_collation
#define switch_core_db_create_collation16 sqlite3_create_collation16
#define switch_core_db_create_function sqlite3_create_function
#define switch_core_db_create_function16 sqlite3_create_function16
#define switch_core_db_data_count sqlite3_data_count
#define switch_core_db_db_handle sqlite3_db_handle
#define switch_core_db_errcode sqlite3_errcode
#define switch_core_db_errmsg sqlite3_errmsg
#define switch_core_db_errmsg16 sqlite3_errmsg16
#define switch_core_db_exec sqlite3_exec
#define switch_core_db_expired sqlite3_expired
#define switch_core_db_finalize sqlite3_finalize
#define switch_core_db_free sqlite3_free
#define switch_core_db_free_table sqlite3_free_table
#define switch_core_db_get_autocommit sqlite3_get_autocommit
#define switch_core_db_get_auxdata sqlite3_get_auxdata
#define switch_core_db_get_table sqlite3_get_table
#define switch_core_db_get_table_cb sqlite3_get_table_cb
#define switch_core_db_global_recover sqlite3_global_recover
#define switch_core_db_interrupt sqlite3_interrupt
#define switch_core_db_interrupt_count sqlite3_interrupt_count
#define switch_core_db_last_insert_rowid sqlite3_last_insert_rowid
#define switch_core_db_libversion sqlite3_libversion
#define switch_core_db_libversion_number sqlite3_libversion_number
#define switch_core_db_malloc_failed sqlite3_malloc_failed
#define switch_core_db_mprintf sqlite3_mprintf
#define switch_core_db_open sqlite3_open
#define switch_core_db_open16 sqlite3_open16
#define switch_core_db_opentemp_count sqlite3_opentemp_count
#define switch_core_db_os_trace sqlite3_os_trace
#define switch_core_db_prepare sqlite3_prepare
#define switch_core_db_prepare16 sqlite3_prepare16
#define switch_core_db_profile sqlite3_profile
#define switch_core_db_progress_handler sqlite3_progress_handler
#define switch_core_db_reset sqlite3_reset
#define switch_core_db_result_blob sqlite3_result_blob
#define switch_core_db_result_double sqlite3_result_double
#define switch_core_db_result_error sqlite3_result_error
#define switch_core_db_result_error16 sqlite3_result_error16
#define switch_core_db_result_int sqlite3_result_int
#define switch_core_db_result_int64 sqlite3_result_int64
#define switch_core_db_result_null sqlite3_result_null
#define switch_core_db_result_text sqlite3_result_text
#define switch_core_db_result_text16 sqlite3_result_text16
#define switch_core_db_result_text16be sqlite3_result_text16be
#define switch_core_db_result_text16le sqlite3_result_text16le
#define switch_core_db_result_value sqlite3_result_value
#define switch_core_db_search_count sqlite3_search_count
#define switch_core_db_set_authorizer sqlite3_set_authorizer
#define switch_core_db_set_auxdata sqlite3_set_auxdata
#define switch_core_db_snprintf sqlite3_snprintf
#define switch_core_db_sort_count sqlite3_sort_count
#define switch_core_db_step sqlite3_step
#define switch_core_db_temp_directory sqlite3_temp_directory
#define switch_core_db_total_changes sqlite3_total_changes
#define switch_core_db_trace sqlite3_trace
#define switch_core_db_transfer_bindings sqlite3_transfer_bindings
#define switch_core_db_user_data sqlite3_user_data
#define switch_core_db_value_blob sqlite3_value_blob
#define switch_core_db_value_bytes sqlite3_value_bytes
#define switch_core_db_value_bytes16 sqlite3_value_bytes16
#define switch_core_db_value_double sqlite3_value_double
#define switch_core_db_value_int sqlite3_value_int
#define switch_core_db_value_int64 sqlite3_value_int64
#define switch_core_db_value_text sqlite3_value_text
#define switch_core_db_value_text16 sqlite3_value_text16
#define switch_core_db_value_text16be sqlite3_value_text16be
#define switch_core_db_value_text16le sqlite3_value_text16le
#define switch_core_db_value_type sqlite3_value_type
#define switch_core_db_version sqlite3_version
#define switch_core_db_vmprintf sqlite3_vmprintf

/** @} */
/** @} */



#ifdef __cplusplus
}
#endif

#endif
