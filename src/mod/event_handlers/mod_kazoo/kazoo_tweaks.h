#pragma once

#include <switch.h>

typedef enum {
	KZ_TWEAK_INTERACTION_ID,
	KZ_TWEAK_EXPORT_VARS,
	KZ_TWEAK_SWITCH_URI,
	KZ_TWEAK_REPLACES_CALL_ID,
	KZ_TWEAK_LOOPBACK_VARS,
	KZ_TWEAK_CALLER_ID,
	KZ_TWEAK_TRANSFERS,
	KZ_TWEAK_BRIDGE,
	KZ_TWEAK_BRIDGE_REPLACES_ALEG,
	KZ_TWEAK_BRIDGE_REPLACES_CALL_ID,
	KZ_TWEAK_BRIDGE_VARIABLES,
	KZ_TWEAK_RESTORE_CALLER_ID_ON_BLIND_XFER,

	/* No new flags below this line */
	KZ_TWEAK_MAX
} kz_tweak_t;

void kz_tweaks_start();
void kz_tweaks_stop();
SWITCH_DECLARE(const char *) kz_tweak_name(kz_tweak_t tweak);
SWITCH_DECLARE(switch_status_t) kz_name_tweak(const char *name, kz_tweak_t *type);


#define kz_test_tweak(flag) (kazoo_globals.tweaks[flag] ? 1 : 0)
#define kz_set_tweak(flag) kazoo_globals.tweaks[flag] = 1
#define kz_clear_tweak(flag) kazoo_globals.tweaks[flag] = 0

