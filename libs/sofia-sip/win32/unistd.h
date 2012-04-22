/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2006 Nokia Corporation.
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

/* Dummy unistd.h for win32 */

#include <io.h>
#include <fcntl.h>
#include <stdio.h>

#define write(fd, buf, len) _write((fd), (buf), (unsigned int)(len))
#define read(fd, buf, len)  _read((fd), (buf), (len))
#define close(fd)           _close((fd))
#define mktemp(template)    _mktemp((template))
#define mkstemp(template)   _open(_mktemp(template), _O_RDWR|_O_CREAT, 0600)
#define unlink(name)        _unlink((name))
#define stat _stat

#define O_RDONLY        _O_RDONLY
#define O_WRONLY        _O_WRONLY
#define O_RDWR          _O_RDWR
#define O_APPEND        _O_APPEND
#define O_CREAT         _O_CREAT
#define O_TRUNC         _O_TRUNC
#define O_EXCL          _O_EXCL

#include <process.h>

#define getpid() _getpid()
