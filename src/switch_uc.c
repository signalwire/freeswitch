/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Anthony Minessale II <anthm@freeswitch.org>
 *
 *
 * switch_config.c -- Configuration File Parser
 *
 */

#include <switch.h>
#include <switch_uc.h>
#include "private/switch_core_pvt.h"
#ifdef ENABLE_SSOFT
#include "switch_usbkey.h"
#else
#include <d3des.h>
typedef struct {
	unsigned int  serial;
	unsigned int  id;
	unsigned int  codetype;
	unsigned char  passward[8];
	unsigned char keyval[16];
}at88_data_t;


#define	AT88_IOC_MAGIC	 	'W'

#define IOC_VERIFICATE                 _IOR(AT88_IOC_MAGIC,0,at88_data_t *)
#define IOC_INIT_AT88                  _IOW(AT88_IOC_MAGIC,1,at88_data_t *)
#define IOC_REWRITE					   _IOW(AT88_IOC_MAGIC,2,at88_data_t *)

#define AT88_DEV		"/dev/at88_drv"


unsigned char KeyVal[16]={'L','i','p','p','m','a','n','n','A','M','T','u','r','i','n','g'};       
unsigned char password[8] = {3,7,5,6,3,7,5,6};


#define SYNPBX_UC200_DEVICEID_NAME    "DeviceID=6001"
#define SYNPBX_UC500_DEVICEID_NAME    "DeviceID=6002"
#define SYNPBX_UC500H_DEVICEID_NAME   "DeviceID=6003"
#define SYNPBX_UC500_2G_DEVICEID_NAME "DeviceID=6004"
#define SYNPBX_UC200_8S_DEVICEID_NAME "DeviceID=6006"
#define SYNPBX_UC500HB_DEVICEID_NAME  "DeviceID=6011"


void val_init(at88_data_t *pdata)
{
	memcpy(pdata->keyval,KeyVal,16);
	memcpy(pdata->passward,password,8);
}

