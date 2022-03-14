//#include "freetdm.h"
#include <stdlib.h>
#include <inttypes.h>
#include <memory.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include "detect_amd.h"

// AmdPara gAmdParam = {80/16, 400/16, 180/16, 1200/16, 1000/16, 10000, 70000, 6000};
 AmdPara gAmdParam = {80/20, 400/20,600/20,180/20, 1200/20, 1000/20,3,2000, 70000/20, 30000/20,15000/20};
static uint32_t _sqrt(uint32_t dwValue)
{
    uint32_t dwResult, dwMul, dwStep, dw;

    for (dw = 0; dw < 32; dw++)
        if ((dwValue << dw) & 0x80000000)
            break;

    if (dw >= 31) return 32 -dw;

    dwStep = (32 - dw) / 2;

    dwResult = 1 << dwStep;

    do
    {
        dwMul = dwResult * dwResult;

        if (dwMul == dwValue || dwStep == 0)
            break;

        dwStep--;

        if (dwMul > dwValue) dwResult -= 1 << dwStep;
        else                dwResult += 1 << dwStep;
    }
    while (dwStep);

    return dwResult;
}
//added by dsq for amd 2019-7-17
int32_t tbl_db[49] = /*-24db..+24db*/
{
	516, //-24db 0.0631 
	579, //-23db 0.0708 
	650, //-22db 0.0794 
	730, //-21db 0.0891 
	819, //-20db 0.1000 
	919, //-19db 0.1122 
	1031, //-18db 0.1259 
	1157, //-17db 0.1413 
	1298, //-16db 0.1585 
	1456, //-15db 0.1778 
	1634, //-14db 0.1995 
	1833, //-13db 0.2239 
	2057, //-12db 0.2512 
	2308, //-11db 0.2818 
	2590, //-10db 0.3162 
	2906, //-9db 0.3548 
	3261, //-8db 0.3981 
	3659, //-7db 0.4467 
	4105, //-6db 0.5012 
	4606, //-5db 0.5623 
	5168, //-4db 0.6310 
	5799, //-3db 0.7079 
	6507, //-2db 0.7943 
	7301, //-1db 0.8913 
	8192, //0db 1.0000 
	9191, //1db 1.1220 
	10313, //2db 1.2589 
	11571, //3db 1.4125 
	12983, //4db 1.5849 
	14567, //5db 1.7783 
	16345, //6db 1.9953 
	18339, //7db 2.2387 
	20577, //8db 2.5119 
	23088, //9db 2.8184 
	25905, //10db 3.1623 
	29066, //11db 3.5481 
	32612, //12db 3.9811 
	36592, //13db 4.4668 
	41057, //14db 5.0119 
	46067, //15db 5.6234 
	51688, //16db 6.3096 
	57994, //17db 7.0795 
	65071, //18db 7.9433 
	73011, //19db 8.9125 
	81920, //20db 10.0000 
	91915, //21db 11.2202 
	103131, //22db 12.5893 
	115715, //23db 14.1254 
	129834, //24db 15.8489 
};

