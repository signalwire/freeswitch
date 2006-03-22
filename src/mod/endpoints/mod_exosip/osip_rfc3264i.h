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

#ifndef __SDP_RFC3264I_H__
#define __SDP_RFC3264I_H__

#include <osipparser2/osip_list.h>
#include <osipparser2/sdp_message.h>
#include <osipparser2/osip_rfc3264.h>

#ifndef DOXYGEN

typedef struct osip_rfc3264 osip_rfc3264_t;

struct osip_rfc3264
{
  sdp_media_t *audio_medias[MAX_AUDIO_CODECS];
  sdp_media_t *video_medias[MAX_VIDEO_CODECS];
  sdp_media_t *t38_medias[MAX_T38_CODECS];
  sdp_media_t *app_medias[MAX_APP_CODECS];
};

#endif

#endif
