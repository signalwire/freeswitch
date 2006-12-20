#include <time.h>
#include <win_etpan.h>

struct tm *gmtime_r (const time_t *timep, struct tm *result) {
	*result = *gmtime( timep);
	return result;
}
struct tm *localtime_r (const time_t *timep, struct tm *result) {
	*result = *localtime( timep); 
	return result;
}