#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tpl.h"

#define COUNT 10
#define BUF_SIZE 256
const char *filename = "/tmp/test115.tpl";

typedef struct {
   char* s;
} s1_t;


typedef struct {
   char* s;
   int i;
} s2_t;


typedef struct {
   char c[BUF_SIZE];
   char* s;
   int i;
} s3_t;


const char hw[]="hello, world!";

int main ()
{
   tpl_node* tn;
   s1_t* s1, *S1;
   s2_t* s2, *S2;
   s3_t* s3, *S3;
   int i;

   /* case 1: */
   s1 = (s1_t*)calloc (sizeof (s1_t), COUNT);
   for(i=0; i < COUNT; i++) {
     s1[i].s = malloc(sizeof(hw));
     memcpy(s1[i].s, hw, sizeof(hw));
     s1[i].s[sizeof(hw)-2]='0'+i;
   }
   tn = tpl_map ("S(s)#", s1, COUNT);
   tpl_pack (tn, 0);
   tpl_dump (tn, TPL_FILE, filename);
   tpl_free (tn);
   for(i=0; i < COUNT; i++) free(s1[i].s);

   S1 = (s1_t*)calloc (sizeof (s1_t), COUNT);
   memset(S1, 0xff, sizeof(s1_t)*COUNT);
   tn = tpl_map ("S(s)#", S1, COUNT);
   tpl_load (tn, TPL_FILE, filename);
   tpl_unpack (tn, 0);
   tpl_free (tn);

   for(i=0; i<COUNT; i++) {
     printf("%s\n", S1[i].s);
   }


   /* case 2: */
   s2 = (s2_t*)calloc (sizeof (s2_t), COUNT);
   for(i=0; i < COUNT; i++) {
     s2[i].s = malloc(sizeof(hw));
     memcpy(s2[i].s, hw, sizeof(hw));
     s2[i].s[sizeof(hw)-2]='0'+i;
     s2[i].i=i;
   }
   tn = tpl_map ("S(si)#", s2, COUNT);
   tpl_pack (tn, 0);
   tpl_dump (tn, TPL_FILE, filename);
   tpl_free (tn);
   for(i=0; i < COUNT; i++) free(s2[i].s);

   S2 = (s2_t*)calloc (sizeof (s2_t), COUNT);
   memset(S2, 0xff, sizeof(s2_t)*COUNT);
   tn = tpl_map ("S(si)#", S2, COUNT);
   tpl_load (tn, TPL_FILE, filename);
   tpl_unpack (tn, 0);
   tpl_free (tn);

   for(i=0; i<COUNT; i++) {
     printf("%s, %u\n", S2[i].s, S2[i].i);
   }


   /* case 3: */
   s3 = (s3_t*)calloc (sizeof (s3_t), COUNT);
   for(i=0; i < COUNT; i++) {
     memset(s3[i].c, 'a', BUF_SIZE);
     s3[i].c[BUF_SIZE-1]='\0';
     s3[i].s = malloc(sizeof(hw));
     memcpy(s3[i].s, hw, sizeof(hw));
     s3[i].s[sizeof(hw)-2]='0'+i;
     s3[i].i=i;
   }
   tn = tpl_map ("S(c#si)#", s3, BUF_SIZE, COUNT);
   tpl_pack (tn, 0);
   tpl_dump (tn, TPL_FILE, filename);
   tpl_free (tn);

   S3 = (s3_t*)calloc (sizeof (s3_t), COUNT);
   memset(S3, 0xff, sizeof(s3_t)*COUNT);
   tn = tpl_map ("S(c#si)#", S3, BUF_SIZE, COUNT);
   tpl_load (tn, TPL_FILE, filename);
   tpl_unpack (tn, 0);
   tpl_free (tn);

   for(i=0; i<COUNT; i++) {
     printf("%s, %u\n", S3[i].s, S3[i].i);
     printf("%s\n", S3[i].c);
   }

   return 0;
}

