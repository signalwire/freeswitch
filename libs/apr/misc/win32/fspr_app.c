/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Usage Notes:
 *
 *   this module, and the misc/win32/utf8.c modules must be 
 *   compiled APR_EXPORT_STATIC and linked to an application with
 *   the /entry:wmainCRTStartup flag.  This module becomes the true
 *   wmain entry point, and passes utf-8 reformatted argv and env
 *   arrays to the application's main function.
 *
 *   This module is only compatible with Unicode-only executables.
 *   Mixed (Win9x backwards compatible) binaries should refer instead
 *   to the fspr_startup.c module.
 *
 *   _dbg_malloc/realloc is used in place of the usual API, in order
 *   to convince the MSVCRT that they created these entities.  If we
 *   do not create them as _CRT_BLOCK entities, the crt will fault
 *   on an assert.  We are not worrying about the crt's locks here, 
 *   since we are single threaded [so far].
 */

#include "fspr_general.h"
#include "ShellAPI.h"
#include "crtdbg.h"
#include "wchar.h"
#include "fspr_arch_file_io.h"
#include "assert.h"
#include "fspr_private.h"
#include "fspr_arch_misc.h"

/* This symbol is _private_, although it must be exported.
 */

extern int main(int argc, const char **argv, const char **env);

int wmain(int argc, const wchar_t **wargv, const wchar_t **wenv)
{
    char **argv;
    char **env;
    int dupenv;

    (void)fspr_wastrtoastr(&argv, wargv, argc);

    dupenv = fspr_wastrtoastr(&env, wenv, -1);

    _environ = _malloc_dbg((dupenv + 1) * sizeof (char *), 
                           _CRT_BLOCK, __FILE__, __LINE__ );
    memcpy(_environ, env, (dupenv + 1) * sizeof (char *));

    /* MSVCRT will attempt to maintain the wide environment calls
     * on _putenv(), which is bogus if we've passed a non-ascii
     * string to _putenv(), since they use MultiByteToWideChar
     * and breaking the implicit utf-8 assumption we've built.
     *
     * Reset _wenviron for good measure.
     */
    if (_wenviron) {
        wenv = _wenviron;
        _wenviron = NULL;
        free((wchar_t **)wenv);
    }

    fspr_app_init_complete = 1;

    return main(argc, argv, env);
}
