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

#ifndef UMC_FRAMEWORK_H
#define UMC_FRAMEWORK_H

/**
 * @file umcframework.h
 * @brief UMC Application Framework
 */ 

#include <apr_xml.h>
#include <apr_hash.h>
#include "apt_consumer_task.h"
#include "umcsession.h"

class UmcScenario;

class UmcFramework : public UmcSessionMethodProvider
{
public:
/* ============================ CREATORS =================================== */
	UmcFramework();
	~UmcFramework();

/* ============================ MANIPULATORS =============================== */
	bool Create(apt_dir_layout_t* pDirLayout, apr_pool_t* pool);
	void Destroy();

	void RunSession(const char* pScenarioName, const char* pProfileName);
	void StopSession(const char* id);
	void KillSession(const char* id);

	void ShowScenarios();
	void ShowSessions();

protected:
	bool CreateMrcpClient();
	void DestroyMrcpClient();

	bool CreateTask();
	void DestroyTask();

	UmcScenario* CreateScenario(const char* pType);

	bool LoadScenarios();
	bool LoadScenario(const char* pFilePath);
	void DestroyScenarios();

	bool ProcessRunRequest(const char* pScenarioName, const char* pProfileName);
	void ProcessStopRequest(const char* id);
	void ProcessKillRequest(const char* id);
	void ProcessShowScenarios();
	void ProcessShowSessions();
	void ProcessSessionExit(UmcSession* pUmcSession);

	bool AddSession(UmcSession* pSession);
	bool RemoveSession(UmcSession* pSession);

	void ExitSession(UmcSession* pUmcSession);

/* ============================ HANDLERS =================================== */

	friend apt_bool_t UmcProcessMsg(apt_task_t* pTask, apt_task_msg_t* pMsg);
	friend void UmcOnStartComplete(apt_task_t* pTask);
	friend void UmcOnTerminateComplete(apt_task_t* pTask);
	friend apt_bool_t AppMessageHandler(const mrcp_app_message_t* pAppMessage);

private:
/* ============================ DATA ======================================= */
	apr_pool_t*          m_pPool;
	apt_dir_layout_t*    m_pDirLayout;
	apt_consumer_task_t* m_pTask;

	mrcp_client_t*       m_pMrcpClient;
	mrcp_application_t*  m_pMrcpApplication;

	apr_hash_t*          m_pScenarioTable;
	apr_hash_t*          m_pSessionTable;
};

#endif /* UMC_FRAMEWORK_H */
