#include "tpl.h"
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#define SUM_LENGTH 16 
#define MS_COUNT 9 
struct sum_buf {
    int64_t offset;
    int len;  
    uint32_t sum1;       
    int chain; 
    uint16_t flags; 
    char sum2[SUM_LENGTH]; 
};
 
struct sum_struct {
    int64_t flength;
    struct sum_buf *sums; 
    int count; 
    int blength; 
    int remainder;
    int s2length;  
};
 
const char *filename =  "/tmp/test106.tpl"; 

int pack(int use_fd)  
{
    tpl_node *tn;
    struct sum_struct ms;
    int fd=-1,j;
    unsigned perms;
    
     perms = S_IRUSR|S_IWUSR; 
     if (use_fd) {
       if ( (fd=open( filename,O_WRONLY|O_CREAT,perms)) == -1) {
           printf("failed to open %s: %s", filename, strerror(errno));
           return(-1);
       }
     }
 
     ms.flength = 1000;
     ms.count = MS_COUNT;
     ms.blength = 23;
     ms.remainder = 43;
     ms.s2length = 16;
 
     ms.sums = (struct sum_buf*) malloc((sizeof(struct sum_buf))*ms.count);
 
    for(j=0;j<ms.count;j++)
    {
        ms.sums[j].offset = (uint64_t) j;
        ms.sums[j].len = j*5;
        ms.sums[j].sum1 = j*10;
        ms.sums[j].chain = j*1000+5000;
        ms.sums[j].flags = j*3 + 15; 
        memset(ms.sums[j].sum2,0,SUM_LENGTH); 
        strcpy(ms.sums[j].sum2,"Deepak");
    }
 
    tn = tpl_map( "IS(Iiuijc#)#iiii", &ms.flength,ms.sums,SUM_LENGTH,
      ms.count,&ms.count,&ms.blength,&ms.remainder,&ms.s2length);
    tpl_pack( tn, 0 );

    if (use_fd) {
      tpl_dump(tn,TPL_FD, fd); 
      close(fd);
    } else {
      tpl_dump(tn,TPL_FILE,filename);  
    }

    tpl_free( tn );
    
    return 0;
}
 
int unpack(int use_fd) {
    tpl_node *tn;
    struct sum_struct ms;
    unsigned perms;
    int fd=-1,i;
 
    perms = S_IRUSR|S_IWUSR; 
    if (use_fd) {
      if ( (fd=open( filename,O_RDONLY,perms)) == -1) {
          printf("failed to open %s: %s", filename, strerror(errno));
          return(-1);
      }
    }
 
    ms.sums = (struct sum_buf*) malloc((sizeof(struct sum_buf))*MS_COUNT);
 
    tn = tpl_map( "IS(Iiuijc#)#iiii", &ms.flength,ms.sums,SUM_LENGTH,
      MS_COUNT,&ms.count,&ms.blength,&ms.remainder,&ms.s2length);
    if (use_fd) tpl_load(tn, TPL_FD, fd);
    else tpl_load(tn, TPL_FILE, filename);
    tpl_unpack(tn, 0 );
    tpl_free( tn );
    if (use_fd) close(fd);

    printf("%d\n", (int)(ms.flength)); 
    for(i=0; i < MS_COUNT; i++) {
      printf("  %d, %d, %u, %d, %d, %s\n", (int)(ms.sums[i].offset), ms.sums[i].len, ms.sums[i].sum1, ms.sums[i].chain, (int)(ms.sums[i].flags), ms.sums[i].sum2);
    }
    printf("%d,%d,%d,%d\n", ms.count, ms.blength, ms.remainder, ms.s2length);
 
    return 0;
}
 
int main() {
  printf("testing with TPL_FILE:\n");
  pack(0);
  unpack(0);

  printf("testing with TPL_FD:\n");
  pack(1);
  unpack(1);

  return 0;
}
