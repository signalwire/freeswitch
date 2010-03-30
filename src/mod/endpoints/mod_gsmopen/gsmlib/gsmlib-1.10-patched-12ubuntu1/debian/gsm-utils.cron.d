# /etc/cron.d/gsm-utils: crontab fragment for gsm-utils

*/5 * * * *	root	if [ -x /usr/bin/gsmsmsrequeue ]; then /usr/bin/gsmsmsrequeue; fi
