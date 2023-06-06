#ifndef __AUTHMAG_API_H__
#define __AUTHMAG_API_H__

#include "common/platform/compat.h"

#ifdef __cplusplus
extern "C"
{
#endif

	//���ڹ���USB��
enum
{
	RECORDER_KEY = 0,
	ANALYZER_KEY = 1,
	HMP_KEY		 = 2,
	SBC_KEY		 = 3,
	RCS_KEY		 = 4,//added by yy,for RCS,2020.04.14
	MAX_USB_KEY
};

//added by wangfeng for Os-12566, 2018.8.16
//AM��־�ȼ�
enum
{
	AL_CLOSE = 0,
	AL_ERROR = 1,
	AL_WARNING = 2,
	AL_DEBUG = 3,
};
	
	BOOL WINAPI CheckPassword(char * szLicPw);
	DWORD WINAPI GetUSBKeySerial(int nKeyID);
	WORD  WINAPI GetAuthChNum(int nKeyID, int* pnNormalChNum, int* pnExChNum, BOOL bRetryIfFail);
	//BOOL WINAPI StartAuthManager(CHAR* szIPRErr,char * szLicSn, char * szLicPw);//masked by wangfeng for Os-12566, 2018.8.16
	BOOL WINAPI StartAuthManager(CHAR* szIPRErr,char * szLicSn, char * szLicPw, char * szLogDirectory, UCHAR ucLogLevel, UCHAR ucLogCreatePeriod);//added by wangfeng for Os-12566, 2018.8.16
	void WINAPI CloseAuthManager();
	int WINAPI GetUserAcc(int nKeyID);
	DWORD WINAPI GetValidPeriod(int nKeyID);
	DWORD WINAPI GetUsedPeriod(int nKeyID);
	BOOL WINAPI WriteUsedPeriod(int nKeyID, DWORD deUsedPeriod);
	void WINAPI FreeAuthDll();
	BOOL WINAPI GetCKMLicAuth();
	u64 WINAPI MyGetTickCount();
	BOOL WINAPI CheckValid(int nKeyID,DWORD dwAuthNO,char * szLicPw);
	//BOOL WINAPI StartAuthManagerEx(int nKeyID,CHAR* szIPRErr,char * szLicSn, char * szLicPw);//masked by wangfeng for Os-12566, 2018.8.16
	BOOL WINAPI StartAuthManagerEx(int nKeyID,CHAR* szIPRErr,char * szLicSn, char * szLicPw, char * szLogDirectory, UCHAR ucLogLevel, UCHAR ucLogCreatePeriod);//added by wangfeng for Os-12566, 2018.8.16
	BOOL WINAPI CheckCfgPassword(void * hBitHandle ,char * szLicPw);
	BOOL WINAPI TestSnPassword(char * szLicSn ,char * szLicPw);
	//+++start+++ added by netwolf 2018.07.10
	DWORD WINAPI GetExpAliveTime(int nKeyID);
	//+++end +++ added by netwolf 2018.07.10
	DWORD WINAPI GetAuthExtNum(int nKeyID);//added by yy,for RCS,2020.04.14
	u64 WINAPI GetAuthModules(int nKeyID);//added by yy,for RCS,2020.04.14
	u64 WINAPI GetAuthFeatures(int nKeyID);//added by yy,for RCS,2020.04.14
	BOOL WINAPI ChecKEY(int nKeyID);//added by yy,for RCS,2020.04.14
	WORD WINAPI GetAuthApiNum(int nKeyID);//added by yy,for RCS,2020.04.14
	BOOL WINAPI GetRCSTestAuth();
#ifdef __cplusplus
}
#endif

#endif
