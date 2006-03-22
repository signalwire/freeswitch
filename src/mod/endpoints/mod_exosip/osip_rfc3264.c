/*
  The oSIP library implements the Session Initiation Protocol (SIP -rfc3261-)
  Copyright (C) 2001,2002,2003,2004,2005  Aymeric MOIZARD jack@atosc.org
  
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  
  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
  
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdlib.h>

#include <osipparser2/osip_port.h>
#include <osipparser2/osip_rfc3264.h>
#include <osip_rfc3264i.h>      /* internal include */

/**
 * Initialize negotiation facility..
 * @param config The element to work on.
 */
int
osip_rfc3264_init (struct osip_rfc3264 **config)
{
  osip_rfc3264_t *cnf;

  *config = NULL;
  cnf = (osip_rfc3264_t *) osip_malloc (sizeof (osip_rfc3264_t));
  if (cnf == NULL)
    return -1;
  memset (cnf, 0, sizeof (osip_rfc3264_t));
  *config = cnf;
  return 0;
}


/**
 * Free negotiation facility.
 * @param config The element to work on.
 */
void
osip_rfc3264_free (struct osip_rfc3264 *config)
{
  osip_rfc3264_t *cnf = (osip_rfc3264_t *) config;
  int i;

  if (config == NULL)
    return;

  for (i = 0; i < MAX_AUDIO_CODECS; i++)
    {
      if (cnf->audio_medias[i] != NULL)
        {
          sdp_media_free (cnf->audio_medias[i]);
          cnf->audio_medias[i] = NULL;
        }
    }
  for (i = 0; i < MAX_VIDEO_CODECS; i++)
    {
      if (cnf->video_medias[i] != NULL)
        {
          sdp_media_free (cnf->video_medias[i]);
          cnf->video_medias[i] = NULL;
        }
    }
  for (i = 0; i < MAX_T38_CODECS; i++)
    {
      if (cnf->t38_medias[i] != NULL)
        {
          sdp_media_free (cnf->t38_medias[i]);
          cnf->t38_medias[i] = NULL;
        }
    }
  for (i = 0; i < MAX_APP_CODECS; i++)
    {
      if (cnf->app_medias[i] != NULL)
        {
          sdp_media_free (cnf->app_medias[i]);
          cnf->app_medias[i] = NULL;
        }
    }
  osip_free (cnf);
}


/**
 * Test if a media exist in the configuration.
 * @param config The element to work on.
 * @param pos The index of the media element.
 */
int
osip_rfc3264_endof_media (struct osip_rfc3264 *config, int pos)
{
  if (config == NULL)
    return -1;

  return 0;
}


/**
 * Get a media from the configuration.
 * @param config The element to work on.
 * @param pos The index of the media element to get.
 */
sdp_media_t *
osip_rfc3264_get (struct osip_rfc3264 * config, int pos)
{
  if (config == NULL)
    return NULL;

  return NULL;
}


/**
 * Remove a media from the configuration.
 * @param config The element to work on.
 * @param pos The index of the media element to remove.
 */
int
osip_rfc3264_remove (struct osip_rfc3264 *config, int pos)
{
  if (config == NULL)
    return -1;

  return 0;
}


/**
 * Remove all medias from the configuration.
 * @param config The element to work on.
 */
int
osip_rfc3264_reset_media (struct osip_rfc3264 *config)
{
  if (config == NULL)
    return -1;

  return 0;
}


/**
 * Add a media (for audio) in the configuration.
 * @param config The element to work on.
 * @param med The media element to add.
 * @param pos The index of the media element to add.
 */
int
osip_rfc3264_add_audio_media (struct osip_rfc3264 *config, sdp_media_t * med,
                              int pos)
{
  osip_rfc3264_t *cnf = (osip_rfc3264_t *) config;

  if (config == NULL)
    return -1;
  if (pos >= MAX_AUDIO_CODECS)
    return -1;

  if (pos == -1)
    {
      for (pos = 0; pos < MAX_AUDIO_CODECS && cnf->audio_medias[pos] != NULL;
           pos++)
        {
        }
    }
  if (pos >= MAX_AUDIO_CODECS)
    return -1;                  /* no space left */

  cnf->audio_medias[pos] = med;
  return 0;
}


