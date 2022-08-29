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

#include <stdlib.h>
#include "synthscenario.h"
#include "synthsession.h"
#include "mrcp_message.h"
#include "mrcp_generic_header.h"
#include "mrcp_synth_header.h"
#include "mrcp_synth_resource.h"


SynthScenario::SynthScenario() :
	m_Speak(true),
	m_SpeechLanguage(NULL),
	m_ContentType("application/synthesis+ssml"),
	m_Content(NULL),
	m_ContentLength(0)
{
}

SynthScenario::~SynthScenario()
{
}

void SynthScenario::Destroy()
{
}

bool SynthScenario::LoadElement(const apr_xml_elem* pElem, apr_pool_t* pool)
{
	if(UmcScenario::LoadElement(pElem,pool))
		return true;
	
	if(strcasecmp(pElem->name,"speak") == 0)
	{
		LoadSpeak(pElem,pool);
		return true;
	}
		
	return false;
}

bool SynthScenario::LoadSpeak(const apr_xml_elem* pElem, apr_pool_t* pool)
{
	const apr_xml_attr* pAttr;
	for(pAttr = pElem->attr; pAttr; pAttr = pAttr->next) 
	{
		if(strcasecmp(pAttr->name,"enable") == 0)
		{
			m_Speak = atoi(pAttr->value) > 0;
		}
		else if (strcasecmp(pAttr->name, "speech-language") == 0)
		{
			m_SpeechLanguage = pAttr->value;
		}
		else if(strcasecmp(pAttr->name,"content-type") == 0)
		{
			m_ContentType = pAttr->value;
		}
		else if(strcasecmp(pAttr->name,"content-location") == 0)
		{
			m_Content = LoadFileContent(pAttr->value,m_ContentLength,pool);
		}
	}

	return true;
}

UmcSession* SynthScenario::CreateSession()
{
	return new SynthSession(this);
}
