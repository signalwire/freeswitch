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
#include "umcscenario.h"
#include "apt_log.h"

UmcScenario::UmcScenario() :
	m_pName(NULL),
	m_pMrcpProfile("uni2"),
	m_pDirLayout(NULL),
	m_ResourceDiscovery(false),
	m_pCapabilities(NULL),
	m_pRtpDescriptor(NULL)
{
}

UmcScenario::~UmcScenario()
{
}

bool UmcScenario::Load(const apr_xml_elem* pElem, apr_pool_t* pool)
{
	const apr_xml_elem* pChildElem;
	/* Load Child Elements */
	for(pChildElem = pElem->first_child; pChildElem; pChildElem = pChildElem->next)
	{
		if(!LoadElement(pChildElem,pool))
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Load Child Element %s",pChildElem->name);
	}
	return true;
}

void UmcScenario::Destroy()
{
}

bool UmcScenario::LoadElement(const apr_xml_elem* pElem, apr_pool_t* pool)
{
	if(strcasecmp(pElem->name,"resource-discovery") == 0)
	{
		LoadDiscovery(pElem,pool);
		return true;
	}
	else if(strcasecmp(pElem->name,"termination") == 0)
	{
		LoadTermination(pElem,pool);
		return true;
	}
	else if(strcasecmp(pElem->name,"rtp-termination") == 0)
	{
		LoadRtpTermination(pElem,pool);
		return true;
	}

	return false;
}

bool UmcScenario::LoadDiscovery(const apr_xml_elem* pElem, apr_pool_t* pool)
{
	m_ResourceDiscovery = IsElementEnabled(pElem);
	return true;
}

bool UmcScenario::LoadTermination(const apr_xml_elem* pElem, apr_pool_t* pool)
{
	if(!IsElementEnabled(pElem))
		return true;

	const apr_xml_elem* pChildElem;
	/* Load Child Elements */
	for(pChildElem = pElem->first_child; pChildElem; pChildElem = pChildElem->next)
	{
		if(strcasecmp(pChildElem->name,"capabilities") == 0)
			return LoadCapabilities(pChildElem,pool);
	}
	return true;
}

bool UmcScenario::LoadCapabilities(const apr_xml_elem* pElem, apr_pool_t* pool)
{
	const apr_xml_elem* pChildElem;
	/* Load Child Elements */
	m_pCapabilities = (mpf_codec_capabilities_t*) apr_palloc(pool,sizeof(mpf_codec_capabilities_t));
	mpf_codec_capabilities_init(m_pCapabilities,1,pool);
	for(pChildElem = pElem->first_child; pChildElem; pChildElem = pChildElem->next)
	{
		if(strcasecmp(pChildElem->name,"codec") != 0)
			continue;

		const char* pName = NULL;
		const char* pRates = NULL; 
		const apr_xml_attr* pAttr;
		for(pAttr = pChildElem->attr; pAttr; pAttr = pAttr->next) 
		{
			if(strcasecmp(pAttr->name,"name") == 0)
			{
				pName = pAttr->value;
			}
			else if(strcasecmp(pAttr->name,"rates") == 0)
			{
				pRates = pAttr->value;
			}
		}

		if(pName)
		{
			int rates = ParseRates(pRates,pool);
			mpf_codec_capabilities_add(m_pCapabilities,rates,pName);
		}
	}
	return true;
}

int UmcScenario::ParseRates(const char* pStr, apr_pool_t* pool)
{
	int rates = 0;
	if(pStr)
	{
		char* pRateStr;
		char* pState;
		char* pRateListStr = apr_pstrdup(pool,pStr);
		do 
		{
			pRateStr = apr_strtok(pRateListStr, " ", &pState);
			if(pRateStr) 
			{
				apr_uint16_t rate = (apr_uint16_t)atoi(pRateStr);
				rates |= mpf_sample_rate_mask_get(rate);
			}
			pRateListStr = NULL; /* make sure we pass NULL on subsequent calls of apr_strtok() */
		} 
		while(pRateStr);
	}
	return rates;
}

bool UmcScenario::LoadRtpTermination(const apr_xml_elem* pElem, apr_pool_t* pool)
{
	return true;
}

bool UmcScenario::InitCapabilities(mpf_stream_capabilities_t* pCapabilities) const
{
	if(m_pCapabilities)
	{
		int i;
		mpf_codec_attribs_t *pAttribs;
		for(i=0; i<m_pCapabilities->attrib_arr->nelts; i++)
		{
			pAttribs = &APR_ARRAY_IDX(m_pCapabilities->attrib_arr,i,mpf_codec_attribs_t);
			mpf_codec_capabilities_add(
					&pCapabilities->codecs,
					pAttribs->sample_rates,
					pAttribs->name.buf);
		}
	}
	else
	{
		/* add default codec capabilities (Linear PCM) */
		mpf_codec_capabilities_add(
				&pCapabilities->codecs,
				MPF_SAMPLE_RATE_8000 | MPF_SAMPLE_RATE_16000,
				"LPCM");
	}

	return true;
}

bool UmcScenario::IsElementEnabled(const apr_xml_elem* pElem)
{
	const apr_xml_attr* pAttr;
	for(pAttr = pElem->attr; pAttr; pAttr = pAttr->next) 
	{
		if(strcasecmp(pAttr->name,"enable") == 0)
		{
			return atoi(pAttr->value) > 0;
		}
	}
	return true;
}

const char* UmcScenario::LoadFileContent(const char* pFileName, apr_size_t& size, apr_pool_t* pool) const
{
	if(!m_pDirLayout || !pFileName)
		return NULL;

	char* pFilePath = apt_datadir_filepath_get(m_pDirLayout,pFileName,pool);
	if(!pFilePath)
		return NULL;

	apr_file_t *pFile;
	if(apr_file_open(&pFile,pFilePath,APR_FOPEN_READ|APR_FOPEN_BINARY,0,pool) != APR_SUCCESS) 
	{
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Open File %s",pFilePath);
		return NULL;
	}

	apr_finfo_t finfo;
	if(apr_file_info_get(&finfo,APR_FINFO_SIZE,pFile) != APR_SUCCESS) 
	{
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Get File Info %s",pFilePath);
		apr_file_close(pFile);
		return NULL;
	}

	size = (apr_size_t)finfo.size;
	char* pContent = (char*) apr_palloc(pool,size+1);
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Load File Content size [%" APR_SIZE_T_FMT " bytes] %s",size,pFilePath);
	if(apr_file_read(pFile,pContent,&size) != APR_SUCCESS) 
	{
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Read Content %s",pFilePath);
		apr_file_close(pFile);
		return NULL;
	}
	pContent[size] = '\0';
	apr_file_close(pFile);
	return pContent;
}
