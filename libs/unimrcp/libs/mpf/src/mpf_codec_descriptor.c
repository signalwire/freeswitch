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

#include "mpf_codec_descriptor.h"

/** Match two codec descriptors */
MPF_DECLARE(apt_bool_t) mpf_codec_descriptor_match(const mpf_codec_descriptor_t *descriptor1, const mpf_codec_descriptor_t *descriptor2)
{
	apt_bool_t match = FALSE;
	if(descriptor1->payload_type < 96 && descriptor2->payload_type < 96) {
		if(descriptor1->payload_type == descriptor2->payload_type) {
			match = TRUE;
		}
	}
	else {
		if(apt_string_compare(&descriptor1->name,&descriptor2->name) == TRUE) {
			if(descriptor1->sampling_rate == descriptor2->sampling_rate && 
				descriptor1->channel_count == descriptor2->channel_count) {
				match = TRUE;
			}
		}
	}
	return match;
}

/** Intersect two codec lists */
MPF_DECLARE(apt_bool_t) mpf_codec_list_intersect(mpf_codec_list_t *codec_list1, mpf_codec_list_t *codec_list2)
{
	int i;
	int j;
	mpf_codec_descriptor_t *descriptor1;
	mpf_codec_descriptor_t *descriptor2;
	codec_list1->preffered = NULL;
	codec_list2->preffered = NULL;
	/* find only one match, set the matched codec as preffered, disable the others */
	for(i=0; i<codec_list1->descriptor_arr->nelts; i++) {
		descriptor1 = (mpf_codec_descriptor_t*)codec_list1->descriptor_arr->elts + i;
		if(codec_list1->preffered) {
			descriptor1->enabled = FALSE;
			continue;
		}

		for(j=0; j<codec_list2->descriptor_arr->nelts; j++) {
			descriptor2 = (mpf_codec_descriptor_t*)codec_list2->descriptor_arr->elts + j;

			descriptor1->enabled = mpf_codec_descriptor_match(descriptor1,descriptor2);
			if(descriptor1->enabled == TRUE) {
				codec_list1->preffered = descriptor1;
				codec_list2->preffered = descriptor2;
				break;
			}
		}
	}
	for(j=0; j<codec_list2->descriptor_arr->nelts; j++) {
		descriptor2 = (mpf_codec_descriptor_t*)codec_list2->descriptor_arr->elts + j;
		descriptor2->enabled = (codec_list2->preffered == descriptor2) ? TRUE : FALSE;
	}

	return TRUE;
}
