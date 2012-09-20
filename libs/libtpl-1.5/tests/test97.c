#include "tpl.h"
#include <stdio.h>
#include <inttypes.h>

const char *filename = "/tmp/test97.tpl";

int main() {
   tpl_node *tn;
   int16_t j = -128;
   uint16_t v=65535;

   tn = tpl_map("jv", &j, &v);
   tpl_pack(tn,0);
   tpl_dump(tn,TPL_FILE,filename);
   tpl_free(tn);

   j = v = 0;

   tn = tpl_map("jv", &j, &v);
   tpl_load(tn,TPL_FILE,filename);
   tpl_unpack(tn,0);
   tpl_free(tn);

   printf("j is %d, v is %d\n", (int)j, (int)v);
   return(0);
   
}