/**
 * Remove a media in the configuration.
 * @param config The element to work on.
 * @param pos The index of the media element to remove.
 */
int
osip_rfc3264_del_audio_media (struct osip_rfc3264 *config, int pos)
{
  osip_rfc3264_t *cnf = (osip_rfc3264_t *) config;

  if (config == NULL)
    return -1;
  if (pos >= MAX_AUDIO_CODECS)
    return -1;
  sdp_media_free (cnf->audio_medias[pos]);
  cnf->audio_medias[pos] = NULL;
  return 0;
}

/**
 * Add a media (for video) in the configuration.
 * @param config The element to work on.
 * @param med The media element to add.
 * @param pos The index of the media element to add.
 */
int
osip_rfc3264_add_video_media (struct osip_rfc3264 *config, sdp_media_t * med,
                              int pos)
{
  osip_rfc3264_t *cnf = (osip_rfc3264_t *) config;

  if (config == NULL)
    return -1;
  if (pos >= MAX_VIDEO_CODECS)
    return -1;

  if (pos == -1)
    {
      for (pos = 0; pos < MAX_VIDEO_CODECS && cnf->video_medias[pos] != NULL;
           pos++)
        {
        }
    }
  if (pos >= MAX_VIDEO_CODECS)
    return -1;                  /* no space left */

  cnf->video_medias[pos] = med;
  return 0;
}


/**
 * Remove a media in the configuration.
 * @param config The element to work on.
 * @param pos The index of the media element to remove.
 */
int
osip_rfc3264_del_video_media (struct osip_rfc3264 *config, int pos)
{
  osip_rfc3264_t *cnf = (osip_rfc3264_t *) config;

  if (config == NULL)
    return -1;
  if (pos >= MAX_VIDEO_CODECS)
    return -1;
  sdp_media_free (cnf->video_medias[pos]);
  cnf->video_medias[pos] = NULL;
  return 0;
}

/**
 * Add a media (for t38) in the configuration.
 * @param config The element to work on.
 * @param med The media element to add.
 * @param pos The index of the media element to add.
 */
int
osip_rfc3264_add_t38_media (struct osip_rfc3264 *config, sdp_media_t * med,
                            int pos)
{
  osip_rfc3264_t *cnf = (osip_rfc3264_t *) config;

  if (config == NULL)
    return -1;
  if (pos >= MAX_T38_CODECS)
    return -1;

  if (pos == -1)
    {
      for (pos = 0; pos < MAX_T38_CODECS && cnf->t38_medias[pos] != NULL; pos++)
        {
        }
    }
  if (pos >= MAX_T38_CODECS)
    return -1;                  /* no space left */

  cnf->t38_medias[pos] = med;
  return 0;
}


/**
 * Remove a media in the configuration.
 * @param config The element to work on.
 * @param pos The index of the media element to remove.
 */
int
osip_rfc3264_del_t38_media (struct osip_rfc3264 *config, int pos)
{
  osip_rfc3264_t *cnf = (osip_rfc3264_t *) config;

  if (config == NULL)
    return -1;
  if (pos >= MAX_T38_CODECS)
    return -1;
  sdp_media_free (cnf->t38_medias[pos]);
  cnf->t38_medias[pos] = NULL;
  return 0;
}

/**
 * Search for support of a special codec.
 * @param config The element to work on.
 * @param payload The payload to find.
 * @param rtpmap The rtpmap for the payload.
 */
