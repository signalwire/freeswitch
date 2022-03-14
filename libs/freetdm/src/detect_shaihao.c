//#include "freetdm.h"
#include <stdlib.h>
#include <inttypes.h>
#include <memory.h>
#include <string.h>
#include <limits.h>
#include "detect_shaihao.h"

static int CutVoiceHeadForMem(char* szDest,char *szSoure)
{
	FILE * fSource = fopen(szDest,"r");//筛前原始文件
	char fDestFile[64] ={0};
	char szCmd[1024] ={0};
	
	if (fSource == NULL)
	{
		return CHECK_FALSE;
	}
	sprintf(fDestFile, "%s3",szDest);//筛后存储
	FILE * fDest = fopen(fDestFile, "wb");
	if (fDest == NULL)
	{
		fclose(fSource);
		return CHECK_FALSE;
	}
	// static int i = 0; //for debug 
	// char szFileDebug[128]={0};  //for debug 
	int bFoundEngry = CHECK_FALSE;
	// int bFoundEngryWhenCopy = 0;
	int nReadCount = 0;
	int16_t temp = 0;
	unsigned int engry = 0;

	int bStartCopy = CHECK_FALSE;
	fseek(fSource,0,SEEK_END); //get file length 
	int recordLen = 0;
	recordLen = ftell(fSource);
	if (recordLen < 16000) 
	{
		fclose(fDest);
		fclose(fSource);
		sprintf(szCmd,"rm -f %s",fDestFile);
		system(szCmd);
		return CHECK_FALSE;
	}
	fseek(fSource,0,SEEK_SET); 
	while(!feof(fSource))
	{
		
		fread(&temp,2,1,fSource);
		if(!bStartCopy){
			engry += abs(temp);
			if ((nReadCount % FRAME_SIZE) == FRAME_SIZE-1) {
				if (!bFoundEngry)
				{
					if (engry / FRAME_SIZE > 160) //find engry
					{
						bFoundEngry = CHECK_TRUE;
					}
				}
				else
				{
					if (engry / FRAME_SIZE < 160) //find silence
					{
						bStartCopy = CHECK_TRUE;
					}
				}
				engry = 0;
			}
			
		}else{
			fwrite(&temp, 2, 1, fDest);
		}
		nReadCount++;
	}

	fclose(fDest);
	if (bStartCopy)  
	{
		sprintf(szCmd,"mv %s %s",fDestFile,szDest); 
		system(szCmd);
		#if 0
		sprintf(szFileDebug,"/shdisk/synswitch/var/log/synswitch/debug_%s_%d",&szSoure[5],i++); 
    	sprintf(szCmd,"cp -rf %s %s",szDest,szFileDebug); 
    	system(szCmd);
		#endif
		
		return CHECK_TRUE;
	}
	else
	{
		sprintf(szCmd,"rm -f %s",fDestFile);
		system(szCmd);
		return CHECK_FALSE;
	}
}
static int CutVoiceHead(char* szFileName)
{
	FILE * fSource = fopen(szFileName,"r");
	if(fSource == NULL)
	{
		return CHECK_FALSE;
	} 
	// unsigned char ucBuffer[200*1024];
	char szDest[1024];
	char szCmd[1024];
	sprintf(szDest, "%s2",szFileName);
	FILE * fDest = fopen(szDest,"w");
	if(fDest == NULL)
	{
		fclose(fSource);
		return CHECK_FALSE;
	} 
	int bFoundEngry = CHECK_FALSE;
	int nReadCount=0;
	short temp=0;
	int engry=0;
	
	int bStartCopy = CHECK_FALSE ;
	// int reallen=0;
	while(!feof(fSource))
	{
			nReadCount++;
			fread(&temp,2,1,fSource); //read a short
			
			if(!bStartCopy)
			{
					engry+=abs(temp);
					if ((nReadCount % FRAME_SIZE) == 0) //cal engry every 256 point
					{
						if(!bFoundEngry)
						{
							if (engry / FRAME_SIZE > 160) //find engry
							{
								bFoundEngry=CHECK_TRUE;
							}
						}
						else
						{
							if (engry / FRAME_SIZE < 160) //find silence
								{
									bStartCopy=CHECK_TRUE;
								}
						}
						engry=0;
					}
			}
			else
			{
				fwrite(&temp,2,1,fDest);
			}
	}
	
	fclose(fSource);
	fclose(fDest);

	if(bStartCopy)
	{
		sprintf(szCmd,"mv %s2 %s",szFileName,szFileName);
		system(szCmd);
		return CHECK_TRUE;
	}
	else
	{
		sprintf(szCmd,"rm -f %s2",szFileName);
		system(szCmd);
		return CHECK_FALSE;
	}	
}

