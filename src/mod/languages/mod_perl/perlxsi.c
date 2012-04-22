#if defined (__SVR4) && defined (__sun)
#include <uconfig.h>
#endif
#include <EXTERN.h>
#if defined (__SVR4) && defined (__sun)
#include <embed.h>
#endif
#include <perl.h>

EXTERN_C void xs_init(pTHX);

EXTERN_C void boot_DynaLoader(pTHX_ CV * cv);
EXTERN_C void boot_freeswitch(pTHX_ CV * cv);

EXTERN_C void xs_init(pTHX)
{
	char *file = __FILE__;
	dXSUB_SYS;

	/* DynaLoader is a special case */
	newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
	newXS("freeswitchc::boot_freeswitch", boot_freeswitch, file);
}