sdp_media_t *
osip_rfc3264_find_audio (struct osip_rfc3264 * config, char *payload, char *rtpmap)
{
  osip_rfc3264_t *cnf = (osip_rfc3264_t *) config;
  int i;

  if (config == NULL)
    return NULL;

  if (rtpmap == NULL)
    {
      for (i = 0; i < MAX_AUDIO_CODECS; i++)
        {
          if (cnf->audio_medias[i] != NULL)
            {
              sdp_media_t *med = cnf->audio_medias[i];
              char *str = (char *) osip_list_get (med->m_payloads, 0);

              /* static payload?: only compare payload number */
              if (strlen (str) == strlen (payload)
                  && 0 == osip_strcasecmp (str, payload))
                return med;
            }
        }
      return NULL;
    }

  for (i = 0; i < MAX_AUDIO_CODECS; i++)
    {
      if (cnf->audio_medias[i] != NULL)
        {
          sdp_media_t *med = cnf->audio_medias[i];

          int pos = 0;

          while (!osip_list_eol (med->a_attributes, pos))
            {
              sdp_attribute_t *attr =
                (sdp_attribute_t *) osip_list_get (med->a_attributes, pos);

              if (0 == osip_strcasecmp ("rtpmap", attr->a_att_field)
                  && attr->a_att_value != NULL)
                {
                  char *tmp = strchr (attr->a_att_value, ' ');
                  char *tmp2 = strchr (rtpmap, ' ');

                  if (tmp != NULL && tmp2 != NULL)
                    {
                      if (0 == osip_strcasecmp (tmp, tmp2))
                        return med;
                    }
                }
              pos++;
            }
        }
    }

  return NULL;
}

/**
 * Search for support of a special codec.
 * @param config The element to work on.
 * @param payload The payload to find.
 * @param rtpmap The rtpmap for the payload.
 */
sdp_media_t *
osip_rfc3264_find_video (struct osip_rfc3264 * config, char *payload, char *rtpmap)
{
  osip_rfc3264_t *cnf = (osip_rfc3264_t *) config;
  int i;

  if (config == NULL)
    return NULL;

  if (rtpmap == NULL)
    {
      for (i = 0; i < MAX_VIDEO_CODECS; i++)
        {
          if (cnf->video_medias[i] != NULL)
            {
              sdp_media_t *med = cnf->video_medias[i];
              char *str = (char *) osip_list_get (med->m_payloads, 0);

              /* static payload?: only compare payload number */
              if (strlen (str) == strlen (payload)
                  && 0 == osip_strcasecmp (str, payload))
                return med;
            }
        }
      return NULL;
    }

  for (i = 0; i < MAX_VIDEO_CODECS; i++)
    {
      if (cnf->video_medias[i] != NULL)
        {
          sdp_media_t *med = cnf->video_medias[i];

          int pos = 0;

          while (!osip_list_eol (med->a_attributes, pos))
            {
              sdp_attribute_t *attr =
                (sdp_attribute_t *) osip_list_get (med->a_attributes, pos);

              if (0 == osip_strcasecmp ("rtpmap", attr->a_att_field)
                  && attr->a_att_value != NULL)
                {
                  char *tmp = strchr (attr->a_att_value, ' ');
                  char *tmp2 = strchr (rtpmap, ' ');

                  if (tmp != NULL && tmp2 != NULL)
                    {
                      if (0 == osip_strcasecmp (tmp, tmp2))
                        return med;
                    }
                }
              pos++;
            }
        }
    }

  return NULL;
}

/**
 * Search for support of a special codec.
 * @param config The element to work on.
 * @param payload The payload to find.
 */
sdp_media_t *
osip_rfc3264_find_t38 (struct osip_rfc3264 * config, char *payload)
{
	//osip_rfc3264_t *cnf = (osip_rfc3264_t *) config;

  if (config == NULL)
    return NULL;
  return NULL;
}

/**
 * Search for support of a special codec.
 * @param config The element to work on.
 * @param payload The payload to find.
 */
sdp_media_t *
osip_rfc3264_find_app (struct osip_rfc3264 * config, char *payload)
{
	//  osip_rfc3264_t *cnf = (osip_rfc3264_t *) config;

  if (config == NULL)
    return NULL;
  return NULL;
}

