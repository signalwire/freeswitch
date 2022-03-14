#ifndef _detect_shaihao_H_
#define _detect_shaihao_H_
#include <stdio.h>

enum
{
	CHECK_FALSE= 0,	
	CHECK_TRUE,
};
#define FRAME_SIZE 128

typedef struct  {
    char trainingPath[1280];
    char recordname[64];
    char debugrecordname[128];
    FILE *pRecordfile;
    int nShaihaoState;
    int nShaihaoResult;
    int nProcResult;
}shaihao_state_s_t;

#ifdef __cplusplus
extern "C" {
#endif

int ProcessShaihao(shaihao_state_s_t *pState);
void Init_shaihaoSate(shaihao_state_s_t *pState);
int get_energy_state(shaihao_state_s_t *pState);
#ifdef __cplusplus
};
#endif

#endif
