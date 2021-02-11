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

#ifndef VERIFIER_SCENARIO_H
#define VERIFIER_SCENARIO_H

/**
 * @file verifierscenario.h
 * @brief Verifier Scenario
 */ 

#include "umcscenario.h"

class VerifierScenario : public UmcScenario
{
public:
/* ============================ CREATORS =================================== */
	VerifierScenario();
	virtual ~VerifierScenario();

/* ============================ MANIPULATORS =============================== */
	virtual void Destroy();

	virtual UmcSession* CreateSession();

/* ============================ ACCESSORS ================================== */
	const char* GetRepositoryURI() const;
	const char* GetVerificationMode() const;
	const char* GetVoiceprintIdentifier() const;

/* ============================ INQUIRIES ================================== */
protected:
/* ============================ MANIPULATORS =============================== */
	virtual bool LoadElement(const apr_xml_elem* pElem, apr_pool_t* pool);

	bool LoadVerify(const apr_xml_elem* pElem, apr_pool_t* pool);

/* ============================ DATA ======================================= */
	const char* m_RepositoryURI;
	const char* m_VerificationMode;
	const char* m_VoiceprintIdentifier;
};

/* ============================ INLINE METHODS ============================= */
inline const char* VerifierScenario::GetRepositoryURI() const
{
	return m_RepositoryURI;
}

inline const char* VerifierScenario::GetVerificationMode() const
{
	return m_VerificationMode;
}

inline const char* VerifierScenario::GetVoiceprintIdentifier() const
{
	return m_VoiceprintIdentifier;
}

#endif /* VERIFIER_SCENARIO_H */
