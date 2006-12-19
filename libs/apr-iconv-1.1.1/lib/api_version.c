/* Copyright 2000-2004 The Apache Software Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "apr_iconv.h"
#include "api_version.h"

API_DECLARE(void) api_version(apr_version_t *pvsn)
{
    pvsn->major = API_MAJOR_VERSION;
    pvsn->minor = API_MINOR_VERSION;
    pvsn->patch = API_PATCH_VERSION;
#ifdef API_IS_DEV_VERSION
    pvsn->is_dev = 1;
#else
    pvsn->is_dev = 0;
#endif
}

API_DECLARE(const char *) api_version_string(void)
{
    return API_VERSION_STRING;
}
