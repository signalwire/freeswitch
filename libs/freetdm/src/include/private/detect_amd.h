#ifndef _detect_amd_H_
#define _detect_amd_H_

#ifndef MAX_CH
#define MAX_CH                      64
#endif

#ifndef MAX_DTMF_DIGITS
#define MAX_DTMF_DIGITS 128
#endif

#define TONE_ANALYZE_PERIOD  20  //tone analyzation period(ms) added by dsq 
#if 0
#define S2_TOn				 5  //80/16
#define S2_TOff				(400/16) 
#define AMDTimeA			(600/16)
#define AMDTimeB			(180/16)
#define AMDTimeC			(1200/16)
#define AMDTimeD			(1000/16)
#define SilentEnergy 		10000
#define TIMEOUT             70000
#define NoSoundTime         6000
#endif 
enum
{
	VOICETYPE_NOVOICE = 0,
    VOICETYPE_TONE,
    VOICETYPE_VOICE,
};

typedef struct
{ 
    uint32_t  	dwOverallEnergy;
    uint32_t  	dwOverallEnergyLast1;
	uint32_t  	dwPeakEnergyLast1;
	int32_t 	wCurVoiceType;
	int32_t 	max_level;
}over_energy_t;

typedef struct {
    int nS2TOn;
    int nS2TOff	;			
    int nAMDTimeA;			
    int nAMDTimeB;			
    int nAMDTimeC;		
    int nAMDTimeD;	
    int ToneTimeLimit;			
    int nSilentEnergy; 		
    int nTimeout;
    int nNoSoundAfterDialTime;           
    int nNoSoundTime ;        
}AmdPara;

typedef struct 
{
    over_energy_t getOverallEnergy;
    uint8_t     uEnableAmd;
    uint8_t 	Stream1;
    int32_t 	Stream2;	
    uint8_t 	Stream3;
    uint32_t  	dwInforToneCnt;
    uint32_t 	dwS2OnCnt;
    uint32_t 	dwS2OffCnt;
    uint32_t 	dwS3OnCnt;
    uint32_t 	dwS3OffCnt;
    int32_t  	nStatus;
	int32_t  	nOldStatus;
    uint8_t 	bDetectColorRing;
	uint8_t 	bDetectTone;
    uint32_t 	dwNoSoundAfterDial;
    uint32_t 	dwNoSound;
    uint32_t 	dwTimeOut;
    int32_t  	nAMDResult;
    int32_t     nLastAMDReslut;
	uint32_t 	dwEchoHighCount;
	uint32_t 	dwEchoLowCount;
}amd_acutalpickup_t ;

enum
{
    T1_WaitOff,
    T1_CountOff,
    T2_CountOn,
    T3_CountOff,
};

enum
{
    AMD_ACTUAL_PICKUP, 
    AMD_TONE,   
    AMD_COLORRING,  
    AMD_TIMEOUT,  
    AMD_NOSOUND,  
    AMD_NOSOUND_AFTERDIAL,
    AMD_BUSYTONE,   
    AMD_FAX,   
	AMD_BEEPTONE,
};
//end added by dsq for DS-73667


#ifdef __cplusplus
extern "C" {
#endif

void InitAmd(amd_acutalpickup_t *pickup);
void get_overall_energy(amd_acutalpickup_t *s, const int16_t data[], int samples,float rxgain);
void ExecutAMD(amd_acutalpickup_t *pt);
void set_amdtime_param(AmdPara *pAmdPara);
#ifdef __cplusplus
};
#endif

#endif
