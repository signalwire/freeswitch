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
#include "recorderscenario.h"
#include "recordersession.h"

RecorderScenario::RecorderScenario() :
	m_Record(true),
	m_AudioSource(NULL)
{
}

RecorderScenario::~RecorderScenario()
{
}

void RecorderScenario::Destroy()
{
}

bool RecorderScenario::LoadElement(const apr_xml_elem* pElem, apr_pool_t* pool)
{
	if(UmcScenario::LoadElement(pElem,pool))
		return true;
	
	if(strcasecmp(pElem->name,"record") == 0)
	{
		LoadRecord(pElem,pool);
		return true;
	}
		
	return false;
}

bool RecorderScenario::LoadRecord(const apr_xml_elem* pElem, apr_pool_t* pool)
{
	const apr_xml_attr* pAttr;
	for(pAttr = pElem->attr; pAttr; pAttr = pAttr->next) 
	{
		if(strcasecmp(pAttr->name,"enable") == 0)
		{
			m_Record = atoi(pAttr->value) > 0;
		}
		else if(strcasecmp(pAttr->name,"audio-source") == 0)
		{
			m_AudioSource = pAttr->value;
		}
	}

	return true;
}

UmcSession* RecorderScenario::CreateSession()
{
	return new RecorderSession(this);
}
