/*
 *
 *
 */
#define _BSD_SOURCE
#include "private/ftdm_core.h"

#ifdef HAVE_EXECINFO_H
#include <stdlib.h>
#include <unistd.h>
#include <execinfo.h>
#include <syscall.h>

#define FTDM_BACKTRACE_MAX	50

FT_DECLARE(ftdm_status_t) ftdm_backtrace_walk(void (* callback)(const int tid, const void *addr, const char *symbol, void *priv), void *priv)
{
	void *stacktrace[FTDM_BACKTRACE_MAX];
	char **symbols = NULL;
	size_t size = 0;
	pid_t tid = 0;
	int si = 0;

	if (!callback) {
		return FTDM_EINVAL;
	}

	tid = syscall(SYS_gettid);

	size = backtrace(stacktrace, ftdm_array_len(stacktrace));
	symbols = backtrace_symbols(stacktrace, size);

	for (si = 0; si < size; si++) {
		callback(tid, stacktrace[si], symbols[si], priv);
	}

	free(symbols);
	return FTDM_SUCCESS;
}

#else	/* !HAVE_EXECINFO_H */

FT_DECLARE(ftdm_status_t) ftdm_backtrace_walk(void (* callback)(const int tid, const void *addr, const char *symbol, void *priv), void *priv)
{
	ftdm_log(FTDM_LOG_DEBUG, "Stack traces are not available on this platform!\n");
	return FTDM_NOTIMPL;
}

#endif


static void span_backtrace(const int tid, const void *addr, const char *symbol, void *priv)
{
	ftdm_span_t *span = priv;
	ftdm_log(FTDM_LOG_DEBUG, "[%d][tid:%d] %p -> %s\n",
		ftdm_span_get_id(span), tid, addr, symbol);
}

FT_DECLARE(ftdm_status_t) ftdm_backtrace_span(ftdm_span_t *span)
{
	return ftdm_backtrace_walk(&span_backtrace, span);
}


static void chan_backtrace(const int tid, const void *addr, const char *symbol, void *priv)
{
	ftdm_channel_t *chan = priv;
	ftdm_log(FTDM_LOG_DEBUG, "[%d:%d][tid:%d] %p -> %s\n",
		ftdm_channel_get_span_id(chan), ftdm_channel_get_id(chan), tid, addr, symbol);
}

FT_DECLARE(ftdm_status_t) ftdm_backtrace_chan(ftdm_channel_t *chan)
{
	return ftdm_backtrace_walk(&chan_backtrace, chan);
}
