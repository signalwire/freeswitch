#include <windows.h>
#include "tpl.h"

int WINAPI WinMain (HINSTANCE hInstance, 
                    HINSTANCE hPrevInstance, 
                    PSTR szCmdLine, 
                    int iCmdShow) 
{
  MessageBox (NULL, "Hello", "Hello Demo", MB_OK);
  return (0);
}
     
