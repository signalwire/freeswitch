/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2009 Nokia Corporation.
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

#include "config.h"

#include "s2util.h"

/* -- Delay scenarios --------------------------------------------------- */

static unsigned long time_offset;

extern void (*_su_time)(su_time_t *tv);

static void _su_time_fast_forwarder(su_time_t *tv)
{
  tv->tv_sec += time_offset;
}

void s2_fast_forward(unsigned long seconds,
		     su_root_t *root)
{
  if (_su_time == NULL)
    _su_time = _su_time_fast_forwarder;

  time_offset += seconds;

  if (root)
    su_root_step(root, 0);
}