/**
 * Compare remote sdp packet against local supported media.
 *    Only one media line is checked.
 *
 * @param config The element to work on.
 * @param remote_sdp The remote SDP packet.
 * @param audio_tab The local list of media supported by both side.
 * @param video_tab The local list of media supported by both side.
 * @param t38_tab The local list of media supported by both side.
 * @param app_tab The local list of media supported by both side.
 * @param pos_media The position of the media line to match.
 */
int
osip_rfc3264_match (struct osip_rfc3264 *config,
                    sdp_message_t * remote_sdp,
                    sdp_media_t * audio_tab[],
                    sdp_media_t * video_tab[],
                    sdp_media_t * t38_tab[],
                    sdp_media_t * app_tab[], int pos_media)
{
  sdp_media_t *remote_med;
  int pos;

  audio_tab[0] = NULL;
  video_tab[0] = NULL;
  t38_tab[0] = NULL;
  app_tab[0] = NULL;

  if (config == NULL)
    return -1;

  pos = 0;
  while (!sdp_message_endof_media (remote_sdp, pos))
    {
      if (pos_media == 0)
        {
          remote_med = osip_list_get (remote_sdp->m_medias, pos);
          if (remote_med->m_media != NULL
              && 0 == osip_strcasecmp (remote_med->m_media, "audio"))
            {
              osip_rfc3264_match_audio (config, remote_sdp, remote_med, audio_tab);
          } else if (remote_med->m_media != NULL
                     && 0 == osip_strcasecmp (remote_med->m_media, "video"))
            {
              osip_rfc3264_match_video (config, remote_sdp, remote_med, video_tab);
          } else if (remote_med->m_media != NULL
                     && 0 == osip_strcasecmp (remote_med->m_media, "image"))
            {
              osip_rfc3264_match_t38 (config, remote_sdp, remote_med, t38_tab);
          } else if (remote_med->m_media != NULL
                     && 0 == osip_strcasecmp (remote_med->m_media, "application"))
            {
              osip_rfc3264_match_app (config, remote_sdp, remote_med, app_tab);
            }
          return 0;
        }

      remote_med = NULL;
      pos++;
      pos_media--;
    }

  return -1;
}

/**
 * Compare remote sdp packet against local supported media for audio.
 * @param config The element to work on.
 * @param remote_sdp The remote SDP packet.
 * @param remote_med The remote Media SDP line.
 * @param audio_tab The local list of media supported by both side.
 */
int
osip_rfc3264_match_audio (struct osip_rfc3264 *config,
                          sdp_message_t * remote_sdp,
                          sdp_media_t * remote_med, sdp_media_t * audio_tab[])
{
  osip_rfc3264_t *cnf = (osip_rfc3264_t *) config;
  int num = 0;
  int pos;

  audio_tab[0] = NULL;

  if (cnf == NULL)
    return -1;

  /* search for the audio media line */
  pos = 0;
  while (!osip_list_eol (remote_med->m_payloads, pos))
    {
      char *payload = (char *) osip_list_get (remote_med->m_payloads, pos);
      sdp_media_t *local_med;
      char *rtpmap = NULL;
      int posattr = 0;

      /* search for the rtpmap associated to the payload */
      while (!osip_list_eol (remote_med->a_attributes, posattr))
        {
          sdp_attribute_t *attr =
            (sdp_attribute_t *) osip_list_get (remote_med->a_attributes,
                                               posattr);

          if (0 == osip_strncasecmp (attr->a_att_field, "rtpmap", 6))
            {
              if (attr->a_att_value != NULL &&
                  0 == osip_strncasecmp (attr->a_att_value, payload,
                                         strlen (payload)))
                {
                  /* TODO check if it was not like 101: == 10 */
                  rtpmap = attr->a_att_value;
                  break;
                }
            }
          posattr++;
        }

      local_med = osip_rfc3264_find_audio (config, payload, rtpmap);
      if (local_med != NULL)
        {
          /* found a supported codec? */
          audio_tab[num] = local_med;
          num++;
        }

      /* search for support of this codec in local media list */
      pos++;
    }

  audio_tab[num] = NULL;
  return 0;
}

