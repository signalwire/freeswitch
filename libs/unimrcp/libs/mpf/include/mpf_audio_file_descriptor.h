/*
 * Copyright 2008 Arsen Chaloyan
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

#ifndef __MPF_AUDIO_FILE_DESCRIPTOR_H__
#define __MPF_AUDIO_FILE_DESCRIPTOR_H__

/**
 * @file mpf_audio_file_descriptor.h
 * @brief MPF Audio File Descriptor
 */ 

#include <stdio.h>
#include "mpf_stream_mode.h"
#include "mpf_codec_descriptor.h"

APT_BEGIN_EXTERN_C

/** FILE_READER is defined as STREAM_MODE_RECEIVE */
#define FILE_READER STREAM_MODE_RECEIVE
/** FILE_WRITER is defined as STREAM_MODE_SEND */
#define FILE_WRITER STREAM_MODE_SEND

/** Audio file descriptor declaration */
typedef struct mpf_audio_file_descriptor_t mpf_audio_file_descriptor_t;

/** Audio file descriptor */
struct mpf_audio_file_descriptor_t {
	/** Indicate what descriptor for (reader and/or write) */
	mpf_stream_mode_e      mask;
	/** Codec descriptor to use for audio file read/write */
	mpf_codec_descriptor_t codec_descriptor;
	/** File handle to read audio stream */
	FILE                  *read_handle;
	/** File handle to write audio stream */
	FILE                  *write_handle;
	/** Max size of file  */
	apr_size_t             max_write_size;
};

APT_END_EXTERN_C

#endif /*__MPF_AUDIO_FILE_DESCRIPTOR_H__*/
