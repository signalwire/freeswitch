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

#ifndef UMC_CONSOLE_H
#define UMC_CONSOLE_H

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
	static void Usage();

private:
/* ============================ DATA ======================================= */
	struct UmcOptions
	{
		const char*        m_RootDirPath;
		const char*        m_DirLayoutConf;
		const char*        m_LogPriority;
		const char*        m_LogOutput;

		UmcOptions() : 
			m_RootDirPath(NULL), m_DirLayoutConf(NULL), 
			m_LogPriority(NULL), m_LogOutput(NULL) {}
	};

	UmcOptions      m_Options;
	UmcFramework*   m_pFramework;
};

#endif /* UMC_CONSOLE_H */
