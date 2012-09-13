#include "tpl.h"
#include <stdio.h>
#include <inttypes.h>

const char *filename = "/tmp/test98.tpl";

int main() {
   tpl_node *tn;
   int16_t j = -128;
   uint16_t v=65535;

   tn = tpl_map("A(jv)", &j, &v);
   tpl_pack(tn,1); j -= 1; v-= 1;
   tpl_pack(tn,1); j -= 1; v-= 1;
   tpl_pack(tn,1); 
   tpl_dump(tn,TPL_FILE,filename);
   tpl_free(tn);

   j = v = 0;

   tn = tpl_map("A(jv)", &j, &v);
   tpl_load(tn,TPL_FILE,filename);
   while (tpl_unpack(tn,1) > 0) {
     printf("j is %d, v is %d\n", (int)j, (int)v);
   }
   tpl_free(tn);
   return(0);
}