/**
 * Compare remote sdp packet against local supported media for video.
 * @param config The element to work on.
 * @param remote_sdp The remote SDP packet.
 * @param remote_med The remote Media SDP line.
 * @param video_tab The local list of media supported by both side.
 */
int
osip_rfc3264_match_video (struct osip_rfc3264 *config,
                          sdp_message_t * remote_sdp,
                          sdp_media_t * remote_med, sdp_media_t * video_tab[])
{
  osip_rfc3264_t *cnf = (osip_rfc3264_t *) config;
  int num = 0;
  int pos;

  video_tab[0] = NULL;

  if (cnf == NULL)
    return -1;

  /* search for the video media line */

  pos = 0;
  while (!osip_list_eol (remote_med->m_payloads, pos))
    {
      char *payload = (char *) osip_list_get (remote_med->m_payloads, pos);
      sdp_media_t *local_med;
      char *rtpmap = NULL;
      int posattr = 0;

      /* search for the rtpmap associated to the payload */
      while (!osip_list_eol (remote_med->a_attributes, posattr))
        {
          sdp_attribute_t *attr =
            (sdp_attribute_t *) osip_list_get (remote_med->a_attributes,
                                               posattr);

          if (0 == osip_strncasecmp (attr->a_att_field, "rtpmap", 6))
            {
              if (attr->a_att_value != NULL &&
                  0 == osip_strncasecmp (attr->a_att_value, payload,
                                         strlen (payload)))
                {
                  /* TODO check if it was not like 101: == 10 */
                  rtpmap = attr->a_att_value;
                  break;
                }
            }
          posattr++;
        }

      local_med = osip_rfc3264_find_video (config, payload, rtpmap);
      if (local_med != NULL)
        {
          /* found a supported codec? */
          video_tab[num] = local_med;
          num++;
        }

      /* search for support of this codec in local media list */
      pos++;
    }

  video_tab[num] = NULL;
  return 0;
}

/**
 * Compare remote sdp packet against local supported media for t38.
 * @param config The element to work on.
 * @param remote_sdp The remote SDP packet.
 * @param remote_med The remote Media SDP line.
 * @param t38_tab The local list of media supported by both side.
 */
int
osip_rfc3264_match_t38 (struct osip_rfc3264 *config,
                        sdp_message_t * remote_sdp,
                        sdp_media_t * remote_med, sdp_media_t * t38_tab[])
{
  osip_rfc3264_t *cnf = (osip_rfc3264_t *) config;

  t38_tab[0] = NULL;

  if (cnf == NULL)
    return -1;

  return 0;
}

/**
 * Compare remote sdp packet against local supported media for application.
 * @param config The element to work on.
 * @param remote_sdp The remote SDP packet.
 * @param remote_med The remote Media SDP line.
 * @param app_tab The local list of media supported by both side.
 */
int
osip_rfc3264_match_app (struct osip_rfc3264 *config,
                        sdp_message_t * remote_sdp,
                        sdp_media_t * remote_med, sdp_media_t * app_tab[])
{
  osip_rfc3264_t *cnf = (osip_rfc3264_t *) config;

  app_tab[0] = NULL;

  if (cnf == NULL)
    return -1;

  return 0;
}


/**
 * Prepare an uncomplete answer.
 * @param config The element to work on.
 * @param remote_sdp The remote SDP packet.
 * @param local_sdp The local SDP packet to prepare.
 * @param length The local SDP packet's length.
 */
