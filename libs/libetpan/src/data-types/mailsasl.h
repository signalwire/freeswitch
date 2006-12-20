#ifndef MAILSASL_H

#define MAILSASL_H

/* if Cyrus-SASL is used outside of libetpan */
void mailsasl_external_ref(void);

void mailsasl_ref(void);
void mailsasl_unref(void);

#endif
