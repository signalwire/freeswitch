#include "blade.h"
#include "tap.h"


int main(void)
{
	ks_status_t status;
	blade_handle_t *bh = NULL;

	blade_init();

	plan(1);

	status = blade_handle_create(&bh, NULL);
	status = blade_handle_destroy(&bh);

	ok(status == KS_STATUS_SUCCESS);
	done_testing();

	blade_shutdown();

	return 0;
}
