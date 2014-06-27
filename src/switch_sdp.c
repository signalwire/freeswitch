
#include <switch.h>
#include <sofia-sip/sdp.h>

sdp_connection_t *sdp_media_connections(sdp_media_t const *m)
{
  if (m) {
    if (m->m_connections)
      return m->m_connections;
    if (m->m_session)
      return m->m_session->sdp_connection;
  }
  return NULL;
}

#include <su_alloc.c>
#include <su_errno.c>
#include <su_string.c>
#include <sdp_parse.c>
#ifdef _MSC_VER
#define longlong __int64
#include <strtoull.c>
#endif

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
