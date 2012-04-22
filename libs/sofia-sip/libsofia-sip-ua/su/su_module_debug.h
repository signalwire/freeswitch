/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2005 Nokia Corporation.
 *
 * Contact: Pekka Pessi <pekka.pessi@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef SU_MODULE_DEBUG_H
/** Defined when <su_module_debug.h> has been included. */
#define SU_MODULE_DEBUG_H

/**@ingroup su_log
 * @internal @file su_module_debug.h
 * @brief Debug log for @b su module
 *
 * The su_module_debug.h defines a common debug log #su_log_global for all
 * functions within @b su module.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed Jan 29 18:18:49 2003 ppessi
 *
 */

#include <sofia-sip/su_log.h>

SOFIA_BEGIN_DECLS

/** Debugging log for @b su module. */
SOFIAPUBVAR su_log_t su_log_global[];
#define SU_LOG (su_log_global)
#include <sofia-sip/su_debug.h>

SOFIA_END_DECLS

#endif /* !defined SU_MODULE_DEBUG_H */