//ended by dsq for ds-73667 amd 2019-07-17
static void AnalyzeActualPersonPickup(amd_acutalpickup_t *pt)
{
    switch (pt->nStatus)
    {
    	case T1_WaitOff:
		if (pt->nStatus != pt->nOldStatus){
			pt->nOldStatus = pt->nStatus;
		}
        if (pt->Stream2 == 0) //Add by pgy for SC-190 2008/05/28
        {
			++pt->dwS2OffCnt;
            
            // if (pt->dwS2OffCnt < S2_TOff)
            if (pt->dwS2OffCnt < gAmdParam.nS2TOff)
                break;
            else
            {
                pt->dwS2OnCnt = 0;
                pt->nStatus = T1_CountOff;
            }
        }
        else
        {
            pt->dwS2OffCnt = 0;
        }

        break;

    case T1_CountOff: //Add by pgy for SC-190 2008/05/29
		++pt->dwS2OffCnt; //Add by pgy for SC-190 2008/06/04
        if (pt->Stream2 == 1) //Add by pgy for SC-190 S3=1,2008/05/29
        {
			++pt->dwS2OnCnt;
            if (pt->dwS2OnCnt < gAmdParam.nS2TOn)
			// if (pt->dwS2OnCnt < 4)
                break;
            else
            {
                pt->dwS2OffCnt -= pt->dwS2OnCnt;
				if (pt->nStatus != pt->nOldStatus)
				{
					pt->nOldStatus = pt->nStatus;
				}
                if (pt->dwS2OffCnt > gAmdParam.nAMDTimeA) //Add by pgy for SC-190 2008/06/03
                {

                    pt->dwS2OffCnt = 0;
                    pt->nStatus = T2_CountOn;
                }else
                {
                    pt->dwS2OffCnt = 0;
                    pt->nStatus = T1_WaitOff;
                }
            }
        }
		//else if(amd->Stream2 == 2 && amd->dwEchoHighCount >=Cfg.AMDCfg.ToneTimeLimit)	
		else if(pt->Stream2 == 2 && pt->dwEchoHighCount >=gAmdParam.ToneTimeLimit)	
		{
			pt->dwS2OffCnt = 0;
			pt->dwS2OnCnt = 0;
			pt->nStatus = T1_WaitOff;
		}
        else
        {
            pt->dwS2OnCnt = 0;
        }

        break;

    case T2_CountOn: //Add by pgy for SC-190 ,2008/05/29

		++pt->dwS2OnCnt; //Add by pgy for SC-190,2008/06/04

        if (pt->Stream2 == 1)	//
        {
            pt->dwS2OffCnt = 0;
        }
        else if (pt->Stream2 == 0)	//
        {
			++pt->dwS2OffCnt;
        }
		//else if (amd->Stream2 == 2 && amd->dwEchoHighCount >= Cfg.AMDCfg.ToneTimeLimit)	
		else if (pt->Stream2 == 2 && pt->dwEchoHighCount >= gAmdParam.ToneTimeLimit)	
		{
			pt->dwS2OffCnt = 0;	
			pt->dwS2OnCnt = 0;
			pt->nStatus = T1_WaitOff;
			break;
		}

        if (pt->dwS2OffCnt == 0) 
        {
            //if (amd->dwS2OnCnt > Cfg.AMDCfg.S3_TimeC)
			if (pt->dwS2OnCnt > gAmdParam.nAMDTimeC)
            {
				if (pt->nStatus != pt->nOldStatus)
				{
					pt->nOldStatus = pt->nStatus;
				}
                pt->dwS2OnCnt = 0;
                pt->dwS2OffCnt = 0;
                pt->nStatus = T1_WaitOff;
                //pop hint or color ring event once

                 if (!pt->bDetectColorRing)	//SC-4078 
                 {
                    //ftdmchan->bDetectColorRing = "true";
					pt->bDetectColorRing = 1;
                    pt->dwNoSound = 0;
                    pt->nAMDResult = AMD_COLORRING;
                 }
            }
            else
                break;
        }
        else if (pt->dwS2OffCnt < gAmdParam.nS2TOff)
		{
			break;
        }
        else  //Add by pgy for SC-190,2008/06/03
        {
            pt->dwS2OnCnt -= pt->dwS2OffCnt;

			if (pt->nStatus != pt->nOldStatus)
			{
				pt->nOldStatus = pt->nStatus;
			}
			if (pt->dwS2OnCnt < gAmdParam.nAMDTimeB)
            //if (amd->dwS2OnCnt < Cfg.AMDCfg.S3_TimeB)
            {

                pt->dwS2OnCnt = 0;
                pt->nStatus = T1_CountOff;

            }
            else if (pt->dwS2OnCnt > gAmdParam.nAMDTimeC)
            {
                pt->dwS2OnCnt = 0;
                pt->nStatus = T1_CountOff;
                //pop hint or color ring event once

                if (!pt->bDetectColorRing)	//SC-4078 
                {
                    //ftdmchan->bDetectColorRing = "true";
					pt->bDetectColorRing = 1;
                    pt->dwNoSound = 0;
                    pt->nAMDResult = AMD_COLORRING;
                }
            }
            else
            {

                pt->dwS2OnCnt = 0;
                pt->nStatus = T3_CountOff;
            }
        }

        break;

    case T3_CountOff: //Add by pgy for SC-190 ,2008/05/29
		++pt->dwS2OffCnt;
        if (pt->Stream2 == 1)	
        {
			++pt->dwS2OnCnt;
        }
        else if (pt->Stream2 == 0)	
        {
            pt->dwS2OnCnt = 0;
        }
		else if (pt->Stream2 == 2)	
		{
			++pt->dwS2OnCnt;
		}


        if (pt->dwS2OnCnt == 0) 
        {
            //if (amd->dwS2OffCnt > Cfg.AMDCfg.S3_TimeD)
			if (pt->dwS2OffCnt > gAmdParam.nAMDTimeD)
            {
				if (pt->nStatus != pt->nOldStatus)
				{
					pt->nOldStatus = pt->nStatus;
				}
                pt->nAMDResult = AMD_ACTUAL_PICKUP;
				//AnalogChCallProc(ftdmchan, E_HANDLER_VOICE);
				pt->uEnableAmd = 0; //close amd 
                //CloseAMD(ftdmchan);
            }
            else
                break;
        }
		//else if (amd->dwS2OnCnt < Cfg.AMDCfg.S2_TOn)
		else if (pt->dwS2OnCnt < gAmdParam.nS2TOn)
        {
            break; 
        }
        else
        {
            pt->dwS2OffCnt -= pt->dwS2OnCnt;
			if (pt->nStatus != pt->nOldStatus)
			{
				pt->nOldStatus = pt->nStatus;
			}

            //if (amd->dwS2OffCnt > Cfg.AMDCfg.S3_TimeD)
			if (pt->dwS2OffCnt > gAmdParam.nAMDTimeD)
			
            {
                pt->nAMDResult = AMD_ACTUAL_PICKUP;
				pt->uEnableAmd = 0; //close amd 
            }
            else
            {
                pt->dwS2OffCnt = 0;
                pt->nStatus = T1_WaitOff;
				pt->bDetectColorRing = 1;
				pt->dwNoSound = 0;
				pt->nAMDResult = AMD_COLORRING;
            }
        }

        break;

    default:
        break;
    }
}