int
osip_rfc3264_prepare_answer (struct osip_rfc3264 *config,
                             sdp_message_t * remote_sdp,
                             char *local_sdp, int length)
{
  int pos, pos2;

  if (config == NULL)
    return -1;
  if (remote_sdp == NULL)
    return -1;

  if (osip_list_size (remote_sdp->t_descrs) > 0)

#if !defined __PALMOS__ && (defined WIN32 || defined _WIN32_WCE)
    _snprintf (local_sdp, 4096, "v=0\r\n\
o=userX 20000001 20000001 IN IP4 TOREPLACE\r\n\
s=-\r\n\
c=IN IP4 TOREPLACE\r\n");
#else
    snprintf (local_sdp, 4096, "v=0\r\n\
o=userX 20000001 20000001 IN IP4 TOREPLACE\r\n\
s=-\r\n\
c=IN IP4 TOREPLACE\r\n");
#endif
  /* Fill t= (and r=) fields */
  pos = 0;
  while (!osip_list_eol (remote_sdp->t_descrs, pos))
    {
      char tmp[100];
      sdp_time_descr_t *td;

      td = (sdp_time_descr_t *) osip_list_get (remote_sdp->t_descrs, pos);
      if (td->t_start_time != NULL && td->t_stop_time != NULL)
#if !defined __PALMOS__ && (defined WIN32 || defined _WIN32_WCE)
        _snprintf (tmp, 100, "t=%s %s\r\n", td->t_start_time, td->t_stop_time);
#else
        snprintf (tmp, 100, "t=%s %s\r\n", td->t_start_time, td->t_stop_time);
#endif
      else
#if !defined __PALMOS__ && (defined WIN32 || defined _WIN32_WCE)
        _snprintf (tmp, 100, "t=0 0\r\n");
#else
        snprintf (tmp, 100, "t=0 0\r\n");
#endif
      if ((int) (strlen (local_sdp) + strlen (tmp) + 1) < length)

        {
          strcat (local_sdp, tmp);

          pos2 = 0;
          while (!osip_list_eol (td->r_repeats, pos2))
            {
              char *str = (char *) osip_list_get (td->r_repeats, pos2);

              if ((int) (strlen (local_sdp) + strlen (str) + 5 + 1) < length)
                {
                  strcat (local_sdp, "r=");
                  strcat (local_sdp, str);
                  strcat (local_sdp, "\r\n");
              } else
                return -1;
              pos2++;
            }
      } else
        return -1;
      pos++;
    }


  pos = 0;
  while (!osip_list_eol (remote_sdp->m_medias, pos))
    {
      int posattr = 0;
      char tmp[200];
      char tmp2[200];
      char inactive = 'X';
      sdp_media_t *med;

#if !defined __PALMOS__ && (defined WIN32 || defined _WIN32_WCE)
      _snprintf (tmp2, 199, "\r\n");
#else
      snprintf (tmp2, 199, "\r\n");
#endif
      med = (sdp_media_t *) osip_list_get (remote_sdp->m_medias, pos);

      /* search for the rtpmap associated to the payload */
      while (!osip_list_eol (med->a_attributes, posattr))
        {
          sdp_attribute_t *attr =
            (sdp_attribute_t *) osip_list_get (med->a_attributes, posattr);
          if (strlen (attr->a_att_field) == 8 && attr->a_att_value == NULL)
            {
              if (0 == osip_strncasecmp (attr->a_att_field, "sendonly", 8))
                {
#if !defined __PALMOS__ && (defined WIN32 || defined _WIN32_WCE)
                  _snprintf (tmp2, 199, "\r\na=recvonly\r\n");
#else
                  snprintf (tmp2, 199, "\r\na=recvonly\r\n");
#endif
                  break;
              } else if (0 == osip_strncasecmp (attr->a_att_field, "recvonly", 8))
                {
#if !defined __PALMOS__ && (defined WIN32 || defined _WIN32_WCE)
                  _snprintf (tmp2, 199, "\r\na=sendonly\r\n");
#else
                  snprintf (tmp2, 199, "\r\na=sendonly\r\n");
#endif
                  break;
              } else if (0 == osip_strncasecmp (attr->a_att_field, "sendrecv", 8))
                {
                  break;
              } else if (0 == osip_strncasecmp (attr->a_att_field, "inactive", 8))
                {
#if !defined __PALMOS__ && (defined WIN32 || defined _WIN32_WCE)
                  _snprintf (tmp2, 199, "\r\na=inactive\r\n");
#else
                  snprintf (tmp2, 199, "\r\na=inactive\r\n");
#endif
                  inactive = '0';
                  break;
                }
            }
          posattr++;
        }

      if (med->m_media != NULL && med->m_proto != NULL
          && med->m_number_of_port == NULL)
        {
#if !defined __PALMOS__ && (defined WIN32 || defined _WIN32_WCE)
          _snprintf (tmp, 199, "m=%s %c %s ", med->m_media, inactive,
                     med->m_proto);
#else
          snprintf (tmp, 199, "m=%s %c %s ", med->m_media, inactive, med->m_proto);
#endif
      } else if (med->m_media != NULL && med->m_proto != NULL
                 && med->m_number_of_port != NULL)
        {
#if !defined __PALMOS__ && (defined WIN32 || defined _WIN32_WCE)
          _snprintf (tmp, 199, "m=%s %c %s/%s ", med->m_media, inactive,
                     med->m_proto, med->m_number_of_port);
#else
          snprintf (tmp, 199, "m=%s %c %s/%s ", med->m_media, inactive,
                    med->m_proto, med->m_number_of_port);
#endif
      } else
        return -1;

      if ((int) (strlen (local_sdp) + strlen (tmp) + 1) < length)
        strcat (local_sdp, tmp);
      else
        return -1;

      if ((int) (strlen (local_sdp) + strlen (tmp2) + 1) < length)
        strcat (local_sdp, tmp2);
      else
        return -1;

      pos++;
    }

  return 0;
}

