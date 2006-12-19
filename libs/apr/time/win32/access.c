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

#include "win32/apr_arch_atime.h"
#include "apr_time.h"
#include "apr_general.h"
#include "apr_lib.h"

apr_status_t apr_get_curtime(struct atime_t *time, apr_time_t *rv)
{
    if (time) {
        (*rv) = time->currtime;
        return APR_SUCCESS;
    }
    return APR_ENOTIME;    
}

apr_status_t apr_get_sec(struct atime_t *time, apr_int32_t *rv)
{
    if (time) {
        (*rv) = time->explodedtime->wSecond;
        return APR_SUCCESS;
    }
    return APR_ENOTIME;
}

apr_status_t apr_get_min(struct atime_t *time, apr_int32_t *rv)
{
    if (time) {
        (*rv) = time->explodedtime->wMinute;
        return APR_SUCCESS;
    }
    return APR_ENOTIME;
}

apr_status_t apr_get_hour(struct atime_t *time, apr_int32_t *rv)
{
    if (time) {
        (*rv) = time->explodedtime->wHour;
        return APR_SUCCESS;
    }
    return APR_ENOTIME;
}

apr_status_t apr_get_mday(struct atime_t *time, apr_int32_t *rv)
{
    if (time) {
        (*rv) = time->explodedtime->wDay;
        return APR_SUCCESS;
    }
    return APR_ENOTIME;
}

apr_status_t apr_get_mon(struct atime_t *time, apr_int32_t *rv)
{
    if (time) {
        (*rv) = time->explodedtime->wMonth;
        return APR_SUCCESS;
    }
    return APR_ENOTIME;
}

apr_status_t apr_get_year(struct atime_t *time, apr_int32_t *rv)
{
    if (time) {
        (*rv) = time->explodedtime->wYear;
        return APR_SUCCESS;
    }
    return APR_ENOTIME;
}

apr_status_t apr_get_wday(struct atime_t *time, apr_int32_t *rv)
{
    if (time) {
        (*rv) = time->explodedtime->wDayOfWeek;
        return APR_SUCCESS;
    }
    return APR_ENOTIME;
}

apr_status_t apr_set_sec(struct atime_t *time, apr_int32_t value)
{
    if (!time) {
        return APR_ENOTIME;
    }
    if (time->explodedtime == NULL) {
        time->explodedtime = (SYSTEMTIME *)apr_pcalloc(time->cntxt, 
                              sizeof(SYSTEMTIME));
    }
    if (time->explodedtime == NULL) {
        return APR_ENOMEM;
    }
    time->explodedtime->wSecond = value;
    return APR_SUCCESS;
}

apr_status_t apr_set_min(struct atime_t *time, apr_int32_t value)
{
    if (!time) {
        return APR_ENOTIME;
    }
    if (time->explodedtime == NULL) {
        time->explodedtime = (SYSTEMTIME *)apr_pcalloc(time->cntxt, 
                              sizeof(SYSTEMTIME));
    }
    if (time->explodedtime == NULL) {
        return APR_ENOMEM;
    }
    time->explodedtime->wMinute = value;
    return APR_SUCCESS;
}

apr_status_t apr_set_hour(struct atime_t *time, apr_int32_t value)
{
    if (!time) {
        return APR_ENOTIME;
    }
    if (time->explodedtime == NULL) {
        time->explodedtime = (SYSTEMTIME *)apr_pcalloc(time->cntxt, 
                              sizeof(SYSTEMTIME));
    }
    if (time->explodedtime == NULL) {
        return APR_ENOMEM;
    }
    time->explodedtime->wHour = value;
    return APR_SUCCESS;
}

apr_status_t apr_set_mday(struct atime_t *time, apr_int32_t value)
{
    if (!time) {
        return APR_ENOTIME;
    }
    if (time->explodedtime == NULL) {
        time->explodedtime = (SYSTEMTIME *)apr_pcalloc(time->cntxt, 
                              sizeof(SYSTEMTIME));
    }
    if (time->explodedtime == NULL) {
        return APR_ENOMEM;
    }
    time->explodedtime->wDay = value;
    return APR_SUCCESS;
}

apr_status_t apr_set_mon(struct atime_t *time, apr_int32_t value)
{
    if (!time) {
        return APR_ENOTIME;
    }
    if (time->explodedtime == NULL) {
        time->explodedtime = (SYSTEMTIME *)apr_pcalloc(time->cntxt, 
                              sizeof(SYSTEMTIME));
    }
    if (time->explodedtime == NULL) {
        return APR_ENOMEM;
    }
    time->explodedtime->wMonth = value;
    return APR_SUCCESS;
}

apr_status_t apr_set_year(struct atime_t *time, apr_int32_t value)
{
    if (!time) {
        return APR_ENOTIME;
    }
    if (time->explodedtime == NULL) {
        time->explodedtime = (SYSTEMTIME *)apr_pcalloc(time->cntxt, 
                              sizeof(SYSTEMTIME));
    }
    if (time->explodedtime == NULL) {
        return APR_ENOMEM;
    }
    time->explodedtime->wYear = value;
    return APR_SUCCESS;
}

apr_status_t apr_set_wday(struct atime_t *time, apr_int32_t value)
{
    if (!time) {
        return APR_ENOTIME;
    }
    if (time->explodedtime == NULL) {
        time->explodedtime = (SYSTEMTIME *)apr_pcalloc(time->cntxt, 
                              sizeof(SYSTEMTIME));
    }
    if (time->explodedtime == NULL) {
        return APR_ENOMEM;
    }
    time->explodedtime->wDayOfWeek = value;
    return APR_SUCCESS;
}
