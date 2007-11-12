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

set config=\Debug\
:: set config=\Release\

tests\torture_su_alloc%config%torture_su_alloc.exe -a
@if errorlevel 1 ( echo torture_su_alloc: FAIL ) else echo torture_su_alloc: PASS

tests\torture_su_root%config%torture_su_root.exe -a
@if errorlevel 1 ( echo torture_su_root: FAIL ) else echo torture_su_root: PASS

tests\torture_su_tag%config%torture_su_tag.exe -a
@if errorlevel 1 ( echo torture_su_tag: FAIL ) else echo torture_su_tag: PASS

tests\test_su%config%test_su.exe
@if errorlevel 1 ( echo test_su: FAIL ) else echo test_su: PASS

tests\torture_su_time%config%torture_su_time.exe -a
@if errorlevel 1 ( echo torture_su_time: FAIL ) else echo torture_su_time: PASS

tests\torture_su_timer%config%torture_su_timer.exe
@if errorlevel 1 ( echo torture_su_timer: FAIL ) else echo torture_su_timer: PASS

tests\torture_su%config%torture_su.exe -a
@if errorlevel 1 ( echo torture_su: FAIL ) else echo torture_su: PASS

tests\test_memmem%config%test_memmem.exe -a
@if errorlevel 1 ( echo test_memmem: FAIL ) else echo test_memmem: PASS

tests\test_tport%config%test_tport.exe -a
@if errorlevel 1 ( echo test_tport: FAIL ) else echo test_tport: PASS

tests\test_nta%config%test_nta.exe -a
@if errorlevel 1 ( echo test_nta: FAIL ) else echo test_nta: PASS

tests\test_nua%config%test_nua.exe -a
@if errorlevel 1 ( echo test_nua: FAIL ) else echo test_nua: PASS

tests\test_htable%config%test_htable.exe -a
@if errorlevel 1 ( echo test_htable: FAIL ) else echo test_htable: PASS

tests\torture_rbtree%config%torture_rbtree.exe -a
@if errorlevel 1 ( echo torture_rbtree: FAIL ) else echo torture_rbtree: PASS

tests\torture_su_bm%config%torture_su_bm.exe -a
@if errorlevel 1 ( echo torture_su_bm: FAIL ) else echo torture_su_bm: PASS

:: tests\torture_su_port%config%torture_su_port.exe -a
:: @if errorlevel 1 ( echo torture_su_port: FAIL ) else echo torture_su_port: PASS