/**
 * Agree to support a specific codec.
 *   This method should be called for each codec returned by
 *   osip_rfc3264_match(...) that the calle agree to support.
 *
 * @param config The element to work on.
 * @param remote_sdp The remote SDP packet.
 * @param local_sdp The local SDP packet to complete.
 * @param med One of the media returned by osip_rfc3264_match.
 * @param mline The position of the media line to complete.
 */
int
osip_rfc3264_complete_answer (struct osip_rfc3264 *config,
                              sdp_message_t * remote_sdp,
                              sdp_message_t * local_sdp,
                              sdp_media_t * med, int mline)
{
  osip_rfc3264_t *cnf = (osip_rfc3264_t *) config;
  sdp_media_t *remote_med = NULL;
  sdp_media_t *local_med = NULL;
  int pos;

  if (cnf == NULL)
    return -1;
  if (remote_sdp == NULL)
    return -1;
  if (med == NULL)
    return -1;
  if (mline < 0)
    return -1;
  if (local_sdp == NULL)
    return -1;
  pos = 0;
  while (!osip_list_eol (remote_sdp->m_medias, pos))
    {
      remote_med = (sdp_media_t *) osip_list_get (remote_sdp->m_medias, pos);
      local_med = (sdp_media_t *) osip_list_get (local_sdp->m_medias, pos);
      if (pos == mline)
        break;
      remote_med = NULL;
      local_med = NULL;
      pos++;
    }
  if (remote_med == NULL)
    return -1;

  pos = 0;
  while (!osip_list_eol (med->a_attributes, pos))
    {
      sdp_attribute_t *attr =
        (sdp_attribute_t *) osip_list_get (med->a_attributes, pos);

      if (0 == osip_strcasecmp ("rtpmap", attr->a_att_field)
          && attr->a_att_value != NULL)
        {
          sdp_attribute_t *mattr;
          char *tmp;

          /* fill the m= line */
          tmp = (char *) osip_list_get (med->m_payloads, 0);
          if (tmp != NULL)
            osip_list_add (local_med->m_payloads, osip_strdup (tmp), -1);
          else
            return -1;

          sdp_attribute_init (&mattr);
          mattr->a_att_field = osip_strdup (attr->a_att_field);
          mattr->a_att_value = osip_strdup (attr->a_att_value);

          /* fill the a= line */
          osip_list_add (local_med->a_attributes, mattr, -1);
          return 0;
        }
    }

  return -1;                    /* no rtpmap found? It is mandatory in audio and video media */
}

/**
 * Agree to support a specific codec.
 *   This method should be called for each codec returned by
 *   osip_rfc3264_match(...)
 *
 * @param config The element to work on.
 * @param med One of the media returned by osip_rfc3264_match
 * @param remote_sdp The remote SDP packet.
 * @param local_sdp The local SDP packet to prepare.
 */
