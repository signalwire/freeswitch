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
#include "recogscenario.h"
#include "recogsession.h"
#include "mrcp_message.h"
#include "mrcp_generic_header.h"
#include "mrcp_recog_header.h"
#include "mrcp_recog_resource.h"
#include "apt_log.h"

RecogScenario::RecogScenario() :
	m_DefineGrammar(true),
	m_Recognize(true),
	m_ContentType("application/srgs+xml"),
	m_Content(NULL),
	m_ContentLength(0),
	m_AudioSource(NULL)
{
}

RecogScenario::~RecogScenario()
{
}

void RecogScenario::Destroy()
{
}

bool RecogScenario::LoadElement(const apr_xml_elem* pElem, apr_pool_t* pool)
{
	if(UmcScenario::LoadElement(pElem,pool))
		return true;
	
	if(strcasecmp(pElem->name,"define-grammar") == 0)
	{
		LoadDefineGrammar(pElem,pool);
		return true;
	}
	else if(strcasecmp(pElem->name,"recognize") == 0)
	{
		LoadRecognize(pElem,pool);
		return true;
	}
		
	return false;
}

bool RecogScenario::LoadRecognize(const apr_xml_elem* pElem, apr_pool_t* pool)
{
	const apr_xml_attr* pAttr;
	for(pAttr = pElem->attr; pAttr; pAttr = pAttr->next) 
	{
		if(strcasecmp(pAttr->name,"enable") == 0)
		{
			m_Recognize = atoi(pAttr->value) > 0;
		}
		else if(strcasecmp(pAttr->name,"content-type") == 0)
		{
			m_ContentType = pAttr->value;
		}
		else if(strcasecmp(pAttr->name,"content-location") == 0)
		{
			m_Content = LoadFileContent(pAttr->value,m_ContentLength,pool);
		}
		else if(strcasecmp(pAttr->name,"audio-source") == 0)
		{
			m_AudioSource = pAttr->value;
		}
	}

	return true;
}

bool RecogScenario::LoadDefineGrammar(const apr_xml_elem* pElem, apr_pool_t* pool)
{
	const apr_xml_attr* pAttr;
	for(pAttr = pElem->attr; pAttr; pAttr = pAttr->next) 
	{
		if(strcasecmp(pAttr->name,"enable") == 0)
		{
			m_DefineGrammar = atoi(pAttr->value) > 0;
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

UmcSession* RecogScenario::CreateSession()
{
	return new RecogSession(this);
}
