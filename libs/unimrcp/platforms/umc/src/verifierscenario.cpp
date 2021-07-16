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
#include "verifierscenario.h"
#include "verifiersession.h"
#include "mrcp_message.h"
#include "mrcp_generic_header.h"
#include "mrcp_recog_header.h"
#include "mrcp_recog_resource.h"
#include "apt_log.h"

VerifierScenario::VerifierScenario() :
	m_RepositoryURI(NULL),
	m_VerificationMode(NULL),
	m_VoiceprintIdentifier(NULL)
{
}

VerifierScenario::~VerifierScenario()
{
}

void VerifierScenario::Destroy()
{
}

bool VerifierScenario::LoadElement(const apr_xml_elem* pElem, apr_pool_t* pool)
{
	if(UmcScenario::LoadElement(pElem,pool))
		return true;
			
	if(strcasecmp(pElem->name,"verify") == 0)
	{
		LoadVerify(pElem,pool);
		return true;
	}
	return false;
}

bool VerifierScenario::LoadVerify(const apr_xml_elem* pElem, apr_pool_t* pool)
{
	const apr_xml_attr* pAttr;
	for(pAttr = pElem->attr; pAttr; pAttr = pAttr->next) 
	{
		if(strcasecmp(pAttr->name,"repository-uri") == 0)
		{
			m_RepositoryURI = pAttr->value;
		}
		else if(strcasecmp(pAttr->name,"verification-mode") == 0)
		{
			m_VerificationMode = pAttr->value;
		}
		else if(strcasecmp(pAttr->name,"voiceprint-identifier") == 0)
		{
			m_VoiceprintIdentifier = pAttr->value;
		}
	}

	return true;
}


UmcSession* VerifierScenario::CreateSession()
{
	return new VerifierSession(this);
}