int
osip_rfc3264_accept_codec (struct osip_rfc3264 *config,
                           sdp_media_t * med,
                           sdp_message_t * remote_sdp, sdp_message_t * local_sdp)
{
  if (config == NULL)
    return -1;

  return 0;
}


/* #ifdef RFC3264_DEBUG */

/**
 * List supported codecs. (for debugging purpose only)
 *
 * @param config The element to work on.
 */
int
__osip_rfc3264_print_codecs (struct osip_rfc3264 *config)
{
  osip_rfc3264_t *cnf = (osip_rfc3264_t *) config;
  int i, pos;

  if (config == NULL)
    return -1;

  fprintf (stdout, "Audio codecs Supported:\n");
  for (i = 0; i < MAX_AUDIO_CODECS; i++)
    {
      if (cnf->audio_medias[i] != NULL)
        {
          sdp_media_t *med = cnf->audio_medias[i];
          char *str = (char *) osip_list_get (med->m_payloads, 0);

          fprintf (stdout, "\tm=%s %s %s %s\n",
                   med->m_media, med->m_port, med->m_proto, str);
          pos = 0;
          while (!osip_list_eol (med->a_attributes, pos))
            {
              sdp_attribute_t *attr =
                (sdp_attribute_t *) osip_list_get (med->a_attributes, pos);
              fprintf (stdout, "\ta=%s:%s\n",
                       attr->a_att_field, attr->a_att_value);
              pos++;
            }
          fprintf (stdout, "\n");
        }
    }

  fprintf (stdout, "Video codecs Supported:\n");
  for (i = 0; i < MAX_VIDEO_CODECS; i++)
    {
      if (cnf->video_medias[i] != NULL)
        {
          sdp_media_t *med = cnf->video_medias[i];
          char *str = (char *) osip_list_get (med->m_payloads, 0);

          fprintf (stdout, "\tm=%s %s %s %s\n",
                   med->m_media, med->m_port, med->m_proto, str);
          pos = 0;
          while (!osip_list_eol (med->a_attributes, pos))
            {
              sdp_attribute_t *attr =
                (sdp_attribute_t *) osip_list_get (med->a_attributes, pos);
              fprintf (stdout, "\ta=%s:%s\n",
                       attr->a_att_field, attr->a_att_value);
              pos++;
            }
          fprintf (stdout, "\n");
        }
    }

  fprintf (stdout, "t38 configs Supported:\n");
  for (i = 0; i < MAX_T38_CODECS; i++)
    {
      if (cnf->t38_medias[i] != NULL)
        {
          sdp_media_t *med = cnf->t38_medias[i];

          fprintf (stdout, "m=%s %s %s X\n",
                   med->m_media, med->m_port, med->m_proto);
          pos = 0;
          while (!osip_list_eol (med->a_attributes, pos))
            {
              sdp_attribute_t *attr =
                (sdp_attribute_t *) osip_list_get (med->a_attributes, pos);
              fprintf (stdout, "\ta=%s:%s\n",
                       attr->a_att_field, attr->a_att_value);
              pos++;
            }
          fprintf (stdout, "\n");
        }
    }

  fprintf (stdout, "Application config Supported:\n");
  for (i = 0; i < MAX_APP_CODECS; i++)
    {
      if (cnf->app_medias[i] != NULL)
        {
          sdp_media_t *med = cnf->app_medias[i];

          fprintf (stdout, "m=%s %s %s X\n",
                   med->m_media, med->m_port, med->m_proto);
          pos = 0;
          while (!osip_list_eol (med->a_attributes, pos))
            {
              sdp_attribute_t *attr =
                (sdp_attribute_t *) osip_list_get (med->a_attributes, pos);
              fprintf (stdout, "\ta=%s:%s\n",
                       attr->a_att_field, attr->a_att_value);
              pos++;
            }
          fprintf (stdout, "\n");
        }
    }

  return 0;
}

/* #endif */
