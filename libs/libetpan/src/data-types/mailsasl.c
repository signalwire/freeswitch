#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailsasl.h"

#ifdef USE_SASL

#ifdef LIBETPAN_REENTRANT
#include <pthread.h>
#endif
#include <sasl/sasl.h>

#ifdef LIBETPAN_REENTRANT
static pthread_mutex_t sasl_lock = PTHREAD_MUTEX_INITIALIZER;
#endif
static int sasl_use_count = 0;

void mailsasl_external_ref(void)
{
#ifdef LIBETPAN_REENTRANT
  pthread_mutex_lock(&sasl_lock);
#endif
  sasl_use_count ++;
#ifdef LIBETPAN_REENTRANT
  pthread_mutex_unlock(&sasl_lock);
#endif
}

void mailsasl_ref(void)
{
#ifdef LIBETPAN_REENTRANT
  pthread_mutex_lock(&sasl_lock);
#endif
  sasl_use_count ++;
  if (sasl_use_count == 1)
    sasl_client_init(NULL);
#ifdef LIBETPAN_REENTRANT
  pthread_mutex_unlock(&sasl_lock);
#endif
}

void mailsasl_unref(void)
{
#ifdef LIBETPAN_REENTRANT
  pthread_mutex_lock(&sasl_lock);
#endif
  sasl_use_count --;
  if (sasl_use_count == 0) {
    sasl_done();
  }
#ifdef LIBETPAN_REENTRANT
  pthread_mutex_unlock(&sasl_lock);
#endif
}

#else

void mailsasl_external_ref(void)
{
}

void mailsasl_ref(void)
{
}

void mailsasl_unref(void)
{
}

#endif
