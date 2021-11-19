#include "AuthManager.h"
int main()
{
	int n = StartAuthManagerEx(4,0,"sn00f45baecf0dae", "pw274bb0c1636880",0,0,0);	
	printf("%d\n",n);
	CloseAuthManager();
}