SWITCH_DECLARE(switch_status_t) switch_core_at88_test(int seq,int id,int code_type,int rewrite)
{
	unsigned char readval[16];
	unsigned char readval1[16];
	//unsigned char input;
	int at88_fd;
	
	//fprintf(stderr, "codetype = 0x%x.\n",code_type);
#if 0
	at88_data_t buf = {
		buf.codetype = code_type,
		buf.id 		 = id,
		buf.serial	 = seq
	};
#else
	at88_data_t buf;
	val_init(&buf);
	buf.serial = seq;
	buf.codetype = code_type;
	buf.id	= id;
#endif
	at88_fd = open(AT88_DEV,O_RDWR);
	if( at88_fd < 0 ){
		fprintf(stderr, "Open %s error.\n",AT88_DEV);
		return SWITCH_STATUS_FALSE;
	}

	//fprintf(stderr, "\n\n\nPlease cheack the information.\n\n");
	fprintf(stderr, "The   SEQ    is %.8d\n",buf.serial);
	fprintf(stderr, "The DeviceID is 0x%x\n",buf.id);
	fprintf(stderr, "The CodeType is 0x%x\n",buf.codetype);

	/*
	fprintf(stderr, "Write the passward?[Y/N] ");
	fprintf(stderr, "buf addr = %lx\n",&buf);
	
	input=getchar();
	getchar();	//�Իس�
	if(input=='Y' || input=='y')
	{
		if( rewrite )
			ioctl(at88_fd,IOC_REWRITE,&buf);
		else
			ioctl(at88_fd,IOC_INIT_AT88,&buf);
	}*/
	//printf("%s , %d\n",__func__,__LINE__);
	read(at88_fd,readval,16);
	close(at88_fd);
	//printf("%s , %d\n",__func__,__LINE__);
	des2key(KeyVal,DE1);
	//printf("%s , %d\n",__func__,__LINE__);
	D2des(readval,readval1) ;
	//printf("%s , %d\n",__func__,__LINE__);
	fprintf(stderr, "serial = %02x %02x %02x %02x\n",readval1[1],readval1[3],readval1[5],readval1[7]);
	fprintf(stderr, "id = %02x %02x %02x %02x\n",readval1[9],readval1[11],readval1[13],readval1[15]);
	fprintf(stderr, "codetype = %02x %02x %02x %02x\n",readval1[0],readval1[2],readval1[4],readval1[6]);
	if(buf.serial != ((readval1[1]<<24)|(readval1[3]<<16)|(readval1[5]<<8)|(readval1[7])))
	{
		fprintf(stderr, "the SEQ read write error,please make sure is ok\n");	
		return SWITCH_STATUS_FALSE;
	}
	if(buf.id != ((readval1[9]<<24)|(readval1[11]<<16)|(readval1[13]<<8)|(readval1[15])))
	{
		fprintf(stderr, "the DeviceID read write error,please make sure is ok\n");	
		return SWITCH_STATUS_FALSE;
	}
	if(buf.codetype != ((readval1[0]<<24)|(readval1[2]<<16)|(readval1[4]<<8)|(readval1[6])))
	{
		fprintf(stderr, "the CodeType read write error,please make sure is ok\n");
		return SWITCH_STATUS_FALSE;
	}
	printf("SEQ=%.8d \nDeviceID=0x%x\nCodeType=0x%x\n",buf.serial,buf.id,buf.codetype);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_encrypt(switch_core_flag_t flags, switch_bool_t console, const char **err)
{

	char cmd_buf[1024],pcb_ver[9];
	char szSerial[128],szMaxSessions[128],szModule[128];
	char szMaxSessions2[128],szModule2[128]; //added by dsq for OS-14754
	char szMaxExtensions[128],szDisModule[128];

	char *pSeq,*pId,*pCodeType,*pcbver,*pubootver,*end;
	int device_id;
	int serial;
	int code_type;

	FILE *cmdline;
	FILE *softver;
	FILE *kernelver;
	
	// memset(&runtime, 0, sizeof(runtime)); //modified by dsq for DS-89926 2020-12-02

	cmdline = fopen("/proc/cmdline","r");
	if(NULL==cmdline)
	{
		*err = "FATAL ERROR! Could not open cmdline!\n";
		return SWITCH_STATUS_FALSE;
	}
	fseek(cmdline,0,SEEK_SET);
	fread(cmd_buf,1023,sizeof(unsigned char),cmdline);
	fclose(cmdline);

	pId = strstr(cmd_buf,"DeviceID=");
	pSeq = strstr(cmd_buf,"SEQ="); 
	pCodeType = strstr(cmd_buf,"CodeType="); 
	pcbver=strstr(cmd_buf,"PCBVer=");
	pubootver = strstr(cmd_buf,"uboot_version=");

	if( pcbver ) {	
		strncpy(pcb_ver,pcbver+strlen("PCBVer="),8);
		fprintf(stderr, "PcbVer [%s]\n", pcb_ver);
	}

	if( pubootver ) {	
		strncpy(runtime.uboot_ver,pubootver+strlen("uboot_version="),12);
	}

	if( NULL != pSeq )
	{
		fprintf(stderr, "The   SEQ	 is %s\n",pSeq);
		while(*pSeq>'9' || *pSeq<'0') pSeq++;
		serial = strtol(pSeq,NULL,10);
		strcpy(runtime.device_sn,pSeq);
		end = strchr(runtime.device_sn,'\n');
		if(end)
			*end = '\0';
		fprintf(stderr, "The   SEQ	 is %.8d\n",serial);
	}
	else
	{
		*err = "FATAL ERROR! SEQ is null\n";
		return SWITCH_STATUS_FALSE;
	}

	if( NULL != pCodeType )
	{
		pCodeType=pCodeType+9;
		code_type = strtol(pCodeType,NULL,16);
		fprintf(stderr, "the code type is 0x%x\n",code_type);
	}
	else
	{
		*err = "FATAL ERROR! CodeType is null\n";
		return SWITCH_STATUS_FALSE;
	}

	if(NULL == pId)
		pId=strstr(cmd_buf,"DeviceId=");

	if( NULL != pId ) {
		strcpy(runtime.device_id,pId);

		fprintf(stderr, "runtime.device_id is %s\n",runtime.device_id);

		pId=pId + strlen("DeviceID=");
		
		device_id = strtol(pId,NULL,16);

		fprintf(stderr, "the DeviceID is %x\n",device_id);

	}else{
		*err = "FATAL ERROR! DeviceID is null\n";
		return SWITCH_STATUS_FALSE;
	}

	if (switch_core_at88_test(serial,device_id,code_type,0) != SWITCH_STATUS_SUCCESS) {
		*err = "FATAL ERROR! Could not initialize at88\n";
		return SWITCH_STATUS_FALSE;
	}

	softver = fopen("/shdisk/synpbx/version.web","r");

	if(NULL==softver)
	{
		*err = "FATAL ERROR! Could not open version.web!\n";
		return SWITCH_STATUS_FALSE;
	}
	fseek(softver,0,SEEK_SET);
	fread(runtime.soft_ver,255,sizeof(unsigned char),softver);
	fclose(softver);


	system("uname -a > /tmp/kernel.ver");
	
	kernelver = fopen("/tmp/kernel.ver","r");

	if(NULL==kernelver)
	{
		*err = "FATAL ERROR! Could not open kernel.ver!\n";
		return SWITCH_STATUS_FALSE;
	}
	fseek(kernelver,0,SEEK_SET);
	fread(runtime.kernel_ver,255,sizeof(unsigned char),kernelver);
	fclose(kernelver);
	

	if(strstr(runtime.device_id,SYNPBX_UC200_DEVICEID_NAME))
	{
		strcpy(runtime.device_type,"UC200");
	}
	else if(strstr(runtime.device_id,SYNPBX_UC500_DEVICEID_NAME))
	{
		strcpy(runtime.device_type,"UC500");
	}
	else if(strstr(runtime.device_id,SYNPBX_UC500H_DEVICEID_NAME))
	{
		strcpy(runtime.device_type,"UC500H");
	}
	else if(strstr(runtime.device_id,SYNPBX_UC500HB_DEVICEID_NAME))
	{
		strcpy(runtime.device_type,"UC500HB");
	}
	else if(strstr(runtime.device_id,SYNPBX_UC500_2G_DEVICEID_NAME))
	{
		strcpy(runtime.device_type,"UC500");
	}
	else if(strstr(runtime.device_id,SYNPBX_UC200_8S_DEVICEID_NAME))
	{
		strcpy(runtime.device_type,"UC200_8S");
	}
	else
	{  
		strcpy(runtime.device_type,"Unknown");
	}

	runtime.max_concurrency = 15;
	runtime.max_extension = 60;
	runtime.device_features_auth_sn = 0;
	runtime.device_modules_auth_sn = 0;

	//++begin++ added by fky for IPPBX-43,2018-8-8
	if(access("/shapp/ippbx",F_OK) == 0)
	{
		//try to remove ippbx.ini before decrypting
		system("rm -f /shapp/ippbx.ini");
		system("openssl enc -aes-128-cbc -d -k 13173137 -in /shapp/ippbx -out /shapp/ippbx.ini");
		if(access("/shapp/ippbx.ini",F_OK) == 0)
		{
			switch_core_get_ini_key_string("ippbx","Serial","null",szSerial,"/shapp/ippbx.ini");
			if(!strcmp(szSerial,runtime.device_sn))
			{
				switch_core_get_ini_key_string("ippbx","MaxSessions","0",szMaxSessions,"/shapp/ippbx.ini");
				switch_core_get_ini_key_string("ippbx","MaxSessions2","0",szMaxSessions2,"/shapp/ippbx.ini");
				runtime.max_concurrency =atoi(szMaxSessions2)!=0?atoi(szMaxSessions2):atoi(szMaxSessions);

				switch_core_get_ini_key_string("ippbx","MaxExtensions","0",szMaxExtensions,"/shapp/ippbx.ini");
				runtime.max_extension = atoi(szMaxExtensions);

				switch_core_get_ini_key_string("ippbx","Module","0",szModule,"/shapp/ippbx.ini");
				switch_core_get_ini_key_string("ippbx","Module2","0",szModule2,"/shapp/ippbx.ini"); 
				runtime.device_features_auth_sn = atoi(szModule2)!=0?atoi(szModule2):atoi(szModule);

				switch_core_get_ini_key_string("ippbx","Disable_Module","0",szDisModule,"/shapp/ippbx.ini");
				runtime.device_modules_auth_sn = atoi(szDisModule);
			}
			
		}
	}
	else
	{
		//added by yy for IPPBX-43,2018.11.29
		if(strstr(runtime.device_id,SYNPBX_UC200_DEVICEID_NAME))
		{
			fprintf(stderr,"uc200\n");
		}
		else if(strstr(runtime.device_id,SYNPBX_UC500_DEVICEID_NAME))
		{
			fprintf(stderr,"uc500\n");
			runtime.max_concurrency = 30;
			runtime.max_extension = 150;
		}
		else if(strstr(runtime.device_id,SYNPBX_UC500H_DEVICEID_NAME))
		{
			fprintf(stderr,"uc500h\n");
			runtime.max_concurrency = 30;
			runtime.max_extension = 150;
		}
		else if(strstr(runtime.device_id,SYNPBX_UC500HB_DEVICEID_NAME))
		{
			fprintf(stderr,"uc500hb\n");
			runtime.max_concurrency = 30;
			runtime.max_extension = 150;
		}
		else if(strstr(runtime.device_id,SYNPBX_UC500_2G_DEVICEID_NAME))
		{
			fprintf(stderr,"uc500 2g\n");
			runtime.max_concurrency = 30;
			runtime.max_extension = 150;
		}
		else if(strstr(runtime.device_id,SYNPBX_UC200_8S_DEVICEID_NAME))
		{
			fprintf(stderr,"uc200 8s\n");
		}
	}

	switch_core_session_limit(runtime.max_concurrency*2);

	return SWITCH_STATUS_SUCCESS;
}

//++begin++ added by fky for IPPBX-43,2018-8-8
SWITCH_DECLARE(int)  switch_core_get_ini_key_string(char *title,char *key,char *defaultstr,char *returnstr,char *filename)
{
	FILE *fp;
	char szLine[1024];
	char tmpstr[1024];
	int rtnval;
	int i = 0;
	int flag = 0;
	char *tmp;
 
	if((fp = fopen(filename, "r")) == NULL)
	{
		printf("there is no such file %s\n",filename);
		strcpy(returnstr,defaultstr);
		return -1;
	}
	while(!feof(fp))
	{
		rtnval = fgetc(fp);
		//the data and EOF may be at the same line
		if(rtnval == EOF && strlen(szLine) == 0)
		{
			break;
		}
		else
		{
			szLine[i++] = rtnval;
		}
		if(rtnval == '\n' || rtnval == EOF)
		{
			if(szLine[i-2] == '\r')
				i--;
			szLine[--i] = '\0';
			i = 0;
			tmp = strchr(szLine, '=');
 
			if(tmp != NULL && flag == 1)
			{
				if(strstr(szLine,key)!=NULL)
				{
					//ignore comment
					if ('#' == szLine[0])
					{
					}
					else if ('/' == szLine[0] && '/' == szLine[1])
					{
						
					}
					else
					{
						//find key
						strcpy(returnstr,tmp+1);
						fclose(fp);
						return 0;
					}
				}
				else
				{
					memset(szLine,0,1024);
				}
			}
			else
			{
				strcpy(tmpstr,"[");
				strcat(tmpstr,title);
				strcat(tmpstr,"]");
				if(strncmp(tmpstr,szLine,strlen(tmpstr)) == 0)
				{
					//find title
					flag = 1;
				}
			}
		}
	}
	fclose(fp);
	strcpy(returnstr,defaultstr);
	return -1;
}
#endif

SWITCH_DECLARE(switch_bool_t) switch_get_soft(void)
{
	return runtime.switch_soft;
}

SWITCH_DECLARE(switch_bool_t) switch_get_work(void)
{
	return runtime.can_rcs_work && (!runtime.auth_expired);
}


SWITCH_DECLARE(unsigned int) switch_get_maxch_num(void)
{
	return runtime.max_concurrency;

}
SWITCH_DECLARE(unsigned int) switch_get_maxext_num(void)
{
	return runtime.max_extension;

}
SWITCH_DECLARE(unsigned int) switch_get_maxapi_num(void)
{
	return runtime.max_api_num;

}

SWITCH_DECLARE(unsigned int) switch_get_valid_period(void)
{
	return runtime.valid_period;

}

SWITCH_DECLARE(unsigned int) switch_get_remaining_valid_period(void)
{
	if(runtime.used_period < runtime.valid_period) {
		return runtime.valid_period - runtime.used_period;
	}else {
		return 0;
	}
}

SWITCH_DECLARE(unsigned long long) switch_core_get_device_modules_auth_sn()
{
	return runtime.device_modules_auth_sn;
}

SWITCH_DECLARE(unsigned long long) switch_core_get_device_features_auth_sn()
{
	return runtime.device_features_auth_sn;
}

SWITCH_DECLARE(unsigned long long) switch_core_mem_free(void)
{
	return runtime.mem_free;
}

SWITCH_DECLARE(unsigned long long) switch_core_mem_total(void)
{
	return runtime.mem_total;
}

SWITCH_DECLARE(unsigned long long) switch_core_flash_size(void)
{
	return runtime.flashsize;
}

SWITCH_DECLARE(unsigned long long) switch_core_flash_use(void)
{
	return runtime.flashuse;
}

SWITCH_DECLARE(char *) switch_core_device_type(void)
{
	return runtime.device_type;
}

SWITCH_DECLARE(char *) switch_core_device_sn(void)
{
	return runtime.device_sn;
}

SWITCH_DECLARE(char *) switch_core_soft_ver(void)
{
	return runtime.soft_ver;
}

SWITCH_DECLARE(char *) switch_core_uboot_ver(void)
{
	return runtime.uboot_ver;
}

SWITCH_DECLARE(char *) switch_core_kernel_ver(void)
{
	return runtime.kernel_ver;
}

#ifdef ENABLE_SSOFT

//this function is execute once 1 second
static void check_key_verify(void)
{
	static unsigned long key_lost_cnt = 0;
	static unsigned long check_key_count = 0;
	static unsigned long key_recoverd_cnt = 0;
	static unsigned long int_count = 0;
	static switch_bool_t bFlag = SWITCH_TRUE;
	static switch_bool_t key_removed = SWITCH_FALSE;

	int_count++;

	if (runtime.has_rcs_key)//如果key 存在 则检测key 不存在的情况
	{
		if (switch_GetCKMLicAuth())
		{
			if (int_count % 1200 == 0)// lic 20 分钟 检测一次
			{
				runtime.check_rcs_key = SWITCH_TRUE;
			}
		}
		else if (int_count % 60 == 0)// when usbkey exists ,1 min check once
		{
			runtime.check_rcs_key = SWITCH_TRUE;
		}

		check_key_count ++;

		if(int_count > 18000) { //5 h to SWITCH_TRUE
			//event fire Test Time out
			switch_RCSRunToCrash();
		}


		if (key_removed)//从没有到有的情况，初始化
		{
			if(key_recoverd_cnt == 5)//5s 之后开始工作
			{
				//event fire SYS_USBKEY_DETECTED
				key_recoverd_cnt++;
				runtime.can_rcs_work = SWITCH_TRUE;
			}
			else
			{	
				key_recoverd_cnt++;
				if(key_recoverd_cnt == 300)//重新检测到 并持续5分钟后才清除失联标志
				{
					key_removed = SWITCH_FALSE;
					key_recoverd_cnt = 0;
					key_lost_cnt = 0;
				}
			}
			
		}
		else
		{
			if(check_key_count > 300) //5 分钟
			{
				runtime.used_period += check_key_count/(60);	//1m
				if(runtime.used_period < runtime.valid_period || runtime.valid_period == 0)
				{
					runtime.auth_expired = SWITCH_FALSE;
					runtime.write_used_period = SWITCH_TRUE;
				}
				else if(runtime.used_period == runtime.valid_period && bFlag && runtime.valid_period)
				{
					runtime.auth_expired = SWITCH_TRUE;
					bFlag = SWITCH_FALSE;
					runtime.write_used_period = SWITCH_TRUE;
				}
				else
				{
					runtime.auth_expired = SWITCH_TRUE;
				}
				check_key_count = 0;
			}

			if(key_lost_cnt > 0 && (check_key_count % 5) == 0)//5s
			{
				//event fire SYS_USBKEY_DETECTED
				key_lost_cnt = 0;
			}
		}
	}
	else//如果key 不存在，则继续检测key是否存在，如果5分钟内，一直不再则不再工作
	{
		check_key_count = 0;
		key_lost_cnt++;
		

		if (switch_GetCKMLicAuth())
		{
			if((key_lost_cnt % 60) == 0)//1分钟
			{
				runtime.check_rcs_key = SWITCH_TRUE;//没有检测到，继续检测
			}
		}
		else
		{
			if((key_lost_cnt % 5) == 0)//5秒钟
			{
				runtime.check_rcs_key = SWITCH_TRUE;//没有检测到，继续检测
			}
		}

		
		if (key_lost_cnt % 300 == 0)// when usbkey not exists after 5 min ,usb key removed
		{
			//event fire SYS_USBKEY_DETECTED
			key_removed = SWITCH_TRUE;
			key_recoverd_cnt = 0;
			runtime.can_rcs_work = SWITCH_FALSE;
		}

		if (key_lost_cnt > runtime.exp_alive_time)// when usbkey not exists after 5 min ,usb key removed
		{
			//event fire SYS_USBKEY_DETECTED
			runtime.can_rcs_work = SWITCH_FALSE;
		}
	}
}

SWITCH_STANDARD_SCHED_FUNC(check_key_callback)
{
	check_key_verify();

	/* reschedule this task */
	task->runtime = switch_epoch_time_now(NULL) + 1;//1 s
}


static void *SWITCH_THREAD_FUNC switch_check_usbkey_thread(switch_thread_t *thread, void *obj)
{
	switch_memory_pool_t *pool = (switch_memory_pool_t *) obj;
	int g_nNormalChNum, g_nExChNum;
	int  nErrCnt=0;
	static unsigned long dwCnt = 0;
	char szIPRErr[300]={0};


	while (runtime.running) {
		
		if (runtime.check_rcs_key == SWITCH_TRUE)
		{
			if (switch_GetCKMLicAuth())
			{
				//added by yy for SYNHMP-12.
				//mast check Password and ch num
				if(switch_CheckPassword(runtime.lic_pw) && runtime.max_concurrency <= switch_GetAuthChNum(RCS_KEY_ID, &g_nNormalChNum, &g_nExChNum, FALSE))
				{
					runtime.has_rcs_key = SWITCH_TRUE;
					nErrCnt=0;
				}
				else
				{
					nErrCnt=0;
					runtime.has_rcs_key = SWITCH_FALSE;
					//CloseAuthManager(); //不要反复close，容易成为授权漏洞
				}
				
				runtime.check_rcs_key = SWITCH_FALSE;
			}
			else
			{
				if (runtime.has_rcs_key == FALSE)
				{
					//不要反复close，容易成为授权漏洞
					//CloseAuthManager();
					if (switch_GetCKMLicAuth())
					{
						;
					}
					else
					{
						switch_CloseAuthManager();
					}
					
					switch_StartAuthManagerEx(RCS_KEY_ID,NULL,runtime.lic_sn,runtime.lic_pw);
				}

				if(switch_CheckRCSKEY(RCS_KEY_ID) && (runtime.serial_num == switch_GetUSBKeySerial(RCS_KEY_ID)) && runtime.max_concurrency <= switch_GetAuthChNum(RCS_KEY_ID, &g_nNormalChNum, &g_nExChNum, FALSE))
					runtime.has_rcs_key = SWITCH_TRUE;
				else
					runtime.has_rcs_key = SWITCH_FALSE;
				
				runtime.check_rcs_key = SWITCH_FALSE;
			}
		}

		if (runtime.write_used_period && runtime.has_rcs_key)
		{
			if(switch_GetCKMLicAuth())
			{
				if((dwCnt % 12) == 0)//云授权数据操作不能太频繁，由五分钟一次改为一小时一次
					switch_WriteUsedPeriod(RCS_KEY_ID,runtime.used_period);
			}
			else {
				switch_WriteUsedPeriod(RCS_KEY_ID,runtime.used_period);
			}

			runtime.write_used_period = FALSE;
			dwCnt++;
		}
		
		switch_cond_next();
	}

	switch_core_destroy_memory_pool(&pool);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "usbkey thread exiting\n");
	return NULL;
}

