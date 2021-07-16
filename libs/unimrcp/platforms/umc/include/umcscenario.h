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

#ifndef UMC_SCENARIO_H
#define UMC_SCENARIO_H

/**
 * @file umcscenario.h
 * @brief UMC Scenario
 */ 

#include <apr_xml.h>
#include "mrcp_application.h"

class UmcSession;

class UmcScenario
{
public:
/* ============================ CREATORS =================================== */
	UmcScenario();
	virtual ~UmcScenario();

/* ============================ MANIPULATORS =============================== */
	virtual bool Load(const apr_xml_elem* pElem, apr_pool_t* pool);
	virtual void Destroy();

	virtual UmcSession* CreateSession() = 0;

	void SetDirLayout(apt_dir_layout_t* pDirLayout);
	void SetName(const char* pName);
	void SetMrcpProfile(const char* pMrcpProfile);

	bool InitCapabilities(mpf_stream_capabilities_t* pCapabilities) const;

/* ============================ ACCESSORS ================================== */
	apt_dir_layout_t* GetDirLayout() const;
	const char* GetName() const;
	const char* GetMrcpProfile() const;

/* ============================ INQUIRIES ================================== */
	bool IsDiscoveryEnabled() const;

protected:
/* ============================ MANIPULATORS =============================== */
	virtual bool LoadElement(const apr_xml_elem* pElem, apr_pool_t* pool);
	
	bool LoadDiscovery(const apr_xml_elem* pElem, apr_pool_t* pool);
	bool LoadTermination(const apr_xml_elem* pElem, apr_pool_t* pool);
	bool LoadCapabilities(const apr_xml_elem* pElem, apr_pool_t* pool);
	bool LoadRtpTermination(const apr_xml_elem* pElem, apr_pool_t* pool);

	const char* LoadFileContent(const char* pFileName, apr_size_t& size, apr_pool_t* pool) const;
	static int ParseRates(const char* pStr, apr_pool_t* pool);

/* ============================ INQUIRIES ================================== */
	static bool IsElementEnabled(const apr_xml_elem* pElem);

/* ============================ DATA ======================================= */
	const char*                       m_pName;
	const char*                       m_pMrcpProfile;
	apt_dir_layout_t*                 m_pDirLayout;

	bool                              m_ResourceDiscovery;
	mpf_codec_capabilities_t*         m_pCapabilities;
	mpf_rtp_termination_descriptor_t* m_pRtpDescriptor;
};


/* ============================ INLINE METHODS ============================= */
inline void UmcScenario::SetDirLayout(apt_dir_layout_t* pDirLayout)
{
	m_pDirLayout = pDirLayout;
}

inline apt_dir_layout_t* UmcScenario::GetDirLayout() const
{
	return m_pDirLayout;
}

inline void UmcScenario::SetName(const char* pName)
{
	m_pName = pName;
}

inline const char* UmcScenario::GetName() const
{
	return m_pName;
}

inline void UmcScenario::SetMrcpProfile(const char* pMrcpProfile)
{
	m_pMrcpProfile = pMrcpProfile;
}

inline const char* UmcScenario::GetMrcpProfile() const
{
	return m_pMrcpProfile;
}

inline bool UmcScenario::IsDiscoveryEnabled() const
{
	return m_ResourceDiscovery;
}

#endif /* UMC_SCENARIO_H */