void ExecutAMD(amd_acutalpickup_t *pt){

	const char* bIsTone = "false";  
    //uint32_t dwMaxFreq = 0, dwMinFreq = 0;
    //uint32_t dwFreqErrPer = 8;

	++pt->dwTimeOut;
    //time
    if (pt->dwTimeOut > gAmdParam.nTimeout) //70s
    {
        pt->nAMDResult = AMD_TIMEOUT;
		pt->uEnableAmd = 0; //close amd 
        return;
    }
    if (pt->getOverallEnergy.dwOverallEnergy > gAmdParam.nSilentEnergy) 
    {
        //amd->Stream1 = "true";
		pt->Stream1 = 1;
        pt->dwNoSoundAfterDial = 0;
        if (pt->bDetectColorRing || pt->bDetectTone)
        {
            pt->dwNoSound = 0;
        }
    }
    else
    {
		//amd->Stream1 = "false";
        pt->Stream1 = 0;
        if (!pt->bDetectColorRing && !pt->bDetectTone)
        {
			++pt->dwNoSoundAfterDial;
            if (pt->dwNoSoundAfterDial > gAmdParam.nNoSoundAfterDialTime)
            {
                pt->nAMDResult = AMD_NOSOUND_AFTERDIAL;
				pt->uEnableAmd = 0; //close amd

                return;
            }
        }
        else
        {
			++pt->dwNoSound;
            if (pt->dwNoSound > gAmdParam.nNoSoundTime)
            {
                pt->nAMDResult = AMD_NOSOUND;
          		pt->uEnableAmd = 0; //close amd 
                return;
            }
        }
    }

    //
    // S1===>S2
    //
	
    if (pt->Stream1)
    {
       
		
		if((abs((uint64_t)(pt->getOverallEnergy.dwOverallEnergy - pt->getOverallEnergy.dwOverallEnergyLast1)) * 100) < (pt->getOverallEnergy.dwOverallEnergy * 30)){
			bIsTone = "true";
		}
		if((abs((uint64_t)(pt->getOverallEnergy.dwPeakEnergyLast1 - pt->getOverallEnergy.dwOverallEnergy)) * 100) < (pt->getOverallEnergy.dwOverallEnergy * 30)){
			bIsTone = "true";
		}
		if (pt->getOverallEnergy.wCurVoiceType == VOICETYPE_TONE)
		{
			bIsTone="true";
		}
        if (!strcasecmp(bIsTone, "true")) //we find the voice is tone!
        {
			++pt->dwInforToneCnt;
			//if (amd->dwInforToneCnt > (Cfg.AMDCfg.S2_TOn%2 + Cfg.AMDCfg.S2_TOn/2))))
			if (pt->dwInforToneCnt > (gAmdParam.nS2TOn%2 + gAmdParam.nS2TOn/2))
			{
				pt->Stream2 = 2;
			}
            //pop the tone event once
            if (pt->dwInforToneCnt > gAmdParam.nS2TOn) //Add by pgy for SC-190 ,2008/05/28
            {
                // if (!pt->bDetectTone) //Add by pgy for SC-190 2008/05/28	//SC-4078 
                //  {
                    //ftdmchan->bDetectTone = "true";
					pt->bDetectTone = 1;
					pt->dwInforToneCnt=0;	//clear tone count!
                    pt->dwNoSound = 0;
                    pt->nAMDResult = AMD_TONE;
					++pt->dwEchoHighCount;	
                // }
				
            }
        }
        else
        {
			pt->dwEchoHighCount = 0;
            pt->dwInforToneCnt = 0;
            pt->Stream2 = 1; 
	    }
    }
    else
    {
		pt->dwEchoHighCount = 0;
        pt->dwInforToneCnt = 0;
        pt->Stream2 = 0;

    }


    AnalyzeActualPersonPickup(pt);

	if(pt->getOverallEnergy.dwOverallEnergyLast1 == 0)	
		pt->getOverallEnergy.dwPeakEnergyLast1 = pt->getOverallEnergy.dwOverallEnergy;
	else			
		pt->getOverallEnergy.dwPeakEnergyLast1 = (pt->getOverallEnergy.dwOverallEnergy + pt->getOverallEnergy.dwOverallEnergyLast1) / 2;
	pt->getOverallEnergy.dwOverallEnergyLast1 = pt->getOverallEnergy.dwOverallEnergy;
	
}
void get_overall_energy(amd_acutalpickup_t *s, const int16_t data[], int samples,float rxgain){
	int32_t vol;
	uint32_t k;
	int32_t samp_s32;
	int32_t vol_gain;
	int16_t tmp[1024]={0};
	//int32_t max_level = 0;
	// int32_t count_level = 0;
	if (rxgain < -24) vol = -24;
	if (rxgain >  24) vol =  24;
	if (rxgain >= -24 && rxgain <= 24) vol = rxgain;
	vol_gain =  tbl_db[vol+24];
	for (k = 0; k < samples; k++){
		samp_s32 = data[k];
		samp_s32 = samp_s32*vol_gain/8192;
		if (samp_s32 > 32767) tmp[k] = 32767;
		else if (samp_s32 < -32767) tmp[k] = -32767;
		else tmp[k] = samp_s32;
	}
	int32_t level;
	for (k = 0; k < samples; k++){
	
		short xamp =tmp[k]>>7;
		s->getOverallEnergy.max_level += (int32_t)xamp*xamp;
		// ftdmchan->count_level++;
		// if (ftdmchan->count_level == 160) {  
			// level = 8*_sqrt(2*ftdmchan->max_level);
			// ftdmchan->dwOverallEnergy = level;
			// ftdmchan->max_level = 0;
			// ftdmchan->count_level = 0;
		// }
	}
	level = 8*_sqrt(2*s->getOverallEnergy.max_level);
	s->getOverallEnergy.dwOverallEnergy = level;
	s->getOverallEnergy.max_level = 0;
}

