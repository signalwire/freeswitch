#include <windows.h>
#include "tpl.h"

int WINAPI WinMain (HINSTANCE hInstance, 
                    HINSTANCE hPrevInstance, 
                    PSTR szCmdLine, 
                    int iCmdShow) 
{
  char *status;
  int rc=0,i,j;
  tpl_node *tn;
  void *img;
  size_t sz;

  tn = tpl_map("A(i)", &i);
  for(i=0; i<10; i++) tpl_pack(tn,1);
  tpl_dump(tn,TPL_MEM,&img, &sz);
  tpl_free(tn);

  j=0;
  tn = tpl_map("A(i)", &i);
  tpl_load(tn,TPL_MEM,img,sz);
  while(tpl_unpack(tn,1) > 0) {
        if (i != j++) {
           rc = -1;
           break;
        }
  }
  tpl_free(tn);

  MessageBox (NULL, (rc==0)?"Test1 passed":"Test1 failed", 
        "MinGW Tpl Test", MB_OK);

  /* Test 2 */
  tn = tpl_map("A(i)", &i);
  for(i=0; i<10; i++) tpl_pack(tn,1);
  tpl_dump(tn,TPL_FILE,"mgwtest.tpl");
  tpl_free(tn);

  j=0;
  tn = tpl_map("A(i)", &i);
  tpl_load(tn,TPL_FILE,"mgwtest.tpl");
  while(tpl_unpack(tn,1) > 0) {
        if (i != j++) {
           rc = -1;
           break;
        }
  }
  tpl_free(tn);

  MessageBox (NULL, (rc==0)?"Test2 passed":"Test2 failed", 
        "MinGW Tpl Test", MB_OK);

  return (0);
}
     
