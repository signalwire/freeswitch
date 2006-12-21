::
:: Prepare pristine Sofia SIP source tree for Visual C
::
:: NOTE: this script requires gawk - see http://unxutils.sourceforge.net
::
:: This file is part of the Sofia-SIP package
::
:: Copyright (C) 2006 Nokia Corporation.
::
:: Contact: Pekka Pessi <pekka.pessi@nokia.com>
::
:: This library is free software; you can redistribute it and/or
:: modify it under the terms of the GNU Lesser General Public License
:: as published by the Free Software Foundation; either version 2.1 of
:: the License, or (at your option) any later version.
::
:: This library is distributed in the hope that it will be useful, but
:: WITHOUT ANY WARRANTY; without even the implied warranty of
:: MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
:: Lesser General Public License for more details.
::
:: You should have received a copy of the GNU Lesser General Public
:: License along with this library; if not, write to the Free Software
:: Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
:: 02110-1301 USA
::

@setlocal
@if x%AWK%==x set AWK=gawk

@call version_files.cmd

@call build_sources.cmd

@echo.
@echo NOTE:
@echo NOTE: Remember to install pthreadVC2.dll to your path, too!
@echo NOTE:
@endlocal