void InitAmd(amd_acutalpickup_t *pickup)
{
	
	pickup->getOverallEnergy.dwOverallEnergy = 0;
    pickup->getOverallEnergy.dwOverallEnergyLast1 = 0;
	pickup->getOverallEnergy.dwPeakEnergyLast1 = 0;
	pickup->getOverallEnergy.wCurVoiceType = 0;
	pickup->Stream1 = 0;
	pickup->Stream2 = 0;
	pickup->Stream3 = 0;
	pickup->dwInforToneCnt = 0;
	pickup->dwS2OnCnt = 0;
	pickup->dwS2OffCnt = 0;
	pickup->dwS3OnCnt = 0; 
	pickup->dwS3OffCnt = 0;
	pickup->nStatus = T1_WaitOff;
	pickup->bDetectColorRing = 0;
	pickup->bDetectTone = 0;
	pickup->dwNoSound = 0;
	pickup->dwNoSoundAfterDial = 0;
	pickup->dwTimeOut = 0;
	pickup->nAMDResult = -1;  
	pickup->nOldStatus = -1;
    pickup->uEnableAmd = 0;	
	pickup->dwEchoHighCount = 0;	
	pickup->dwEchoLowCount = 0;	
}


void set_amdtime_param(AmdPara *pAmdPara){
    memcpy(&gAmdParam, pAmdPara, sizeof(gAmdParam));
    return ;
}
