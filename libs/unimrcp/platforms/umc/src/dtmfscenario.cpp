/*
 * Copyright 2008-2014 Arsen Chaloyan
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
 * 
 * $Id: dtmfscenario.cpp 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#include <stdlib.h>
#include "dtmfscenario.h"
#include "dtmfsession.h"
#include "apt_log.h"

DtmfScenario::DtmfScenario() :
	m_ContentType(NULL),
	m_Grammar(NULL),
	m_Digits(NULL)
{
}

DtmfScenario::~DtmfScenario()
{
}

void DtmfScenario::Destroy()
{
}

bool DtmfScenario::LoadElement(const apr_xml_elem* pElem, apr_pool_t* pool)
{
	if(UmcScenario::LoadElement(pElem,pool))
		return true;
	
	if(strcasecmp(pElem->name,"recognize") == 0)
	{
		LoadRecognize(pElem,pool);
		return true;
	}
		
	return false;
}

bool DtmfScenario::LoadRecognize(const apr_xml_elem* pElem, apr_pool_t* pool)
{
	const apr_xml_attr* pAttr;
	for(pAttr = pElem->attr; pAttr; pAttr = pAttr->next) 
	{
		if(strcasecmp(pAttr->name,"content-type") == 0)
		{
			m_ContentType = pAttr->value;
		}
		else if(strcasecmp(pAttr->name,"grammar") == 0)
		{
			m_Grammar = pAttr->value;
		}
		else if(strcasecmp(pAttr->name,"digits") == 0)
		{
			m_Digits = pAttr->value;
		}
	}

	return true;
}


UmcSession* DtmfScenario::CreateSession()
{
	return new DtmfSession(this);
}