int ProcessShaihao(shaihao_state_s_t * pState)
{
    char file[128]={0};
	char szCmd[1024]={0};
	char testCmd[80]={0};
	char szTempBuf[128]={0};
	char szFileDebug[128]={0}; 
	// char szRecordname[64] = {0};
	int i,nResult = 0;
	 FILE *fd=NULL;
	//char szFileS[32]={0};
	// char szFileT[32]={0};
	char szFileW[128]={0};
	char stream[128]={0};
	char szOperator[3][10]={{0},{0},{0}};
	FILE *testfd = NULL;
	testfd = fopen("/shdisk/backup/test.txt","wb");
	strcpy(szOperator[0],"0-yd");
	strcpy(szOperator[1],"1-lt");
	strcpy(szOperator[2],"2-dx");
    sprintf(szFileW,"/tmp/tmp%s",&pState->recordname[5]); 
    sprintf(szCmd,"cp -rf %s %s",pState->recordname,szFileW);
    system(szCmd);
	sprintf(szFileDebug,"/shdisk/synswitch/var/log/synswitch/%s",&pState->recordname[5]); 
	sprintf(szCmd,"cp -rf %s %s",pState->recordname,szFileDebug); 
	system(szCmd);
    sprintf(file,"%s.result",pState->recordname);
    remove(file);
    memset(stream,0,sizeof(stream));
	
	strncpy(pState->debugrecordname, szFileDebug, sizeof(szFileDebug)); //record sr debug record name 
 	
    for(i=0;i<3;i++)
    {
		sprintf(szCmd,"rcg_linux sr2 %s/%s/ %s",pState->trainingPath,szOperator[i],pState->recordname);
        system(szCmd);
        fd = fopen(file, "r");
        if (fd == NULL){
			sprintf(testCmd,"first check get result failed %d \n",__LINE__);
            fwrite(testCmd, 1, sizeof(testCmd), testfd);
            nResult = 0;
        }
        else
        {
            fread(stream, 127, 1, fd);
          
            if(strcmp(stream,"notmatch"))
            {
                sscanf(stream,"%d-%s",&nResult,szTempBuf);
                fclose(fd);
                break;
            }
            else
            {
                nResult = 0;
            }
            fclose(fd);
            remove(file);
        }
    }

    if(nResult == 0)
    {
       
        int cutresult=CutVoiceHead(pState->recordname);
        if(cutresult)
        {
            sprintf(file,"%s.result",pState->recordname);
            remove(file);
            memset(stream,0,sizeof(stream));
            for(i = 0; i < 3;i++)
            {
				sprintf(szCmd,"rcg_linux sr2 %s/%s2/ %s",pState->trainingPath,szOperator[i],pState->recordname);               
                system(szCmd);
                fd = fopen(file, "r");
                if (fd == NULL)
                {
					sprintf(testCmd,"second check get result failed %d \n",__LINE__);
            		fwrite(testCmd, 1, sizeof(testCmd), testfd);
					nResult = 0;
                }
                else
                {
                    fread(stream, 127, 1, fd);												

                    if(strcmp(stream,"notmatch"))
                    {
                        sscanf(stream,"%d-%s",&nResult,szTempBuf);
                        
                        fclose(fd);
                        break;
                    }
                    else
                    {
                        nResult = 0;
                    }
                    fclose(fd);
                    remove(file);
                }
            }
            
        }
    }

	if(nResult == 0)
	{				
		do
		{
			sprintf(file,"%s.result",szFileW);
			remove(file);
			memset(stream,0,sizeof(stream));
			for(i = 0;i < 3;i++)
			{
				
				sprintf(szCmd,"rcg_linux sr2 %s/%s3/ %s",pState->trainingPath,szOperator[i],szFileW);
				system(szCmd);
				fd = fopen(file, "r");
				
				if (fd == NULL)
				{
					sprintf(testCmd,"third check get result failed %d \n",__LINE__);
					fwrite(testCmd, 1, sizeof(testCmd), testfd);
					nResult = 0;
				}
				else
				{
					fread(stream, 127, 1, fd);
					if (strcmp(stream, "notmatch"))
					{
						sscanf(stream, "%d-%s", &nResult, szTempBuf);
						fclose(fd);
						break;
					}
					else
					{
						nResult = 0;
					}
					fclose(fd);
				}
			}
			if(nResult == 0 && pState->nProcResult==2){
				for(i = 0;i <3;i++)
				{
					sprintf(szCmd,"rcg_linux sr2 %s/%s/ %s",pState->trainingPath,szOperator[i],szFileW);
					system(szCmd);
					fd = fopen(file, "r");
					
					if (fd == NULL)
					{
						sprintf(testCmd,"third check get result failed %d \n",__LINE__);
						fwrite(testCmd, 1, sizeof(testCmd), testfd);
						nResult = 0;
					}
					else
					{
						fread(stream, 127, 1, fd);
						if (strcmp(stream, "notmatch"))
						{
							sscanf(stream, "%d-%s", &nResult, szTempBuf);
							fclose(fd);
							break;
						}
						else
						{
							nResult = 0;
						}
						fclose(fd);
					}
				}
			}
			
		}while( nResult == 0  && CutVoiceHeadForMem(szFileW,pState->recordname));
	}

		if(nResult==0){
			// strncpy(pState->debugrecordname, szFileDebug, sizeof(szFileDebug));
			// if(pState->nProcResult==2){
			// 	remove(szFileDebug);
			// 	memset(pState->debugrecordname,0,sizeof(pState->debugrecordname));
			// }
		}else{
			remove(szFileDebug);
			memset(pState->debugrecordname,0,sizeof(pState->debugrecordname));
		}
		remove(pState->recordname);
     	remove(szFileW);	
        remove(file);
	
		pState->nShaihaoResult = nResult;
		fclose(testfd); // output err information
        return pState->nShaihaoResult;
        
    }

	int get_energy_state(shaihao_state_s_t * pState){

		FILE * fSource = fopen(pState->recordname,"r");
		if(fSource == NULL)
		{
			return -1;
		} 
		short temp=0;
		// int engry=0;
		// int position = 0;
		int16_t lastdata = 1;
		int SameNum = 1;
		int ContinueTimes = 0;
		// int i = 0;
		while(!feof(fSource)){
			fread(&temp,2,1,fSource);	
			long multvalue = lastdata*temp;
			if (multvalue>0) //count 
			{
				SameNum = SameNum + 1;
			}
			else if (multvalue <= 0) //cross zero point
			{
				if ((SameNum>7) && (SameNum<11)) //find 450hz cross zero point
				{
					ContinueTimes++;
				}
				else
				{
					ContinueTimes = 0;
				}
				SameNum = 1; 
				// if (ContinueTimes>60)
				if (ContinueTimes>16)			
				{
					pState->nProcResult = 2;
					return pState->nProcResult;
				}
			}
			lastdata = temp;
		}
	
	///////////////////////////////////////////////////////////////////

		//start colorring detect
		fseek(fSource,0,SEEK_SET); 		
		long Energy = 0;//clear energy
		int iReadCnt = 0;
		int HNum = 0;
		int LNum = 0;
		int EStatus = 0;
		// int16_t sdata = 0;
		while (!feof(fSource))
		{
			iReadCnt ++;
			fread(&temp,2,1,fSource);	
			// sdata = temp;
			Energy = Energy + abs(temp);//get energy (160points)
			if(iReadCnt==160){
				iReadCnt = 0;
				Energy = 0;
			}
			if (Energy>160 *300)
			{
				HNum++;
				if ((LNum > 2) && (EStatus == 1) && (HNum > 1))
				{
					EStatus = 0;
					break;
				}
				if ((HNum > 20) && (EStatus != 1)) //above 0.32s high energy
				{
					EStatus = 1;
				}

				if ((LNum != 0) && (HNum > 2))
				{
					LNum = 0;// continue High energy,clear Lnum
				}
			}
			else
			{
				LNum++;
				if ((EStatus == 1) && (LNum>4))
				{
					EStatus = 0;
					break;
				}
				if ((HNum != 0) && (LNum>2))
				{
					HNum = 0;// continue Ligh energy,clear Hnum
				}
			}
		}
	pState->nProcResult = EStatus;
	return pState->nProcResult;
}



    void Init_shaihaoSate(shaihao_state_s_t *pState)
    {
        memset(pState->recordname,0,sizeof(pState->recordname));
		memset(pState->debugrecordname,0,sizeof(pState->debugrecordname));
        memset(pState->trainingPath,0,sizeof(pState->trainingPath));
        pState->nShaihaoState = 0;
        pState->nShaihaoResult = 0;
		pState->pRecordfile = NULL;
    }



