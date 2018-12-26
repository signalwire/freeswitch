/*
 * cacert.cpp -- CA Certificate for cURL on Windows
 *
 * Copyright (c) 2018 SignalWire, Inc
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifdef WIN32

#include <switch.h>
#include <switch_curl.h>
#include <switch_ssl.h>

#include <vector>
#include <wincrypt.h>

std::vector<X509*> m_trustedCertificateList;

SWITCH_BEGIN_EXTERN_C

static void addCertificatesForStore(LPCSTR name)
{
	HCERTSTORE storeHandle = CertOpenSystemStore(NULL, name);

	if (storeHandle == nullptr)	{
		return;
	}

	PCCERT_CONTEXT windowsCertificate = CertEnumCertificatesInStore(storeHandle, nullptr);

	while (windowsCertificate != nullptr) {
		X509 *opensslCertificate = d2i_X509(nullptr, const_cast<unsigned char const **>(&windowsCertificate->pbCertEncoded), windowsCertificate->cbCertEncoded);
		if (opensslCertificate == nullptr) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "A certificate could not be converted.\n");
		} else {
			m_trustedCertificateList.push_back(opensslCertificate);
		}

		windowsCertificate = CertEnumCertificatesInStore(storeHandle, windowsCertificate);
	}

	CertCloseStore(storeHandle, 0);
}

static void setupSslContext(SSL_CTX* context)
{
	X509_STORE* certStore = SSL_CTX_get_cert_store(context);
	for (X509 *x509 : m_trustedCertificateList)	{
		X509_STORE_add_cert(certStore, x509);
	}
}

void sslLoadWindowsCACertificate() {
	if (m_trustedCertificateList.empty()) {
		addCertificatesForStore("CA");
		addCertificatesForStore("AuthRoot");
		addCertificatesForStore("ROOT");
	}
}

void sslUnLoadWindowsCACertificate()
{
	for (X509 *x509 : m_trustedCertificateList) {
		X509_free(x509);
	}

	m_trustedCertificateList.clear();
}

int sslContextFunction(void* curl, void* sslctx, void* userdata)
{
	setupSslContext(reinterpret_cast<SSL_CTX *>(sslctx));
	return CURLE_OK;
}

SWITCH_END_EXTERN_C

#endif

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet
 */
