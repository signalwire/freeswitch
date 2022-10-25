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

#include "win32/fspr_arch_atime.h"
#include "fspr_time.h"
#include "fspr_general.h"
#include "fspr_lib.h"

fspr_status_t fspr_get_curtime(struct atime_t *time, fspr_time_t *rv)
{
    if (time) {
        (*rv) = time->currtime;
        return APR_SUCCESS;
    }
    return APR_ENOTIME;    
}

fspr_status_t fspr_get_sec(struct atime_t *time, fspr_int32_t *rv)
{
    if (time) {
        (*rv) = time->explodedtime->wSecond;
        return APR_SUCCESS;
    }
    return APR_ENOTIME;
}

fspr_status_t fspr_get_min(struct atime_t *time, fspr_int32_t *rv)
{
    if (time) {
        (*rv) = time->explodedtime->wMinute;
        return APR_SUCCESS;
    }
    return APR_ENOTIME;
}

fspr_status_t fspr_get_hour(struct atime_t *time, fspr_int32_t *rv)
{
    if (time) {
        (*rv) = time->explodedtime->wHour;
        return APR_SUCCESS;
    }
    return APR_ENOTIME;
}

fspr_status_t fspr_get_mday(struct atime_t *time, fspr_int32_t *rv)
{
    if (time) {
        (*rv) = time->explodedtime->wDay;
        return APR_SUCCESS;
    }
    return APR_ENOTIME;
}

fspr_status_t fspr_get_mon(struct atime_t *time, fspr_int32_t *rv)
{
    if (time) {
        (*rv) = time->explodedtime->wMonth;
        return APR_SUCCESS;
    }
    return APR_ENOTIME;
}

fspr_status_t fspr_get_year(struct atime_t *time, fspr_int32_t *rv)
{
    if (time) {
        (*rv) = time->explodedtime->wYear;
        return APR_SUCCESS;
    }
    return APR_ENOTIME;
}

fspr_status_t fspr_get_wday(struct atime_t *time, fspr_int32_t *rv)
{
    if (time) {
        (*rv) = time->explodedtime->wDayOfWeek;
        return APR_SUCCESS;
    }
    return APR_ENOTIME;
}

fspr_status_t fspr_set_sec(struct atime_t *time, fspr_int32_t value)
{
    if (!time) {
        return APR_ENOTIME;
    }
    if (time->explodedtime == NULL) {
        time->explodedtime = (SYSTEMTIME *)fspr_pcalloc(time->cntxt, 
                              sizeof(SYSTEMTIME));
    }
    if (time->explodedtime == NULL) {
        return APR_ENOMEM;
    }
    time->explodedtime->wSecond = value;
    return APR_SUCCESS;
}

fspr_status_t fspr_set_min(struct atime_t *time, fspr_int32_t value)
{
    if (!time) {
        return APR_ENOTIME;
    }
    if (time->explodedtime == NULL) {
        time->explodedtime = (SYSTEMTIME *)fspr_pcalloc(time->cntxt, 
                              sizeof(SYSTEMTIME));
    }
    if (time->explodedtime == NULL) {
        return APR_ENOMEM;
    }
    time->explodedtime->wMinute = value;
    return APR_SUCCESS;
}

fspr_status_t fspr_set_hour(struct atime_t *time, fspr_int32_t value)
{
    if (!time) {
        return APR_ENOTIME;
    }
    if (time->explodedtime == NULL) {
        time->explodedtime = (SYSTEMTIME *)fspr_pcalloc(time->cntxt, 
                              sizeof(SYSTEMTIME));
    }
    if (time->explodedtime == NULL) {
        return APR_ENOMEM;
    }
    time->explodedtime->wHour = value;
    return APR_SUCCESS;
}

fspr_status_t fspr_set_mday(struct atime_t *time, fspr_int32_t value)
{
    if (!time) {
        return APR_ENOTIME;
    }
    if (time->explodedtime == NULL) {
        time->explodedtime = (SYSTEMTIME *)fspr_pcalloc(time->cntxt, 
                              sizeof(SYSTEMTIME));
    }
    if (time->explodedtime == NULL) {
        return APR_ENOMEM;
    }
    time->explodedtime->wDay = value;
    return APR_SUCCESS;
}

fspr_status_t fspr_set_mon(struct atime_t *time, fspr_int32_t value)
{
    if (!time) {
        return APR_ENOTIME;
    }
    if (time->explodedtime == NULL) {
        time->explodedtime = (SYSTEMTIME *)fspr_pcalloc(time->cntxt, 
                              sizeof(SYSTEMTIME));
    }
    if (time->explodedtime == NULL) {
        return APR_ENOMEM;
    }
    time->explodedtime->wMonth = value;
    return APR_SUCCESS;
}

fspr_status_t fspr_set_year(struct atime_t *time, fspr_int32_t value)
{
    if (!time) {
        return APR_ENOTIME;
    }
    if (time->explodedtime == NULL) {
        time->explodedtime = (SYSTEMTIME *)fspr_pcalloc(time->cntxt, 
                              sizeof(SYSTEMTIME));
    }
    if (time->explodedtime == NULL) {
        return APR_ENOMEM;
    }
    time->explodedtime->wYear = value;
    return APR_SUCCESS;
}

fspr_status_t fspr_set_wday(struct atime_t *time, fspr_int32_t value)
{
    if (!time) {
        return APR_ENOTIME;
    }
    if (time->explodedtime == NULL) {
        time->explodedtime = (SYSTEMTIME *)fspr_pcalloc(time->cntxt, 
                              sizeof(SYSTEMTIME));
    }
    if (time->explodedtime == NULL) {
        return APR_ENOMEM;
    }
    time->explodedtime->wDayOfWeek = value;
    return APR_SUCCESS;
}