SWITCH_DECLARE(switch_status_t) switch_core_check_usb_key(const char **err)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	switch_memory_pool_t *pool;
	BOOL start = FALSE;
	char szIPRErr[300]={0};
	int g_nNormalChNum, g_nExChNum;
	int myerrno = 0;
	char szAuthconf[30];
	FILE *softver;
	FILE *kernelver;
	
	runtime.serial_num = -1;
	runtime.has_rcs_key = SWITCH_FALSE;
	runtime.max_concurrency = 1;
	runtime.max_extension = 2;
	runtime.max_api_num = 2;
	runtime.can_rcs_work = SWITCH_FALSE;
	runtime.device_features_auth_sn = 0;
	runtime.device_modules_auth_sn = 0;
	runtime.exp_alive_time = 1800;//30 m
	runtime.can_rcs_work = SWITCH_FALSE;
	strcpy(runtime.device_type,"SoftUC");

	start = switch_StartAuthManagerEx(RCS_KEY_ID,szIPRErr,runtime.lic_sn,runtime.lic_pw);

	if (start)
	{
		runtime.serial_num = switch_GetUSBKeySerial(RCS_KEY_ID);
		runtime.has_rcs_key = (switch_bool_t)switch_CheckRCSKEY(RCS_KEY_ID);
		runtime.max_concurrency = switch_GetAuthChNum(RCS_KEY_ID, &g_nNormalChNum, &g_nExChNum, TRUE);
		runtime.max_extension = switch_GetAuthExtNum(RCS_KEY_ID);
		runtime.max_api_num = switch_GetAuthApiNum(RCS_KEY_ID);

		runtime.valid_period = switch_GetValidPeriod(RCS_KEY_ID);
		runtime.used_period = switch_GetUsedPeriod(RCS_KEY_ID);

		if((runtime.used_period < runtime.valid_period) || (runtime.valid_period == 0))
		{
			runtime.auth_expired = SWITCH_FALSE;
			runtime.can_rcs_work = SWITCH_TRUE;
		}
		else
		{
			runtime.auth_expired = SWITCH_TRUE;
		}

		runtime.exp_alive_time = switch_GetExpAliveTime(RCS_KEY_ID) * 125;

		runtime.device_features_auth_sn = switch_GetAuthFeatures(RCS_KEY_ID);

		runtime.device_modules_auth_sn = switch_GetAuthModules(RCS_KEY_ID);
			
	}
	else
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Start Auth Failed %s\n", szIPRErr);
	}
	
	snprintf(runtime.device_sn, sizeof(runtime.device_sn), "%d", runtime.serial_num);

	switch_core_session_limit(runtime.max_concurrency*2);

	softver = fopen("/var/www/synpbx/version.web","r");

	if(NULL==softver)
	{
		*err = "FATAL ERROR! Could not open version.web!\n";
		return SWITCH_STATUS_FALSE;
	}
	fseek(softver,0,SEEK_SET);
	fread(runtime.soft_ver,255,sizeof(unsigned char),softver);
	fclose(softver);


	system("uname -a > /tmp/kernel.ver");
	
	kernelver = fopen("/tmp/kernel.ver","r");

	if(NULL==kernelver)
	{
		*err = "FATAL ERROR! Could not open kernel.ver!\n";
		return SWITCH_STATUS_FALSE;
	}
	fseek(kernelver,0,SEEK_SET);
	fread(runtime.kernel_ver,255,sizeof(unsigned char),kernelver);
	fclose(kernelver);

	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_CloseAuthManager();
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Pool Failure\n");
		return SWITCH_STATUS_GENERR;
	}

	switch_threadattr_create(&thd_attr, pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, switch_check_usbkey_thread, pool, pool);

	switch_scheduler_add_task(switch_epoch_time_now(NULL), check_key_callback, "check_key", "core", 0, NULL, SSHF_NONE | SSHF_NO_DEL);

	return SWITCH_STATUS_SUCCESS;

}

SWITCH_DECLARE(void) switch_close_auth()
{
	return switch_CloseAuthManager();//UC
}
#endif
//++end++ added by fky for IPPBX-43,2018-8-8

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
