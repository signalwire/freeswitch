:: Run test programs
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
tests\torture_su_alloc\Debug\torture_su_alloc.exe
@if errorlevel 1 ( echo torture_su_alloc: FAIL ) else echo torture_su_alloc: PASS

tests\torture_su_root\Debug\torture_su_root.exe
@if errorlevel 1 ( echo torture_su_root: FAIL ) else echo torture_su_root: PASS

tests\torture_su_tag\Debug\torture_su_tag.exe
@if errorlevel 1 ( echo torture_su_tag: FAIL ) else echo torture_su_tag: PASS

tests\test_su\Debug\test_su.exe
@if errorlevel 1 ( echo test_su: FAIL ) else echo test_su: PASS

tests\torture_su_time\Debug\torture_su_time.exe
@if errorlevel 1 ( echo torture_su_time: FAIL ) else echo torture_su_time: PASS

tests\torture_su_timer\Debug\torture_su_timer.exe
@if errorlevel 1 ( echo torture_su_timer: FAIL ) else echo torture_su_timer: PASS

tests\torture_su\Debug\torture_su.exe
@if errorlevel 1 ( echo torture_su: FAIL ) else echo torture_su: PASS

tests\test_memmem\Debug\test_memmem.exe
@if errorlevel 1 ( echo test_memmem: FAIL ) else echo test_memmem: PASS

tests\test_tport\Debug\test_tport.exe
@if errorlevel 1 ( echo test_tport: FAIL ) else echo test_tport: PASS

tests\test_nta\Debug\test_nta.exe
@if errorlevel 1 ( echo test_nta: FAIL ) else echo test_nta: PASS

tests\test_nua\Debug\test_nua.exe
@if errorlevel 1 ( echo test_nua: FAIL ) else echo test_nua: PASS

tests\test_htable\Debug\test_htable.exe
@if errorlevel 1 ( echo test_htable: FAIL ) else echo test_htable: PASS

tests\torture_rbtree\Debug\torture_rbtree.exe
@if errorlevel 1 ( echo torture_rbtree: FAIL ) else echo torture_rbtree: PASS

tests\torture_su_bm\Debug\torture_su_bm.exe
@if errorlevel 1 ( echo torture_su_bm: FAIL ) else echo torture_su_bm: PASS

tests\torture_su_port\Debug\torture_su_port.exe
@if errorlevel 1 ( echo torture_su_port: FAIL ) else echo torture_su_port: PASS
