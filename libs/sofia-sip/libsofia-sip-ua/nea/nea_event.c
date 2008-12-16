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

/**@internal
 * @file nea_event.c
 * @brief Default MIME type for certain events.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Thu Dec 11 20:28:46 2003 ppessi
 */

#include "config.h"

#include <stddef.h>
#include <string.h>

char const *nea_default_content_type(char const *event)
{
  char const *template = strrchr(event, '.');

  if (strcmp(event, "presence") == 0)
    return "application/pidf+xml";
  else if (strcmp(event, "cpl") == 0)
    return "application/cpl+xml";
  else if (strcmp(event, "reg") == 0)
    return "application/reginfo+xml";
  else if (strcmp(event, "presencelist") == 0)
    return "application/cpim-plidf+xml";
  else if (strcmp(event, "message-summary") == 0)
    return "application/simple-message-summary";
  else if (template && strcmp(template, ".acl") == 0)
    return "application/vnd.nokia-acl+xml";
  else if (template && strcmp(template, ".winfo") == 0)
    return "application/watcherinfo+xml";
  else if (template && strcmp(template, ".list") == 0)
    return "application/rlmi+xml";
  else if (strcmp(event, "rlmi") == 0)
    return "application/rlmi+xml";
  else
    return NULL;
}
