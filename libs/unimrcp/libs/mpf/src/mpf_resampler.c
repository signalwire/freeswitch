/*
 * Copyright 2008-2015 Arsen Chaloyan
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

#include "mpf_resampler.h"
#include "apt_log.h"

MPF_DECLARE(mpf_audio_stream_t*) mpf_resampler_create(mpf_audio_stream_t *source, mpf_audio_stream_t *sink, apr_pool_t *pool)
{
	apt_log(MPF_LOG_MARK,APT_PRIO_WARNING,
		"Currently resampling is not supported. "
		"Try to configure and use the same sampling rate on both ends");
	return NULL;
}
