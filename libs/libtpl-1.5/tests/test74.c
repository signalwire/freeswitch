#include <stdio.h>
#include <string.h>
#include "tpl.h"

#define F1_LEN 200
#define F2_LEN 100

struct ms_t {
    int i;
    double f1[F1_LEN];
    double f2[F2_LEN];
};

int main() {
    tpl_node *tn;
    struct ms_t ms, ms2;
    int i;

    ms.i = 1234;
    for(i=0; i < F1_LEN; i++) ms.f1[i] = i * 1.5;
    for(i=0; i < F2_LEN; i++) ms.f2[i] = i * 0.5;

    tn = tpl_map("S(if#f#)", &ms, F1_LEN, F2_LEN);
    tpl_pack(tn,0);
    tpl_dump(tn,TPL_FILE,"/tmp/test74.tpl");
    tpl_free(tn);

    memset(&ms2,0,sizeof(struct ms_t));
    tn = tpl_map("S(if#f#)", &ms2, F1_LEN, F2_LEN);
    tpl_load(tn,TPL_FILE,"/tmp/test74.tpl");
    tpl_unpack(tn,0);
    tpl_free(tn);

    printf("%d\n", ms2.i);
    for(i=0; i < F1_LEN; i++) printf("%.2f\n", ms2.f1[i]);
    for(i=0; i < F2_LEN; i++) printf("%.2f\n", ms2.f2[i]);
    
    return(0);
}
