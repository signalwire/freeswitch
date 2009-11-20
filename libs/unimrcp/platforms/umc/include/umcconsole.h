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

#ifndef __UMC_CONSOLE_H__
#define __UMC_CONSOLE_H__

/**
 * @file umcconsole.h
 * @brief UMC Application Console
 */ 

#include "apt_log.h"

class UmcFramework;

class UmcConsole
{
public:
/* ============================ CREATORS =================================== */
	UmcConsole();
	~UmcConsole();

/* ============================ MANIPULATORS =============================== */
	bool Run(int argc, const char * const *argv);

protected:
	bool LoadOptions(int argc, const char * const *argv, apr_pool_t *pool);
	bool RunCmdLine();
	bool ProcessCmdLine(char* pCmdLine);
	void Usage() const;

private:
/* ============================ DATA ======================================= */
	struct UmcOptions
	{
		const char*        m_RootDirPath;
		apt_log_priority_e m_LogPriority;
		apt_log_output_e   m_LogOutput;
	};

	UmcOptions    m_Options;
	UmcFramework* m_pFramework;
};

#endif /*__UMC_CONSOLE_H__*/
