::
:: Collect files to be installed to install directory
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

set name=sofia-sip
set major=1.11
set config=debug

:: Uncomment this if you want release
:: set config=release

set destdir=..\..\%name%-%major%-%config%
set includedir=%destdir%\include
set sofiadir=%includedir%\sofia-sip
set libdir=%destdir%\lib
mkdir %destdir% %includedir% %sofiadir% %libdir%

::
:: Copy docs
::
set docs=README AUTHORS COPYING COPYRIGHTS README.developers RELEASE TODO ChangeLog
for %%f in (%docs%) do xcopy /Y ..\%%f %destdir%

::
:: Copy headers
::
set SUBDIRS=su features bnf sresolv sdp url msg sip stun ipt soa tport http nta nea iptsec nth nua

xcopy /Y sofia-sip\*.h %sofiadir%
for %%s in (%SUBDIRS%) do xcopy /Y ..\libsofia-sip-ua\%%s\sofia-sip\*.h %sofiadir%

xcopy /Y pthread\.*.h %includedir%

::
:: Copy libraries
::

::xcopy /Y libsofia-sip-ua\%config%\libsofia_sip_ua.dll %libdir%
xcopy /Y libsofia-sip-ua\%config%\libsofia_sip_ua.lib %libdir%
xcopy /Y Pthread\*.dll %libdir%
xcopy /Y Pthread\*.lib %libdir%
