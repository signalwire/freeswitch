//indent -gnu -ts4 -br -brs -cdw -lp -ce -nbfda -npcs -nprs -npsl -nbbo -saf -sai -saw -cs -bbo -nhnl -nut -sob -l90 
#include "celliax.h"
#include "iconv.h"

extern int celliax_debug;
extern char *celliax_console_active;
extern char celliax_type[];
extern struct celliax_pvt *celliax_iflist;
extern int celliax_dir_entry_extension;

#ifndef GIOVA48
#define SAMPLES_PER_FRAME 160
#else // GIOVA48
#define SAMPLES_PER_FRAME 960
#endif // GIOVA48

#ifdef CELLIAX_ALSA
/*! \brief ALSA pcm format, according to endianess  */
#if __BYTE_ORDER == __LITTLE_ENDIAN
snd_pcm_format_t celliax_format = SND_PCM_FORMAT_S16_LE;
#else
snd_pcm_format_t celliax_format = SND_PCM_FORMAT_S16_BE;
#endif

/*!
 * \brief Initialize the ALSA soundcard channels (capture AND playback) used by one interface (a multichannel soundcard can be used by multiple interfaces) 
 * \param p the celliax_pvt of the interface
 *
 * This function call alsa_open_dev to initialize the ALSA soundcard for each channel (capture AND playback) used by one interface (a multichannel soundcard can be used by multiple interfaces). Called by sound_init
 *
 * \return zero on success, -1 on error.
 */
int alsa_init(struct celliax_pvt *p)
{
  p->alsac = alsa_open_dev(p, SND_PCM_STREAM_CAPTURE);
  if (!p->alsac) {
    ERRORA("Failed opening ALSA capture device: %s\n", CELLIAX_P_LOG, p->alsacname);
    if (alsa_shutdown(p)) {
      ERRORA("alsa_shutdown failed\n", CELLIAX_P_LOG);
      return -1;
    }
    return -1;
  }
  p->alsap = alsa_open_dev(p, SND_PCM_STREAM_PLAYBACK);
  if (!p->alsap) {
    ERRORA("Failed opening ALSA playback device: %s\n", CELLIAX_P_LOG, p->alsapname);
    if (alsa_shutdown(p)) {
      ERRORA("alsa_shutdown failed\n", CELLIAX_P_LOG);
      return -1;
    }
    return -1;
  }

  /* make valgrind very happy */
  snd_config_update_free_global();
  return 0;
}

/*!
 * \brief Shutdown the ALSA soundcard channels (input and output) used by one interface (a multichannel soundcard can be used by multiple interfaces) 
 * \param p the celliax_pvt of the interface
 *
 * This function shutdown the ALSA soundcard channels (input and output) used by one interface (a multichannel soundcard can be used by multiple interfaces). Called by sound_init
 *
 * \return zero on success, -1 on error.
 */

int alsa_shutdown(struct celliax_pvt *p)
{

  int err;

  if (p->alsap) {
    err = snd_pcm_drop(p->alsap);
    if (err < 0) {
      ERRORA("device [%s], snd_pcm_drop failed with error '%s'\n", CELLIAX_P_LOG,
             p->alsapname, snd_strerror(err));
      return -1;
    }
    err = snd_pcm_close(p->alsap);
    if (err < 0) {
      ERRORA("device [%s], snd_pcm_close failed with error '%s'\n", CELLIAX_P_LOG,
             p->alsapname, snd_strerror(err));
      return -1;
    }
  }
  if (p->alsac) {
    err = snd_pcm_drop(p->alsac);
    if (err < 0) {
      ERRORA("device [%s], snd_pcm_drop failed with error '%s'\n", CELLIAX_P_LOG,
             p->alsacname, snd_strerror(err));
      return -1;
    }
    err = snd_pcm_close(p->alsac);
    if (err < 0) {
      ERRORA("device [%s], snd_pcm_close failed with error '%s'\n", CELLIAX_P_LOG,
             p->alsacname, snd_strerror(err));
      return -1;
    }
  }

  return 0;
}

/*!
 * \brief Setup and open the ALSA device (capture OR playback) 
 * \param p the celliax_pvt of the interface
 * \param stream the ALSA capture/playback definition
 *
 * This function setup and open the ALSA device (capture OR playback). Called by alsa_init
 *
 * \return zero on success, -1 on error.
 */
snd_pcm_t *alsa_open_dev(struct celliax_pvt * p, snd_pcm_stream_t stream)
{

  snd_pcm_t *handle = NULL;
  snd_pcm_hw_params_t *params;
  snd_pcm_sw_params_t *swparams;
  snd_pcm_uframes_t buffer_size;
  int err;
  size_t n;
  //snd_pcm_uframes_t xfer_align;
  unsigned int rate;
  snd_pcm_uframes_t start_threshold, stop_threshold;
  snd_pcm_uframes_t period_size = 0;
  snd_pcm_uframes_t chunk_size = 0;
  int start_delay = 0;
  int stop_delay = 0;
  snd_pcm_state_t state;
  snd_pcm_info_t *info;

  period_size = p->alsa_period_size;

  snd_pcm_hw_params_alloca(&params);
  snd_pcm_sw_params_alloca(&swparams);

  if (stream == SND_PCM_STREAM_CAPTURE) {
    err = snd_pcm_open(&handle, p->alsacname, stream, 0 | SND_PCM_NONBLOCK);
  } else {
    err = snd_pcm_open(&handle, p->alsapname, stream, 0 | SND_PCM_NONBLOCK);
  }
  if (err < 0) {
    ERRORA
      ("snd_pcm_open failed with error '%s' on device '%s', if you are using a plughw:n device please change it to be a default:n device (so to allow it to be shared with other concurrent programs), or maybe you are using an ALSA voicemodem and slmodemd"
       " is running?\n", CELLIAX_P_LOG, snd_strerror(err),
       stream == SND_PCM_STREAM_CAPTURE ? p->alsacname : p->alsapname);
    return NULL;
  }

  snd_pcm_info_alloca(&info);

  if ((err = snd_pcm_info(handle, info)) < 0) {
    ERRORA("info error: %s", CELLIAX_P_LOG, snd_strerror(err));
    return NULL;
  }

  err = snd_pcm_nonblock(handle, 1);
  if (err < 0) {
    ERRORA("nonblock setting error: %s", CELLIAX_P_LOG, snd_strerror(err));
    return NULL;
  }

  err = snd_pcm_hw_params_any(handle, params);
  if (err < 0) {
    ERRORA("Broken configuration for this PCM, no configurations available: %s\n",
           CELLIAX_P_LOG, snd_strerror(err));
    return NULL;
  }

  err = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
  if (err < 0) {
    ERRORA("Access type not available: %s\n", CELLIAX_P_LOG, snd_strerror(err));
    return NULL;
  }
  err = snd_pcm_hw_params_set_format(handle, params, celliax_format);
  if (err < 0) {
    ERRORA("Sample format non available: %s\n", CELLIAX_P_LOG, snd_strerror(err));
    return NULL;
  }
  err = snd_pcm_hw_params_set_channels(handle, params, 1);
  if (err < 0) {
    DEBUGA_SOUND("Channels count set failed: %s\n", CELLIAX_P_LOG, snd_strerror(err));
  }
#if 1
  unsigned int chan_num;
  err = snd_pcm_hw_params_get_channels(params, &chan_num);
  if (err < 0) {
    ERRORA("Channels count non available: %s\n", CELLIAX_P_LOG, snd_strerror(err));
    return NULL;
  }
  if (chan_num < 1 || chan_num > 2) {
    ERRORA("Channels count MUST BE 1 or 2, it is: %d\n", CELLIAX_P_LOG, chan_num);
    ERRORA("Channels count MUST BE 1 or 2, it is: %d on %s %s\n", CELLIAX_P_LOG, chan_num,
           p->alsapname, p->alsacname);
    return NULL;
  } else {
    if (chan_num == 1) {
      if (stream == SND_PCM_STREAM_CAPTURE)
        p->alsa_capture_is_mono = 1;
      else
        p->alsa_play_is_mono = 1;
    } else {
      if (stream == SND_PCM_STREAM_CAPTURE)
        p->alsa_capture_is_mono = 0;
      else
        p->alsa_play_is_mono = 0;
    }
  }
#else
  p->alsa_capture_is_mono = 1;
  p->alsa_play_is_mono = 1;
#endif

#if 0
  unsigned int buffer_time = 0;
  unsigned int period_time = 0;
  snd_pcm_uframes_t period_frames = 0;
  snd_pcm_uframes_t buffer_frames = 0;

  if (buffer_time == 0 && buffer_frames == 0) {
    err = snd_pcm_hw_params_get_buffer_time_max(params, &buffer_time, 0);
    assert(err >= 0);
    if (buffer_time > 500000)
      buffer_time = 500000;
  }
  if (period_time == 0 && period_frames == 0) {
    if (buffer_time > 0)
      period_time = buffer_time / 4;
    else
      period_frames = buffer_frames / 4;
  }
  if (period_time > 0)
    err = snd_pcm_hw_params_set_period_time_near(handle, params, &period_time, 0);
  else
    err = snd_pcm_hw_params_set_period_size_near(handle, params, &period_frames, 0);
  assert(err >= 0);
  if (buffer_time > 0) {
    err = snd_pcm_hw_params_set_buffer_time_near(handle, params, &buffer_time, 0);
  } else {
    err = snd_pcm_hw_params_set_buffer_size_near(handle, params, &buffer_frames);
  }
#endif

#if 1
  rate = p->celliax_sound_rate;
  err = snd_pcm_hw_params_set_rate_near(handle, params, &rate, 0);
  if ((float) p->celliax_sound_rate * 1.05 < rate
      || (float) p->celliax_sound_rate * 0.95 > rate) {
    WARNINGA("Rate is not accurate (requested = %iHz, got = %iHz)\n", CELLIAX_P_LOG,
             p->celliax_sound_rate, rate);
  }

  if (err < 0) {
    ERRORA("Error setting rate: %s\n", CELLIAX_P_LOG, snd_strerror(err));
    return NULL;
  }
  p->celliax_sound_rate = rate;

  err = snd_pcm_hw_params_set_period_size_near(handle, params, &period_size, 0);

  if (err < 0) {
    ERRORA("Error setting period_size: %s\n", CELLIAX_P_LOG, snd_strerror(err));
    return NULL;
  }

  p->alsa_period_size = period_size;

  p->alsa_buffer_size = p->alsa_period_size * p->alsa_periods_in_buffer;

  err = snd_pcm_hw_params_set_buffer_size_near(handle, params, &p->alsa_buffer_size);

  if (err < 0) {
    ERRORA("Error setting buffer_size: %s\n", CELLIAX_P_LOG, snd_strerror(err));
    return NULL;
  }
#endif

  err = snd_pcm_hw_params(handle, params);
  if (err < 0) {
    ERRORA("Unable to install hw params: %s\n", CELLIAX_P_LOG, snd_strerror(err));
    return NULL;
  }

  snd_pcm_hw_params_get_period_size(params, &chunk_size, 0);
  snd_pcm_hw_params_get_buffer_size(params, &buffer_size);
  if (chunk_size == buffer_size) {
    ERRORA("Can't use period equal to buffer size (%lu == %lu)\n", CELLIAX_P_LOG,
           chunk_size, buffer_size);
    return NULL;
  }

  snd_pcm_sw_params_current(handle, swparams);

#if 0
  err = snd_pcm_sw_params_get_xfer_align(swparams, &xfer_align);
  if (err < 0) {
    ERRORA("Unable to obtain xfer align: %s\n", CELLIAX_P_LOG, snd_strerror(err));
  }
  NOTICA("xfer_align: %d\n", CELLIAX_P_LOG, xfer_align);
  /* for some reason, on some platforms, xfer_align here is zero, that gives a floating point exception later. So, let's try to force it to 160, the frame size used by celliax */
  xfer_align = p->alsa_period_size;
  NOTICA("xfer_align: %d\n", CELLIAX_P_LOG, xfer_align);

  err = snd_pcm_sw_params_set_xfer_align(handle, swparams, xfer_align);
  if (err < 0) {
    ERRORA("Error setting xfer_align: %s\n", CELLIAX_P_LOG, snd_strerror(err));
  }
  NOTICA("xfer_align: %d\n", CELLIAX_P_LOG, xfer_align);

  err = snd_pcm_sw_params_get_xfer_align(swparams, &xfer_align);
  if (err < 0) {
    ERRORA("Unable to obtain xfer align: %s\n", CELLIAX_P_LOG, snd_strerror(err));
  }
  NOTICA("xfer_align: %d\n", CELLIAX_P_LOG, xfer_align);
#endif

  /*
     if (sleep_min)
     xfer_align = 1;
     err = snd_pcm_sw_params_set_sleep_min(handle, swparams,
     0);

     if (err < 0) {
     ERRORA("Error setting slep_min: %s\n", CELLIAX_P_LOG, snd_strerror(err));
     }
   */
  n = chunk_size;
  err = snd_pcm_sw_params_set_avail_min(handle, swparams, n);
  if (err < 0) {
    ERRORA("Error setting avail_min: %s\n", CELLIAX_P_LOG, snd_strerror(err));
  }
#if 0
  /* round up to closest transfer boundary */
  if (xfer_align == 0) {        //so to avoid floating point exception ????
    xfer_align = 160;
  }
  //original n = (buffer_size / xfer_align) * xfer_align;
  n = (chunk_size / xfer_align) * xfer_align;
#endif
  if (stream == SND_PCM_STREAM_CAPTURE) {
    start_delay = 1;
  }
  if (start_delay <= 0) {
    start_threshold = n + (double) rate *start_delay / 1000000;
  } else {
    start_threshold = (double) rate *start_delay / 1000000;
  }
  if (start_threshold < 1)
    start_threshold = 1;
  if (start_threshold > n)
    start_threshold = n;
  err = snd_pcm_sw_params_set_start_threshold(handle, swparams, start_threshold);
  if (err < 0) {
    ERRORA("Error setting start_threshold: %s\n", CELLIAX_P_LOG, snd_strerror(err));
  }

  if (stop_delay <= 0)
    stop_threshold = buffer_size + (double) rate *stop_delay / 1000000;
  else
    stop_threshold = (double) rate *stop_delay / 1000000;

  if (stream == SND_PCM_STREAM_CAPTURE) {
    stop_threshold = -1;
  }

  err = snd_pcm_sw_params_set_stop_threshold(handle, swparams, stop_threshold);

  if (err < 0) {
    ERRORA("Error setting stop_threshold: %s\n", CELLIAX_P_LOG, snd_strerror(err));
  }
#if 0
  err = snd_pcm_sw_params_set_xfer_align(handle, swparams, xfer_align);

  if (err < 0) {
    ERRORA("Error setting xfer_align: %s\n", CELLIAX_P_LOG, snd_strerror(err));
  }
#endif

  if (snd_pcm_sw_params(handle, swparams) < 0) {
    ERRORA("Error installing software parameters: %s\n", CELLIAX_P_LOG,
           snd_strerror(err));
  }

  err = snd_pcm_poll_descriptors_count(handle);
  if (err <= 0) {
    ERRORA("Unable to get a poll descriptors count, error is %s\n", CELLIAX_P_LOG,
           snd_strerror(err));
    return NULL;
  }

  if (err != 1) {               //number of poll descriptors
    DEBUGA_SOUND("Can't handle more than one device\n", CELLIAX_P_LOG);
    return NULL;
  }

  err = snd_pcm_poll_descriptors(handle, &p->pfd, err);
  if (err != 1) {
    ERRORA("snd_pcm_poll_descriptors failed, %s\n", CELLIAX_P_LOG, snd_strerror(err));
    return NULL;
  }
  DEBUGA_SOUND("Acquired fd %d from the poll descriptor\n", CELLIAX_P_LOG, p->pfd.fd);

  if (stream == SND_PCM_STREAM_CAPTURE) {
    p->celliax_sound_capt_fd = p->pfd.fd;
  }

  state = snd_pcm_state(handle);

  if (state != SND_PCM_STATE_RUNNING) {
    if (state != SND_PCM_STATE_PREPARED) {
      err = snd_pcm_prepare(handle);
      if (err) {
        ERRORA("snd_pcm_prepare failed, %s\n", CELLIAX_P_LOG, snd_strerror(err));
        return NULL;
      }
      DEBUGA_SOUND("prepared!\n", CELLIAX_P_LOG);
    }
    if (stream == SND_PCM_STREAM_CAPTURE) {
      err = snd_pcm_start(handle);
      if (err) {
        ERRORA("snd_pcm_start failed, %s\n", CELLIAX_P_LOG, snd_strerror(err));
        return NULL;
      }
      DEBUGA_SOUND("started!\n", CELLIAX_P_LOG);
    }
  }
  if (option_debug > 1) {
    snd_output_t *output = NULL;
    err = snd_output_stdio_attach(&output, stdout, 0);
    if (err < 0) {
      ERRORA("snd_output_stdio_attach failed: %s\n", CELLIAX_P_LOG, snd_strerror(err));
    }
    snd_pcm_dump(handle, output);
  }
  if (option_debug > 1)
    DEBUGA_SOUND("ALSA handle = %ld\n", CELLIAX_P_LOG, (long int) handle);
  return handle;

}

/*! \brief Read audio frames from interface */

struct ast_frame *alsa_read(struct celliax_pvt *p)
{
  static struct ast_frame f;
  static short __buf[CELLIAX_FRAME_SIZE + AST_FRIENDLY_OFFSET / 2];
  static short __buf2[(CELLIAX_FRAME_SIZE + AST_FRIENDLY_OFFSET / 2) * 2];
  short *buf;
  short *buf2;
  static int readpos = 0;
  static int left = CELLIAX_FRAME_SIZE;
  snd_pcm_state_t state;
  int r = 0;
  int off = 0;
  int error = 0;
  //time_t now_timestamp;

  //memset(&f, 0, sizeof(struct ast_frame)); //giova

  f.frametype = AST_FRAME_NULL;
  f.subclass = 0;
  f.samples = 0;
  f.datalen = 0;
  f.data = NULL;
  f.offset = 0;
  f.src = celliax_type;
  f.mallocd = 0;
  f.delivery.tv_sec = 0;
  f.delivery.tv_usec = 0;

  state = snd_pcm_state(p->alsac);
  if (state != SND_PCM_STATE_RUNNING) {
    DEBUGA_SOUND("ALSA read state is not SND_PCM_STATE_RUNNING\n", CELLIAX_P_LOG);

    if (state != SND_PCM_STATE_PREPARED) {
      error = snd_pcm_prepare(p->alsac);
      if (error) {
        ERRORA("snd_pcm_prepare failed, %s\n", CELLIAX_P_LOG, snd_strerror(error));
        return &f;
      }
      DEBUGA_SOUND("prepared!\n", CELLIAX_P_LOG);
    }
    usleep(1000);
    error = snd_pcm_start(p->alsac);
    if (error) {
      ERRORA("snd_pcm_start failed, %s\n", CELLIAX_P_LOG, snd_strerror(error));
      return &f;
    }
    DEBUGA_SOUND("started!\n", CELLIAX_P_LOG);
    usleep(1000);
  }

  buf = __buf + AST_FRIENDLY_OFFSET / 2;
  buf2 = __buf2 + ((AST_FRIENDLY_OFFSET / 2) * 2);

  if (p->alsa_capture_is_mono) {
    r = snd_pcm_readi(p->alsac, buf + readpos, left);
  } else {
    r = snd_pcm_readi(p->alsac, buf2 + (readpos * 2), left);

    int a = 0;
    int i = 0;
    for (i = 0; i < (CELLIAX_FRAME_SIZE + AST_FRIENDLY_OFFSET / 2) * 2;) {
      __buf[a] = (__buf2[i] + __buf2[i + 1]) / 2;   //comment out this line to use only left
      //__buf[a] = __buf2[i]; // enable this line to use only left
      a++;
      i++;
      i++;
    }
  }

  if (r == -EPIPE) {
    ERRORA("XRUN read\n\n\n\n\n", CELLIAX_P_LOG);
    return &f;
  } else if (r == -ESTRPIPE) {
    ERRORA("-ESTRPIPE\n", CELLIAX_P_LOG);
    return &f;

  } else if (r == -EAGAIN) {
    DEBUGA_SOUND("ALSA read -EAGAIN, the soundcard is not ready to be read by celliax\n",
                 CELLIAX_P_LOG);
    while (r == -EAGAIN) {
      usleep(1000);

      if (p->alsa_capture_is_mono) {
        r = snd_pcm_readi(p->alsac, buf + readpos, left);
      } else {
        r = snd_pcm_readi(p->alsac, buf2 + (readpos * 2), left);

        int a = 0;
        int i = 0;
        for (i = 0; i < (CELLIAX_FRAME_SIZE + AST_FRIENDLY_OFFSET / 2) * 2;) {
          __buf[a] = (__buf2[i] + __buf2[i + 1]) / 2;
          a++;
          i++;
          i++;
        }
      }

    }
  } else if (r < 0) {
    WARNINGA("ALSA Read error: %s\n", CELLIAX_P_LOG, snd_strerror(r));
  } else if (r >= 0) {
    //DEBUGA_SOUND("read: r=%d, readpos=%d, left=%d, off=%d\n", CELLIAX_P_LOG, r, readpos, left, off);
    off -= r;                   //what is the meaning of this? a leftover, probably
  }
  /* Update positions */
  readpos += r;
  left -= r;

  if (readpos >= CELLIAX_FRAME_SIZE) {
    /* A real frame */
    readpos = 0;
    left = CELLIAX_FRAME_SIZE;

    f.frametype = AST_FRAME_VOICE;
    f.subclass = AST_FORMAT_SLINEAR;
    f.samples = CELLIAX_FRAME_SIZE;
    f.datalen = CELLIAX_FRAME_SIZE * 2;
    f.data = buf;
    f.offset = AST_FRIENDLY_OFFSET;
    f.src = celliax_type;
    f.mallocd = 0;
#ifdef ALSA_MONITOR
    alsa_monitor_read((char *) buf, CELLIAX_FRAME_SIZE * 2);
#endif

  }
  return &f;
}

/*! \brief Write audio frames to interface */
int alsa_write(struct celliax_pvt *p, struct ast_frame *f)
{
  static char sizbuf[8000];
  static char sizbuf2[16000];
  static char silencebuf[8000];
  static int sizpos = 0;
  int len = sizpos;
  int pos;
  int res = 0;
  time_t now_timestamp;
  /* size_t frames = 0; */
  snd_pcm_state_t state;
  snd_pcm_sframes_t delayp1;
  snd_pcm_sframes_t delayp2;

  /* We have to digest the frame in 160-byte portions */
  if (f->datalen > sizeof(sizbuf) - sizpos) {
    ERRORA("Frame too large\n", CELLIAX_P_LOG);
    res = -1;
  } else {
    memcpy(sizbuf + sizpos, f->data, f->datalen);
    len += f->datalen;
    pos = 0;
#ifdef ALSA_MONITOR
    alsa_monitor_write(sizbuf, len);
#endif
    state = snd_pcm_state(p->alsap);
    if (state == SND_PCM_STATE_XRUN) {
      int i;

      DEBUGA_SOUND
        ("You've got an ALSA write XRUN in the past (celliax can't fill the soundcard buffer fast enough). If this happens often (not after silence or after a pause in the speech, that's OK), and appear to damage the sound quality, first check if you have some IRQ problem, maybe sharing the soundcard IRQ with a broken or heavy loaded ethernet or graphic card. Then consider to increase the alsa_periods_in_buffer (now is set to %d) for this interface in the config file\n",
         CELLIAX_P_LOG, p->alsa_periods_in_buffer);
      res = snd_pcm_prepare(p->alsap);
      if (res) {
        ERRORA("audio play prepare failed: %s\n", CELLIAX_P_LOG, snd_strerror(res));
      } else {
        res = snd_pcm_format_set_silence(celliax_format, silencebuf, len / 2);
        if (res < 0) {
          DEBUGA_SOUND("Silence error %s\n", CELLIAX_P_LOG, snd_strerror(res));
          res = -1;
        }
        for (i = 0; i < (p->alsa_periods_in_buffer - 1); i++) {
          res = snd_pcm_writei(p->alsap, silencebuf, len / 2);
          if (res != len / 2) {
            DEBUGA_SOUND("Write returned a different quantity: %d\n", CELLIAX_P_LOG, res);
            res = -1;
          } else if (res < 0) {
            DEBUGA_SOUND("Write error %s\n", CELLIAX_P_LOG, snd_strerror(res));
            res = -1;
          }
        }
      }

    }

    res = snd_pcm_delay(p->alsap, &delayp1);
    if (res < 0) {
      DEBUGA_SOUND("Error %d on snd_pcm_delay: \"%s\"\n", CELLIAX_P_LOG, res,
                   snd_strerror(res));
      res = snd_pcm_prepare(p->alsap);
      if (res) {
        DEBUGA_SOUND("snd_pcm_prepare failed: '%s'\n", CELLIAX_P_LOG, snd_strerror(res));
      }
      res = snd_pcm_delay(p->alsap, &delayp1);
    }

    delayp2 = snd_pcm_avail_update(p->alsap);
    if (delayp2 < 0) {
      DEBUGA_SOUND("Error %d on snd_pcm_avail_update: \"%s\"\n", CELLIAX_P_LOG,
                   (int) delayp2, snd_strerror(delayp2));

      res = snd_pcm_prepare(p->alsap);
      if (res) {
        DEBUGA_SOUND("snd_pcm_prepare failed: '%s'\n", CELLIAX_P_LOG, snd_strerror(res));
      }
      delayp2 = snd_pcm_avail_update(p->alsap);
    }

    if (                        /* delayp1 != 0 && delayp1 != 160 */
         delayp1 < 160 || delayp2 > p->alsa_buffer_size) {

      res = snd_pcm_prepare(p->alsap);
      if (res) {
        DEBUGA_SOUND
          ("snd_pcm_prepare failed while trying to prevent an ALSA write XRUN: %s, delayp1=%d, delayp2=%d\n",
           CELLIAX_P_LOG, snd_strerror(res), (int) delayp1, (int) delayp2);
      } else {

        int i;
        for (i = 0; i < (p->alsa_periods_in_buffer - 1); i++) {
          res = snd_pcm_format_set_silence(celliax_format, silencebuf, len / 2);
          if (res < 0) {
            DEBUGA_SOUND("Silence error %s\n", CELLIAX_P_LOG, snd_strerror(res));
            res = -1;
          }
          res = snd_pcm_writei(p->alsap, silencebuf, len / 2);
          if (res < 0) {
            DEBUGA_SOUND("Write error %s\n", CELLIAX_P_LOG, snd_strerror(res));
            res = -1;
          } else if (res != len / 2) {
            DEBUGA_SOUND("Write returned a different quantity: %d\n", CELLIAX_P_LOG, res);
            res = -1;
          }
        }

        DEBUGA_SOUND
          ("PREVENTING an ALSA write XRUN (celliax can't fill the soundcard buffer fast enough). If this happens often (not after silence or after a pause in the speech, that's OK), and appear to damage the sound quality, first check if you have some IRQ problem, maybe sharing the soundcard IRQ with a broken or heavy loaded ethernet or graphic card. Then consider to increase the alsa_periods_in_buffer (now is set to %d) for this interface in the config file. delayp1=%d, delayp2=%d\n",
           CELLIAX_P_LOG, p->alsa_periods_in_buffer, (int) delayp1, (int) delayp2);
      }

    }

    memset(sizbuf2, 0, sizeof(sizbuf2));
    if (p->alsa_play_is_mono) {
      res = snd_pcm_writei(p->alsap, sizbuf, len / 2);
    } else {
      int a = 0;
      int i = 0;
      for (i = 0; i < 8000;) {
        sizbuf2[a] = sizbuf[i];
        a++;
        i++;
        sizbuf2[a] = sizbuf[i];
        a++;
        i--;
        sizbuf2[a] = sizbuf[i]; // comment out this line to use only left 
        a++;
        i++;
        sizbuf2[a] = sizbuf[i]; // comment out this line to use only left
        a++;
        i++;
      }
      res = snd_pcm_writei(p->alsap, sizbuf2, len);
    }
    if (res == -EPIPE) {
      DEBUGA_SOUND
        ("ALSA write EPIPE (XRUN) (celliax can't fill the soundcard buffer fast enough). If this happens often (not after silence or after a pause in the speech, that's OK), and appear to damage the sound quality, first check if you have some IRQ problem, maybe sharing the soundcard IRQ with a broken or heavy loaded ethernet or graphic card. Then consider to increase the alsa_periods_in_buffer (now is set to %d) for this interface in the config file. delayp1=%d, delayp2=%d\n",
         CELLIAX_P_LOG, p->alsa_periods_in_buffer, (int) delayp1, (int) delayp2);
      res = snd_pcm_prepare(p->alsap);
      if (res) {
        ERRORA("audio play prepare failed: %s\n", CELLIAX_P_LOG, snd_strerror(res));
      } else {

        if (p->alsa_play_is_mono) {
          res = snd_pcm_writei(p->alsap, sizbuf, len / 2);
        } else {
          int a = 0;
          int i = 0;
          for (i = 0; i < 8000;) {
            sizbuf2[a] = sizbuf[i];
            a++;
            i++;
            sizbuf2[a] = sizbuf[i];
            a++;
            i--;
            sizbuf2[a] = sizbuf[i];
            a++;
            i++;
            sizbuf2[a] = sizbuf[i];
            a++;
            i++;
          }
          res = snd_pcm_writei(p->alsap, sizbuf2, len);
        }

      }

    } else {
      if (res == -ESTRPIPE) {
        ERRORA("You've got some big problems\n", CELLIAX_P_LOG);
      } else if (res == -EAGAIN) {
        res = 0;
      } else if (res < 0) {
        ERRORA("Error %d on audio write: \"%s\"\n", CELLIAX_P_LOG, res,
               snd_strerror(res));
      }
    }
  }

  if (p->audio_play_reset_period) {
    time(&now_timestamp);
    if ((now_timestamp - p->audio_play_reset_timestamp) > p->audio_play_reset_period) {
      if (option_debug)
        DEBUGA_SOUND("reset audio play\n", CELLIAX_P_LOG);
      res = snd_pcm_wait(p->alsap, 1000);
      if (res < 0) {
        ERRORA("audio play wait failed: %s\n", CELLIAX_P_LOG, snd_strerror(res));
      }
      res = snd_pcm_drop(p->alsap);
      if (res) {
        ERRORA("audio play drop failed: %s\n", CELLIAX_P_LOG, snd_strerror(res));
      }
      res = snd_pcm_prepare(p->alsap);
      if (res) {
        ERRORA("audio play prepare failed: %s\n", CELLIAX_P_LOG, snd_strerror(res));
      }
      res = snd_pcm_wait(p->alsap, 1000);
      if (res < 0) {
        ERRORA("audio play wait failed: %s\n", CELLIAX_P_LOG, snd_strerror(res));
      }
      time(&p->audio_play_reset_timestamp);
    }
  }
  res = 0;
  if (res > 0)
    res = 0;
  return res;
}


/*! \brief Write audio frames to interface */
#endif /* CELLIAX_ALSA */

#ifdef CELLIAX_PORTAUDIO
int celliax_portaudio_devlist(struct celliax_pvt *p)
{
  int i, numDevices;
  const PaDeviceInfo *deviceInfo;

  numDevices = Pa_GetDeviceCount();
  if (numDevices < 0) {
    return 0;
  }
  for (i = 0; i < numDevices; i++) {
    deviceInfo = Pa_GetDeviceInfo(i);
    NOTICA
      ("Found PORTAUDIO device: id=%d\tname=%s\tmax input channels=%d\tmax output channels=%d\n",
       CELLIAX_P_LOG, i, deviceInfo->name, deviceInfo->maxInputChannels,
       deviceInfo->maxOutputChannels);
  }

  return numDevices;
}

int celliax_portaudio_init(struct celliax_pvt *p)
{
  PaError err;
  int c;
  PaStreamParameters inputParameters, outputParameters;
  int numdevices;
  const PaDeviceInfo *deviceInfo;

#ifndef GIOVA48
  setenv("PA_ALSA_PLUGHW", "1", 1);
#endif // GIOVA48

  err = Pa_Initialize();
  if (err != paNoError)
    return err;

  numdevices = celliax_portaudio_devlist(p);

  if (p->portaudiocindex > (numdevices - 1)) {
    ERRORA("Portaudio Capture id=%d is out of range: valid id are from 0 to %d\n",
           CELLIAX_P_LOG, p->portaudiocindex, (numdevices - 1));
    return -1;
  }

  if (p->portaudiopindex > (numdevices - 1)) {
    ERRORA("Portaudio Playback id=%d is out of range: valid id are from 0 to %d\n",
           CELLIAX_P_LOG, p->portaudiopindex, (numdevices - 1));
    return -1;
  }
  //inputParameters.device = 0;
  if (p->portaudiocindex != -1) {
    inputParameters.device = p->portaudiocindex;
  } else {
    inputParameters.device = Pa_GetDefaultInputDevice();
  }
  deviceInfo = Pa_GetDeviceInfo(inputParameters.device);
  NOTICA
    ("Using INPUT PORTAUDIO device: id=%d\tname=%s\tmax input channels=%d\tmax output channels=%d\n",
     CELLIAX_P_LOG, inputParameters.device, deviceInfo->name,
     deviceInfo->maxInputChannels, deviceInfo->maxOutputChannels);
  if (deviceInfo->maxInputChannels == 0) {
    ERRORA
      ("No INPUT channels on device: id=%d\tname=%s\tmax input channels=%d\tmax output channels=%d\n",
       CELLIAX_P_LOG, inputParameters.device, deviceInfo->name,
       deviceInfo->maxInputChannels, deviceInfo->maxOutputChannels);
    return -1;
  }
  inputParameters.channelCount = 1;
  inputParameters.sampleFormat = paInt16;
  //inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultHighInputLatency;
  inputParameters.suggestedLatency = 0.1;
  inputParameters.hostApiSpecificStreamInfo = NULL;

  //outputParameters.device = 3;
  if (p->portaudiopindex != -1) {
    outputParameters.device = p->portaudiopindex;
  } else {
    outputParameters.device = Pa_GetDefaultOutputDevice();
  }
  deviceInfo = Pa_GetDeviceInfo(outputParameters.device);
  NOTICA
    ("Using OUTPUT PORTAUDIO device: id=%d\tname=%s\tmax input channels=%d\tmax output channels=%d\n",
     CELLIAX_P_LOG, outputParameters.device, deviceInfo->name,
     deviceInfo->maxInputChannels, deviceInfo->maxOutputChannels);
  if (deviceInfo->maxOutputChannels == 0) {
    ERRORA
      ("No OUTPUT channels on device: id=%d\tname=%s\tmax input channels=%d\tmax output channels=%d\n",
       CELLIAX_P_LOG, inputParameters.device, deviceInfo->name,
       deviceInfo->maxInputChannels, deviceInfo->maxOutputChannels);
    return -1;
  }
#ifndef GIOVA48
  outputParameters.channelCount = 1;
#else // GIOVA48
  outputParameters.channelCount = 2;
#endif // GIOVA48
  outputParameters.sampleFormat = paInt16;
  //outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultHighOutputLatency;
  outputParameters.suggestedLatency = 0.1;
  outputParameters.hostApiSpecificStreamInfo = NULL;

/* build the pipe that will be polled on by pbx */
  c = pipe(p->audiopipe);
  if (c) {
    ERRORA("Unable to create audio pipe\n", CELLIAX_P_LOG);
    return -1;
  }
  fcntl(p->audiopipe[0], F_SETFL, O_NONBLOCK);
  fcntl(p->audiopipe[1], F_SETFL, O_NONBLOCK);

  err =
#ifndef GIOVA48
    OpenAudioStream(&p->stream, &inputParameters, &outputParameters, 8000,
                    paDitherOff | paClipOff, SAMPLES_PER_FRAME, p->audiopipe[1],
                    &p->speexecho, &p->speexpreprocess, &p->owner);

#else // GIOVA48
    OpenAudioStream(&p->stream, &inputParameters, &outputParameters, 48000,
                    paDitherOff | paClipOff, SAMPLES_PER_FRAME, p->audiopipe[1],
                    &p->speexecho, &p->speexpreprocess, &p->owner);

#endif // GIOVA48
  if (err != paNoError) {
    ERRORA("Unable to open audio stream: %s\n", CELLIAX_P_LOG, Pa_GetErrorText(err));
    return -1;
  }

/* the pipe is our audio fd for pbx to poll on */
  p->celliax_sound_capt_fd = p->audiopipe[0];

  return 0;
}

int celliax_portaudio_write(struct celliax_pvt *p, struct ast_frame *f)
{
  int samples;
#ifdef GIOVA48
  //short buf[CELLIAX_FRAME_SIZE * 2];
  short buf[3840];
  short *buf2;

  //ERRORA("1 f->datalen=: %d\n", CELLIAX_P_LOG, f->datalen);

  memset(buf, '\0', CELLIAX_FRAME_SIZE * 2);

  buf2 = f->data;

  int i = 0, a = 0;

  for (i = 0; i < f->datalen / sizeof(short); i++) {
//stereo, 2 chan 48 -> mono 8
    buf[a] = buf2[i];
    a++;
    buf[a] = buf2[i];
    a++;
    buf[a] = buf2[i];
    a++;
    buf[a] = buf2[i];
    a++;
    buf[a] = buf2[i];
    a++;
    buf[a] = buf2[i];
    a++;
    buf[a] = buf2[i];
    a++;
    buf[a] = buf2[i];
    a++;
    buf[a] = buf2[i];
    a++;
    buf[a] = buf2[i];
    a++;
    buf[a] = buf2[i];
    a++;
    buf[a] = buf2[i];
    a++;
    /*
     */
  }
  f->data = &buf;
  f->datalen = f->datalen * 6;
  //ERRORA("2 f->datalen=: %d\n", CELLIAX_P_LOG, f->datalen);
  //f->datalen = f->datalen;
#endif // GIOVA48

#ifdef ASTERISK_VERSION_1_6_0_1
  samples =
    WriteAudioStream(p->stream, (short *) f->data.ptr,
                     (int) (f->datalen / sizeof(short)));
#else
  samples =
    WriteAudioStream(p->stream, (short *) f->data, (int) (f->datalen / sizeof(short)));
#endif /* ASTERISK_VERSION_1_6_0_1 */

  if (samples != (int) (f->datalen / sizeof(short)))
    ERRORA("WriteAudioStream wrote: %d of %d\n", CELLIAX_P_LOG, samples,
           (int) (f->datalen / sizeof(short)));

  return 0;
}

struct ast_frame *celliax_portaudio_read(struct celliax_pvt *p)
{
  static struct ast_frame f;
  static short __buf[CELLIAX_FRAME_SIZE + AST_FRIENDLY_OFFSET / 2];
  short *buf;
  static short __buf2[CELLIAX_FRAME_SIZE + AST_FRIENDLY_OFFSET / 2];
  short *buf2;
  int samples;
  char c;

  memset(__buf, '\0', (CELLIAX_FRAME_SIZE + AST_FRIENDLY_OFFSET / 2));

  buf = __buf + AST_FRIENDLY_OFFSET / 2;

  memset(__buf2, '\0', (CELLIAX_FRAME_SIZE + AST_FRIENDLY_OFFSET / 2));

  buf2 = __buf2 + AST_FRIENDLY_OFFSET / 2;

  f.frametype = AST_FRAME_NULL;
  f.subclass = 0;
  f.samples = 0;
  f.datalen = 0;

#ifdef ASTERISK_VERSION_1_6_0_1
  f.data.ptr = NULL;
#else
  f.data = NULL;
#endif /* ASTERISK_VERSION_1_6_0_1 */
  f.offset = 0;
  f.src = celliax_type;
  f.mallocd = 0;
  f.delivery.tv_sec = 0;
  f.delivery.tv_usec = 0;

  if ((samples = ReadAudioStream(p->stream, buf, SAMPLES_PER_FRAME)) == 0) {
    //do nothing
  } else {
#ifdef GIOVA48
    int i = 0, a = 0;

    samples = samples / 6;
    for (i = 0; i < samples; i++) {
      buf2[i] = buf[a];
      a = a + 6;                //mono, 1 chan 48 -> 8
    }
    buf = buf2;

    /* A real frame */
    f.frametype = AST_FRAME_VOICE;
    f.subclass = AST_FORMAT_SLINEAR;
    f.samples = CELLIAX_FRAME_SIZE / 6;
    f.datalen = CELLIAX_FRAME_SIZE * 2 / 6;
#else // GIOVA48
    /* A real frame */
    f.frametype = AST_FRAME_VOICE;
    f.subclass = AST_FORMAT_SLINEAR;
    f.samples = CELLIAX_FRAME_SIZE;
    f.datalen = CELLIAX_FRAME_SIZE * 2;
#endif // GIOVA48

#ifdef ASTERISK_VERSION_1_6_0_1
    f.data.ptr = buf;
#else
    f.data = buf;
#endif /* ASTERISK_VERSION_1_6_0_1 */
    f.offset = AST_FRIENDLY_OFFSET;
    f.src = celliax_type;
    f.mallocd = 0;
  }

  read(p->audiopipe[0], &c, 1);

  return &f;
}

int celliax_portaudio_shutdown(struct celliax_pvt *p)
{
  PaError err;

  err = CloseAudioStream(p->stream);

  if (err != paNoError)
    ERRORA("not able to CloseAudioStream\n", CELLIAX_P_LOG);

  Pa_Terminate();
  return 0;
}
#endif // CELLIAX_PORTAUDIO

int celliax_serial_sync_AT(struct celliax_pvt *p)
{
  usleep(10000);                /* 10msec */
  time(&p->celliax_serial_synced_timestamp);
  return 0;
}

int celliax_serial_getstatus_AT(struct celliax_pvt *p)
{
  int res;

  if (p->owner) {
    if (p->owner->_state != AST_STATE_UP && p->owner->_state != AST_STATE_DOWN) {
      DEBUGA_AT("No getstatus, we're neither UP nor DOWN\n", CELLIAX_P_LOG);
      return 0;
    }
  }

  PUSHA_UNLOCKA(&p->controldev_lock);
  LOKKA(&p->controldev_lock);
  res = celliax_serial_write_AT_ack(p, "AT");
  if (res) {
    ERRORA("AT was not acknowledged, continuing but maybe there is a problem\n",
           CELLIAX_P_LOG);
  }
  usleep(1000);

  if (strlen(p->at_query_battchg)) {
    res =
      celliax_serial_write_AT_expect(p, p->at_query_battchg, p->at_query_battchg_expect);
    if (res) {
      WARNINGA("%s does not get %s from the phone. Continuing.\n", CELLIAX_P_LOG,
               p->at_query_battchg, p->at_query_battchg_expect);
    }
    usleep(1000);
  }

  if (strlen(p->at_query_signal)) {
    res =
      celliax_serial_write_AT_expect(p, p->at_query_signal, p->at_query_signal_expect);
    if (res) {
      WARNINGA("%s does not get %s from the phone. Continuing.\n", CELLIAX_P_LOG,
               p->at_query_signal, p->at_query_signal_expect);
    }
    usleep(1000);
  }
  //FIXME all the following commands in config!

  if (p->sms_cnmi_not_supported) {
    res = celliax_serial_write_AT_ack(p, "AT+MMGL=\"HEADER ONLY\"");
    if (res) {
      WARNINGA
        ("%s does not get %s from the modem, maybe a long msg is incoming. If this cellmodem is not a Motorola, you are arriving here because your cellmodem do not supports CNMI kind of incoming SMS alert; please let it know to the developers of Celliax. If this cellmodem is a Motorola and this message keeps repeating, and you cannot correctly receive SMSs from this interface, please manually clean all messages from the cellmodem/SIM. Continuing.\n",
         CELLIAX_P_LOG, "AT+MMGL=\"HEADER ONLY\"", "OK");
    } else {
      usleep(1000);
      if (p->unread_sms_msg_id) {
        char at_command[256];

        if (p->no_ucs2 == 0) {
          res = celliax_serial_write_AT_ack(p, "AT+CSCS=\"UCS2\"");
          if (res) {
            ERRORA
              ("AT+CSCS=\"UCS2\" (set TE messages to ucs2)  do not got OK from the phone\n",
               CELLIAX_P_LOG);
            memset(p->sms_message, 0, sizeof(p->sms_message));
          }
        }

        memset(at_command, 0, sizeof(at_command));
        sprintf(at_command, "AT+CMGR=%d", p->unread_sms_msg_id);
        memset(p->sms_message, 0, sizeof(p->sms_message));

        p->reading_sms_msg = 1;
        res = celliax_serial_write_AT_ack(p, at_command);
        p->reading_sms_msg = 0;
        if (res) {
          ERRORA
            ("AT+CMGR (read SMS) do not got OK from the phone, message sent was:|||%s|||\n",
             CELLIAX_P_LOG, at_command);
        }
        res = celliax_serial_write_AT_ack(p, "AT+CSCS=\"GSM\"");
        if (res) {
          ERRORA
            ("AT+CSCS=\"GSM\" (set TE messages to GSM) do not got OK from the phone\n",
             CELLIAX_P_LOG);
        }
        memset(at_command, 0, sizeof(at_command));
        sprintf(at_command, "AT+CMGD=%d", p->unread_sms_msg_id);    /* delete the message */
        p->unread_sms_msg_id = 0;
        res = celliax_serial_write_AT_ack(p, at_command);
        if (res) {
          ERRORA
            ("AT+CMGD (Delete SMS) do not got OK from the phone, message sent was:|||%s|||\n",
             CELLIAX_P_LOG, at_command);
        }

        if (strlen(p->sms_message)) {

          manager_event(EVENT_FLAG_SYSTEM, "CELLIAXincomingsms",
                        "Interface: %s\r\nSMS_Message: %s\r\n", p->name, p->sms_message);

          if (strlen(p->sms_receiving_program)) {
            int fd1[2];
            pid_t pid1;
            char *arg1[] = { p->sms_receiving_program, (char *) NULL };
            int i;

            NOTICA("incoming SMS message:>>>%s<<<\n", CELLIAX_P_LOG, p->sms_message);
            pipe(fd1);
            pid1 = switch_fork();

            if (pid1 == 0) {    //child
              int err;

              dup2(fd1[0], 0);  // Connect stdin to pipe output
              close(fd1[1]);    // close input pipe side
              setsid();         //session id
              err = execvp(arg1[0], arg1);  //exec our program, with stdin connected to pipe output
              if (err) {
                ERRORA
                  ("'sms_receiving_program' is set in config file to '%s', and it gave us back this error: %d, (%s). SMS received was:---%s---\n",
                   CELLIAX_P_LOG, p->sms_receiving_program, err, strerror(errno),
                   p->sms_message);
              }
              close(fd1[0]);    // close output pipe side
            }                   //starting here continue the parent
            close(fd1[0]);      // close output pipe side
            // write the msg on the pipe input
            for (i = 0; i < strlen(p->sms_message); i++) {
              write(fd1[1], &p->sms_message[i], 1);
            }
            close(fd1[1]);      // close pipe input, let our program know we've finished
          } else {
            ERRORA
              ("got SMS incoming message, but 'sms_receiving_program' is not set in config file. SMS received was:---%s---\n",
               CELLIAX_P_LOG, p->sms_message);
          }
        }
#if 1                           //is this one needed? maybe it can interrupt an incoming call that is just to announce itself
        if (p->phone_callflow == CALLFLOW_CALL_IDLE
            && p->interface_state == AST_STATE_DOWN && p->owner == NULL) {
          /* we're not in a call, neither calling */
          res = celliax_serial_write_AT_ack(p, "AT+CKPD=\"EEE\"");
          if (res) {
            ERRORA
              ("AT+CKPD=\"EEE\" (cellphone screen back to user) do not got OK from the phone\n",
               CELLIAX_P_LOG);
          }
        }
#endif
      }
    }
  }

  UNLOCKA(&p->controldev_lock);
  POPPA_UNLOCKA(&p->controldev_lock);
  return 0;
}

int celliax_serial_read_AT(struct celliax_pvt *p, int look_for_ack, int timeout_usec,
                           int timeout_sec, const char *expected_string, int expect_crlf)
{
  int select_err;
  int res;
  fd_set read_fds;
  struct timeval timeout;
  char tmp_answer[AT_BUFSIZ];
  char tmp_answer2[AT_BUFSIZ];
  char *tmp_answer_ptr;
  char *last_line_ptr;
  int i = 0;
  int read_count = 0;
  int la_counter = 0;
  int at_ack = -1;
  int la_read = 0;

  FD_ZERO(&read_fds);
  FD_SET(p->controldevfd, &read_fds);

  //NOTICA (" INSIDE this celliax_serial_device %s \n", CELLIAX_P_LOG, p->controldevice_name);
  tmp_answer_ptr = tmp_answer;
  memset(tmp_answer, 0, sizeof(char) * AT_BUFSIZ);

  timeout.tv_sec = timeout_sec;
  timeout.tv_usec = timeout_usec;
  PUSHA_UNLOCKA(&p->controldev_lock);
  LOKKA(&p->controldev_lock);

  while ((select_err = select(p->controldevfd + 1, &read_fds, NULL, NULL, &timeout)) > 0) {
    timeout.tv_sec = timeout_sec;   //reset the timeout, linux modify it
    timeout.tv_usec = timeout_usec; //reset the timeout, linux modify it
    read_count =
      read(p->controldevfd, tmp_answer_ptr, AT_BUFSIZ - (tmp_answer_ptr - tmp_answer));

    if (read_count == 0) {
      ERRORA
        ("read 0 bytes!!! Nenormalno! Marking this celliax_serial_device %s as dead, andif it is owned by a channel, hanging up. Maybe the phone is stuck, switched off, power down or battery exhausted\n",
         CELLIAX_P_LOG, p->controldevice_name);
      p->controldev_dead = 1;
      close(p->controldevfd);
      UNLOCKA(&p->controldev_lock);
      if (p->owner) {
        p->owner->hangupcause = AST_CAUSE_FAILURE;
        celliax_queue_control(p->owner, AST_CONTROL_HANGUP);
      }
      return -1;
    }

    if (option_debug > 90) {
      //DEBUGA_AT("1 read %d bytes, --|%s|--\n", CELLIAX_P_LOG, read_count, tmp_answer_ptr);
      //DEBUGA_AT("2 read %d bytes, --|%s|--\n", CELLIAX_P_LOG, read_count, tmp_answer);
    }
    tmp_answer_ptr = tmp_answer_ptr + read_count;

    char *token_ptr;

    la_counter = 0;
    memset(tmp_answer2, 0, sizeof(char) * AT_BUFSIZ);
    strcpy(tmp_answer2, tmp_answer);
    if ((token_ptr = strtok(tmp_answer2, "\n\r"))) {
      last_line_ptr = token_ptr;
      strncpy(p->line_array.result[la_counter], token_ptr, AT_MESG_MAX_LENGTH);
      if (strlen(token_ptr) > AT_MESG_MAX_LENGTH) {
        WARNINGA
          ("AT mesg longer than buffer, original message was: |%s|, in buffer only: |%s|\n",
           CELLIAX_P_LOG, token_ptr, p->line_array.result[la_counter]);
      }
      la_counter++;
      while ((token_ptr = strtok(NULL, "\n\r"))) {
        last_line_ptr = token_ptr;
        strncpy(p->line_array.result[la_counter], token_ptr, AT_MESG_MAX_LENGTH);
        if (strlen(token_ptr) > AT_MESG_MAX_LENGTH) {
          WARNINGA
            ("AT mesg longer than buffer, original message was: |%s|, in buffer only: |%s|\n",
             CELLIAX_P_LOG, token_ptr, p->line_array.result[la_counter]);
        }
        la_counter++;
      }
    } else {
      last_line_ptr = tmp_answer;
    }

    if (expected_string && !expect_crlf) {
      DEBUGA_AT
        ("last_line_ptr=|%s|, expected_string=|%s|, expect_crlf=%d, memcmp(last_line_ptr, expected_string, strlen(expected_string)) = %d\n",
         CELLIAX_P_LOG, last_line_ptr, expected_string, expect_crlf, memcmp(last_line_ptr,
                                                                            expected_string,
                                                                            strlen
                                                                            (expected_string)));
    }

    if (expected_string && !expect_crlf
        && !memcmp(last_line_ptr, expected_string, strlen(expected_string))
      ) {
      strncpy(p->line_array.result[la_counter], last_line_ptr, AT_MESG_MAX_LENGTH);
      // match expected string -> accept it withtout CRLF
      la_counter++;

    }
    /* if the last line read was not a complete line, we'll read the rest in the future */
    else if (tmp_answer[strlen(tmp_answer) - 1] != '\r'
             && tmp_answer[strlen(tmp_answer) - 1] != '\n')
      la_counter--;

    /* let's list the complete lines read so far, without re-listing the lines that has yet been listed */
    if (option_debug > 1) {
      for (i = la_read; i < la_counter; i++)
        DEBUGA_AT("Read line %d: |%s|\n", CELLIAX_P_LOG, i, p->line_array.result[i]);
    }

    /* let's interpret the complete lines read so far (WITHOUT looking for OK, ERROR, and EXPECTED_STRING), without re-interpreting the lines that has been yet interpreted, so we're sure we don't miss anything */
    for (i = la_read; i < la_counter; i++) {

      if ((strcmp(p->line_array.result[i], "RING") == 0)) {
        /* with first RING we wait for callid */
        gettimeofday(&(p->ringtime), NULL);
        /* give CALLID (+CLIP) a chance, wait for the next RING before answering */
        if (p->phone_callflow == CALLFLOW_INCOMING_RING) {
          /* we're at the second ring, set the interface state, will be answered by celliax_do_monitor */
          DEBUGA_AT("|%s| got second RING\n", CELLIAX_P_LOG, p->line_array.result[i]);
          p->interface_state = AST_STATE_RING;
        } else {
          /* we're at the first ring, so there is no CALLID yet thus clean the previous one 
             just in case we don't receive the caller identification in this new call */
          memset(p->callid_name, 0, sizeof(p->callid_name));
          memset(p->callid_number, 0, sizeof(p->callid_number));
          /* only send AT+CLCC? if the device previously reported its support */
          if (p->at_has_clcc != 0) {
            /* we're at the first ring, try to get CALLID (with +CLCC) */
            DEBUGA_AT("|%s| got first RING, sending AT+CLCC?\n", CELLIAX_P_LOG,
                      p->line_array.result[i]);
            res = celliax_serial_write_AT_noack(p, "AT+CLCC?");
            if (res) {
              ERRORA("AT+CLCC? (call list) was not correctly sent to the phone\n",
                     CELLIAX_P_LOG);
            }
          } else {
            DEBUGA_AT("|%s| got first RING, but not sending AT+CLCC? as this device "
                      "seems not to support\n", CELLIAX_P_LOG, p->line_array.result[i]);
          }
        }
        p->phone_callflow = CALLFLOW_INCOMING_RING;
      }

      if ((strncmp(p->line_array.result[i], "+CLCC", 5) == 0)) {
        /* with clcc we wait for clip */
        memset(p->callid_name, 0, sizeof(p->callid_name));
        memset(p->callid_number, 0, sizeof(p->callid_number));
        int commacount = 0;
        int a = 0;
        int b = 0;
        int c = 0;

        for (a = 0; a < strlen(p->line_array.result[i]); a++) {

          if (p->line_array.result[i][a] == ',') {
            commacount++;
          }
          if (commacount == 5) {
            if (p->line_array.result[i][a] != ',' && p->line_array.result[i][a] != '"') {
              p->callid_number[b] = p->line_array.result[i][a];
              b++;
            }
          }
          if (commacount == 7) {
            if (p->line_array.result[i][a] != ',' && p->line_array.result[i][a] != '"') {
              p->callid_name[c] = p->line_array.result[i][a];
              c++;
            }
          }
        }

        p->phone_callflow = CALLFLOW_INCOMING_RING;
        DEBUGA_AT("|%s| CLCC CALLID: name is %s, number is %s\n", CELLIAX_P_LOG,
                  p->line_array.result[i],
                  p->callid_name[0] ? p->callid_name : "not available",
                  p->callid_number[0] ? p->callid_number : "not available");
      }

      if ((strncmp(p->line_array.result[i], "+CLIP", 5) == 0)) {
        /* with CLIP, we want to answer right away */
        memset(p->callid_name, 0, sizeof(p->callid_name));
        memset(p->callid_number, 0, sizeof(p->callid_number));

        int commacount = 0;
        int a = 0;
        int b = 0;
        int c = 0;

        for (a = 7; a < strlen(p->line_array.result[i]); a++) {
          if (p->line_array.result[i][a] == ',') {
            commacount++;
          }
          if (commacount == 0) {
            if (p->line_array.result[i][a] != ',' && p->line_array.result[i][a] != '"') {
              p->callid_number[b] = p->line_array.result[i][a];
              b++;
            }
          }
          if (commacount == 4) {
            if (p->line_array.result[i][a] != ',' && p->line_array.result[i][a] != '"') {
              p->callid_name[c] = p->line_array.result[i][a];
              c++;
            }
          }
        }

        if (p->interface_state != AST_STATE_RING) {
          gettimeofday(&(p->call_incoming_time), NULL);
          DEBUGA_AT("AST_STATE_RING call_incoming_time.tv_sec=%ld\n",
                    CELLIAX_P_LOG, p->call_incoming_time.tv_sec);

        }

        p->interface_state = AST_STATE_RING;
        p->phone_callflow = CALLFLOW_INCOMING_RING;
        DEBUGA_AT("|%s| CLIP INCOMING CALLID: name is %s, number is %s\n", CELLIAX_P_LOG,
                  p->line_array.result[i],
                  p->callid_name[0] != 1 ? p->callid_name : "not available",
                  p->callid_number[0] ? p->callid_number : "not available");
      }

      if ((strcmp(p->line_array.result[i], "BUSY") == 0)) {
        p->phone_callflow = CALLFLOW_CALL_LINEBUSY;
        if (option_debug > 1)
          DEBUGA_AT("|%s| CALLFLOW_CALL_LINEBUSY\n", CELLIAX_P_LOG,
                    p->line_array.result[i]);
        if (p->interface_state != AST_STATE_DOWN && p->owner) {
          ast_setstate(p->owner, AST_STATE_BUSY);
          celliax_queue_control(p->owner, AST_CONTROL_BUSY);
        } else {
          ERRORA("Why BUSY now?\n", CELLIAX_P_LOG);
        }
      }
      if ((strcmp(p->line_array.result[i], "NO ANSWER") == 0)) {
        p->phone_callflow = CALLFLOW_CALL_NOANSWER;
        if (option_debug > 1)
          DEBUGA_AT("|%s| CALLFLOW_CALL_NOANSWER\n", CELLIAX_P_LOG,
                    p->line_array.result[i]);
        if (p->interface_state != AST_STATE_DOWN && p->owner) {
          p->owner->hangupcause = AST_CAUSE_NO_ANSWER;
          celliax_queue_control(p->owner, AST_CONTROL_HANGUP);
        } else {
          ERRORA("Why NO ANSWER now?\n", CELLIAX_P_LOG);
        }
      }
      if ((strcmp(p->line_array.result[i], "NO CARRIER") == 0)) {
	      if (p->phone_callflow != CALLFLOW_CALL_HANGUP_REQUESTED) {
		      p->phone_callflow = CALLFLOW_CALL_NOCARRIER;
		      if (option_debug > 1)
			      DEBUGA_AT("|%s| CALLFLOW_CALL_NOCARRIER\n", CELLIAX_P_LOG,
					      p->line_array.result[i]);
		      p->control_to_send = 0;
		      usleep(20000);
		      if (p->interface_state != AST_STATE_DOWN && p->owner) {
			      p->owner->hangupcause = AST_CAUSE_FAILURE;
			      celliax_queue_control(p->owner, AST_CONTROL_HANGUP);
		      } else {
			      ERRORA("Why NO CARRIER now?\n", CELLIAX_P_LOG);
		      }
	      }
      }

      if ((strncmp(p->line_array.result[i], "+CBC:", 5) == 0)) {
        int power_supply, battery_strenght, err;

        power_supply = battery_strenght = 0;

        err =
          sscanf(&p->line_array.result[i][6], "%d,%d", &power_supply, &battery_strenght);
        if (err < 2) {
          DEBUGA_AT("|%s| is not formatted as: |+CBC: xx,yy| now trying  |+CBC:xx,yy|\n",
                    CELLIAX_P_LOG, p->line_array.result[i]);

          err =
            sscanf(&p->line_array.result[i][5], "%d,%d", &power_supply,
                   &battery_strenght);
          DEBUGA_AT("|%s| +CBC: Powered by %s, battery strenght=%d\n", CELLIAX_P_LOG,
                    p->line_array.result[i], power_supply ? "power supply" : "battery",
                    battery_strenght);

        }

        if (err < 2) {
          DEBUGA_AT("|%s| is not formatted as: |+CBC:xx,yy| giving up\n", CELLIAX_P_LOG,
                    p->line_array.result[i]);
        }

        else {
          if (option_debug > 1)
            DEBUGA_AT("|%s| +CBC: Powered by %s, battery strenght=%d\n", CELLIAX_P_LOG,
                      p->line_array.result[i], power_supply ? "power supply" : "battery",
                      battery_strenght);
          if (!power_supply) {
            if (battery_strenght < 10) {
              ERRORA("|%s| BATTERY ALMOST EXHAUSTED\n", CELLIAX_P_LOG,
                     p->line_array.result[i]);
            } else if (battery_strenght < 20) {
              WARNINGA("|%s| BATTERY LOW\n", CELLIAX_P_LOG, p->line_array.result[i]);

            }

          }
        }

      }

      if ((strncmp(p->line_array.result[i], "+CSQ:", 5) == 0)) {
        int signal_quality, ber, err;

        signal_quality = ber = 0;

        err = sscanf(&p->line_array.result[i][6], "%d,%d", &signal_quality, &ber);
        if (option_debug > 1)
          DEBUGA_AT("|%s| +CSQ: Signal Quality: %d, Error Rate=%d\n", CELLIAX_P_LOG,
                    p->line_array.result[i], signal_quality, ber);
        if (err < 2) {
          ERRORA("|%s| is not formatted as: |+CSQ: xx,yy|\n", CELLIAX_P_LOG,
                 p->line_array.result[i]);
        } else {
          if (signal_quality < 11 || signal_quality == 99) {
            WARNINGA
              ("|%s| CELLPHONE GETS ALMOST NO SIGNAL, consider to move it or additional antenna\n",
               CELLIAX_P_LOG, p->line_array.result[i]);
          } else if (signal_quality < 15) {
            WARNINGA("|%s| CELLPHONE GETS SIGNAL LOW\n", CELLIAX_P_LOG,
                     p->line_array.result[i]);

          }

        }

      }
      if ((strncmp(p->line_array.result[i], "+CMGW:", 6) == 0)) {
        int err;

        err = sscanf(&p->line_array.result[i][7], "%s", p->at_cmgw);
        DEBUGA_AT("|%s| +CMGW: %s\n", CELLIAX_P_LOG, p->line_array.result[i], p->at_cmgw);
        if (err < 1) {
          ERRORA("|%s| is not formatted as: |+CMGW: xxxx|\n", CELLIAX_P_LOG,
                 p->line_array.result[i]);
        }

      }

      /* at_call_* are unsolicited messages sent by the modem to signal us about call processing activity and events */
      if ((strcmp(p->line_array.result[i], p->at_call_idle) == 0)) {
        p->phone_callflow = CALLFLOW_CALL_IDLE;
        if (option_debug > 1)
          DEBUGA_AT("|%s| CALLFLOW_CALL_IDLE\n", CELLIAX_P_LOG, p->line_array.result[i]);
        if (p->interface_state != AST_STATE_DOWN && p->owner) {
          DEBUGA_AT("just received a remote HANGUP\n", CELLIAX_P_LOG);
          p->owner->hangupcause = AST_CAUSE_NORMAL;
          celliax_queue_control(p->owner, AST_CONTROL_HANGUP);
          DEBUGA_AT("just sent AST_CONTROL_HANGUP\n", CELLIAX_P_LOG);
        }
      }

      if ((strcmp(p->line_array.result[i], p->at_call_incoming) == 0)) {

        //char list_command[64];

        if (option_debug > 1)
          DEBUGA_AT("|%s| CALLFLOW_CALL_INCOMING\n", CELLIAX_P_LOG,
                    p->line_array.result[i]);

        if (p->phone_callflow != CALLFLOW_CALL_INCOMING
            && p->phone_callflow != CALLFLOW_INCOMING_RING) {
          //mark the time of CALLFLOW_CALL_INCOMING
          gettimeofday(&(p->call_incoming_time), NULL);
          p->phone_callflow = CALLFLOW_CALL_INCOMING;
          DEBUGA_AT("CALLFLOW_CALL_INCOMING call_incoming_time.tv_sec=%ld\n",
                    CELLIAX_P_LOG, p->call_incoming_time.tv_sec);

        }
      }

      if ((strcmp(p->line_array.result[i], p->at_call_active) == 0)) {
        p->phone_callflow = CALLFLOW_CALL_ACTIVE;
        if (option_debug > 1)
          DEBUGA_AT("|%s| CALLFLOW_CALL_ACTIVE\n", CELLIAX_P_LOG,
                    p->line_array.result[i]);

        if (p->owner && p->interface_state == CALLFLOW_CALL_DIALING) {
          DEBUGA_PBX("just received a remote ANSWER\n", CELLIAX_P_LOG);
          if (p->owner->_state != AST_STATE_UP) {
            celliax_queue_control(p->owner, AST_CONTROL_RINGING);
            DEBUGA_PBX("just sent AST_CONTROL_RINGING\n", CELLIAX_P_LOG);
            DEBUGA_PBX("going to send AST_CONTROL_ANSWER\n", CELLIAX_P_LOG);
            celliax_queue_control(p->owner, AST_CONTROL_ANSWER);
            DEBUGA_PBX("just sent AST_CONTROL_ANSWER\n", CELLIAX_P_LOG);
          }
        } else {
        }
        p->interface_state = AST_STATE_UP;
        DEBUGA_PBX("just interface_state UP\n", CELLIAX_P_LOG);
      }

      if ((strcmp(p->line_array.result[i], p->at_call_calling) == 0)) {
        p->phone_callflow = CALLFLOW_CALL_DIALING;
        if (option_debug > 1)
          DEBUGA_AT("|%s| CALLFLOW_CALL_DIALING\n", CELLIAX_P_LOG,
                    p->line_array.result[i]);
      }
      if ((strcmp(p->line_array.result[i], p->at_call_failed) == 0)) {
        p->phone_callflow = CALLFLOW_CALL_FAILED;
        if (option_debug > 1)
          DEBUGA_AT("|%s| CALLFLOW_CALL_FAILED\n", CELLIAX_P_LOG,
                    p->line_array.result[i]);
        if (p->interface_state != AST_STATE_DOWN && p->owner) {
          p->owner->hangupcause = AST_CAUSE_FAILURE;
          celliax_queue_control(p->owner, AST_CONTROL_HANGUP);
        }
      }

      if ((strncmp(p->line_array.result[i], "+CSCA:", 6) == 0)) {   //TODO SMS FIXME in config!
        if (option_debug > 1)
          DEBUGA_AT("|%s| +CSCA: Message Center Address!\n", CELLIAX_P_LOG,
                    p->line_array.result[i]);
      }

      if ((strncmp(p->line_array.result[i], "+CMGF:", 6) == 0)) {   //TODO SMS FIXME in config!
        if (option_debug > 1)
          DEBUGA_AT("|%s| +CMGF: Message Format!\n", CELLIAX_P_LOG,
                    p->line_array.result[i]);
      }

      if ((strncmp(p->line_array.result[i], "+CMTI:", 6) == 0)) {   //TODO SMS FIXME in config!
        int err;
        int pos;

        //FIXME all the following commands in config!
        if (option_debug)
          DEBUGA_AT("|%s| +CMTI: Incoming SMS!\n", CELLIAX_P_LOG,
                    p->line_array.result[i]);

        err = sscanf(&p->line_array.result[i][12], "%d", &pos);
        if (err < 1) {
          ERRORA("|%s| is not formatted as: |+CMTI: \"MT\",xx|\n", CELLIAX_P_LOG,
                 p->line_array.result[i]);
        } else {
          DEBUGA_AT("|%s| +CMTI: Incoming SMS in position: %d!\n", CELLIAX_P_LOG,
                    p->line_array.result[i], pos);
          p->unread_sms_msg_id = pos;
          usleep(1000);

          if (p->unread_sms_msg_id) {
            char at_command[256];

            if (p->no_ucs2 == 0) {
              res = celliax_serial_write_AT_ack(p, "AT+CSCS=\"UCS2\"");
              if (res) {
                ERRORA
                  ("AT+CSCS=\"UCS2\" (set TE messages to ucs2)  do not got OK from the phone, continuing\n",
                   CELLIAX_P_LOG);
                //memset(p->sms_message, 0, sizeof(p->sms_message));
              }
            }

            memset(at_command, 0, sizeof(at_command));
            sprintf(at_command, "AT+CMGR=%d", p->unread_sms_msg_id);
            memset(p->sms_message, 0, sizeof(p->sms_message));

            p->reading_sms_msg = 1;
            res = celliax_serial_write_AT_ack(p, at_command);
            p->reading_sms_msg = 0;
            if (res) {
              ERRORA
                ("AT+CMGR (read SMS) do not got OK from the phone, message sent was:|||%s|||\n",
                 CELLIAX_P_LOG, at_command);
            }
            res = celliax_serial_write_AT_ack(p, "AT+CSCS=\"GSM\"");
            if (res) {
              ERRORA
                ("AT+CSCS=\"GSM\" (set TE messages to GSM) do not got OK from the phone\n",
                 CELLIAX_P_LOG);
            }
            memset(at_command, 0, sizeof(at_command));
            sprintf(at_command, "AT+CMGD=%d", p->unread_sms_msg_id);    /* delete the message */
            p->unread_sms_msg_id = 0;
            res = celliax_serial_write_AT_ack(p, at_command);
            if (res) {
              ERRORA
                ("AT+CMGD (Delete SMS) do not got OK from the phone, message sent was:|||%s|||\n",
                 CELLIAX_P_LOG, at_command);
            }

            if (strlen(p->sms_message)) {
              manager_event(EVENT_FLAG_SYSTEM, "CELLIAXincomingsms",
                            "Interface: %s\r\nSMS_Message: %s\r\n", p->name,
                            p->sms_message);
              if (strlen(p->sms_receiving_program)) {
                int fd1[2];
                pid_t pid1;
                char *arg1[] = { p->sms_receiving_program, (char *) NULL };
                int i;

                NOTICA("incoming SMS message:>>>%s<<<\n", CELLIAX_P_LOG, p->sms_message);
                pipe(fd1);
                pid1 = switch_fork();

                if (pid1 == 0) {    //child
                  int err;

                  dup2(fd1[0], 0);  // Connect stdin to pipe output
                  close(fd1[1]);    // close input pipe side
		close(p->controldevfd);
                  setsid();     //session id
                  err = execvp(arg1[0], arg1);  //exec our program, with stdin connected to pipe output
                  if (err) {
                    ERRORA
                      ("'sms_receiving_program' is set in config file to '%s', and it gave us back this error: %d, (%s). SMS received was:---%s---\n",
                       CELLIAX_P_LOG, p->sms_receiving_program, err, strerror(errno),
                       p->sms_message);
                  }
                  close(fd1[0]);    // close output pipe side
                }
//starting here continue the parent
                close(fd1[0]);  // close output pipe side
                // write the msg on the pipe input
                for (i = 0; i < strlen(p->sms_message); i++) {
                  write(fd1[1], &p->sms_message[i], 1);
                }
                close(fd1[1]);  // close pipe input, let our program know we've finished
              } else {
                ERRORA
                  ("got SMS incoming message, but 'sms_receiving_program' is not set in config file. SMS received was:---%s---\n",
                   CELLIAX_P_LOG, p->sms_message);
              }
            }
#if 1                           //is this one needed? maybe it can interrupt an incoming call that is just to announce itself
            if (p->phone_callflow == CALLFLOW_CALL_IDLE
                && p->interface_state == AST_STATE_DOWN && p->owner == NULL) {
              /* we're not in a call, neither calling */
              res = celliax_serial_write_AT_ack(p, "AT+CKPD=\"EEE\"");
              if (res) {
                ERRORA
                  ("AT+CKPD=\"EEE\" (cellphone screen back to user) do not got OK from the phone\n",
                   CELLIAX_P_LOG);
              }
            }
#endif
          }                     //unread_msg_id

        }                       //CMTI well formatted

      }                         //CMTI

      if ((strncmp(p->line_array.result[i], "+MMGL:", 6) == 0)) {   //TODO MOTOROLA SMS FIXME in config!
        int err = 0;
        //int unread_msg_id=0;

        if (option_debug)
          DEBUGA_AT("|%s| +MMGL: Listing Motorola SMSs!\n", CELLIAX_P_LOG,
                    p->line_array.result[i]);

        err = sscanf(&p->line_array.result[i][7], "%d", &p->unread_sms_msg_id);
        if (err < 1) {
          ERRORA("|%s| is not formatted as: |+MMGL: xx|\n", CELLIAX_P_LOG,
                 p->line_array.result[i]);
        }
      }
      if ((strncmp(p->line_array.result[i], "+CMGL:", 6) == 0)) {   //TODO  SMS FIXME in config!
        if (option_debug)
          DEBUGA_AT("|%s| +CMGL: Listing SMSs!\n", CELLIAX_P_LOG,
                    p->line_array.result[i]);
      }
      if ((strncmp(p->line_array.result[i], "+MMGR:", 6) == 0)) {   //TODO MOTOROLA SMS FIXME in config!
        if (option_debug)
          DEBUGA_AT("|%s| +MMGR: Reading Motorola SMS!\n", CELLIAX_P_LOG,
                    p->line_array.result[i]);
        if (p->reading_sms_msg)
          p->reading_sms_msg++;
      }
      if ((strncmp(p->line_array.result[i], "+CMGR: \"STO U", 13) == 0)) {  //TODO  SMS FIXME in config!
        if (option_debug)
          DEBUGA_AT("|%s| +CMGR: Reading stored UNSENT SMS!\n", CELLIAX_P_LOG,
                    p->line_array.result[i]);
      } else if ((strncmp(p->line_array.result[i], "+CMGR: \"STO S", 13) == 0)) {   //TODO  SMS FIXME in config!
        if (option_debug)
          DEBUGA_AT("|%s| +CMGR: Reading stored SENT SMS!\n", CELLIAX_P_LOG,
                    p->line_array.result[i]);
      } else if ((strncmp(p->line_array.result[i], "+CMGR: \"REC R", 13) == 0)) {   //TODO  SMS FIXME in config!
        if (option_debug)
          DEBUGA_AT("|%s| +CMGR: Reading received READ SMS!\n", CELLIAX_P_LOG,
                    p->line_array.result[i]);
      } else if ((strncmp(p->line_array.result[i], "+CMGR: \"REC U", 13) == 0)) {   //TODO  SMS FIXME in config!
        if (option_debug)
          DEBUGA_AT("|%s| +CMGR: Reading received UNREAD SMS!\n", CELLIAX_P_LOG,
                    p->line_array.result[i]);
        if (p->reading_sms_msg)
          p->reading_sms_msg++;
      }

      if ((strcmp(p->line_array.result[i], "+MCST: 17") == 0)) {    /* motorola call processing unsolicited messages */
        p->phone_callflow = CALLFLOW_CALL_INFLUX;
        if (option_debug > 1)
          DEBUGA_AT("|%s| CALLFLOW_CALL_INFLUX\n", CELLIAX_P_LOG,
                    p->line_array.result[i]);
      }

      if ((strcmp(p->line_array.result[i], "+MCST: 68") == 0)) {    /* motorola call processing unsolicited messages */
        p->phone_callflow = CALLFLOW_CALL_NOSERVICE;
        if (option_debug > 1)
          DEBUGA_AT("|%s| CALLFLOW_CALL_NOSERVICE\n", CELLIAX_P_LOG,
                    p->line_array.result[i]);
        if (p->interface_state != AST_STATE_DOWN && p->owner) {
          p->owner->hangupcause = AST_CAUSE_FAILURE;
          celliax_queue_control(p->owner, AST_CONTROL_HANGUP);
        }
      }
      if ((strcmp(p->line_array.result[i], "+MCST: 70") == 0)) {    /* motorola call processing unsolicited messages */
        p->phone_callflow = CALLFLOW_CALL_OUTGOINGRESTRICTED;
        if (option_debug > 1)
          DEBUGA_AT("|%s| CALLFLOW_CALL_OUTGOINGRESTRICTED\n", CELLIAX_P_LOG,
                    p->line_array.result[i]);
        if (p->interface_state != AST_STATE_DOWN && p->owner) {
          p->owner->hangupcause = AST_CAUSE_FAILURE;
          celliax_queue_control(p->owner, AST_CONTROL_HANGUP);
        }
      }
      if ((strcmp(p->line_array.result[i], "+MCST: 72") == 0)) {    /* motorola call processing unsolicited messages */
        p->phone_callflow = CALLFLOW_CALL_SECURITYFAIL;
        if (option_debug > 1)
          DEBUGA_AT("|%s| CALLFLOW_CALL_SECURITYFAIL\n", CELLIAX_P_LOG,
                    p->line_array.result[i]);
        if (p->interface_state != AST_STATE_DOWN && p->owner) {
          p->owner->hangupcause = AST_CAUSE_FAILURE;
          celliax_queue_control(p->owner, AST_CONTROL_HANGUP);
        }
      }

      if ((strncmp(p->line_array.result[i], "+CPBR", 5) == 0)) {    /* phonebook stuff begins */

        if (p->phonebook_querying) {    /* probably phonebook struct begins */
          int err, first_entry, last_entry, number_lenght, text_lenght;

          if (option_debug)
            DEBUGA_AT("phonebook struct: |%s|\n", CELLIAX_P_LOG, p->line_array.result[i]);

          err =
            sscanf(&p->line_array.result[i][8], "%d-%d),%d,%d", &first_entry, &last_entry,
                   &number_lenght, &text_lenght);
          if (err < 4) {

            err =
              sscanf(&p->line_array.result[i][7], "%d-%d,%d,%d", &first_entry,
                     &last_entry, &number_lenght, &text_lenght);
          }

          if (err < 4) {
            ERRORA
              ("phonebook struct: |%s| is nor formatted as: |+CPBR: (1-750),40,14| neither as: |+CPBR: 1-750,40,14|\n",
               CELLIAX_P_LOG, p->line_array.result[i]);
          } else {

            if (option_debug)
              DEBUGA_AT
                ("First entry: %d, last entry: %d, phone number max lenght: %d, text max lenght: %d\n",
                 CELLIAX_P_LOG, first_entry, last_entry, number_lenght, text_lenght);
            p->phonebook_first_entry = first_entry;
            p->phonebook_last_entry = last_entry;
            p->phonebook_number_lenght = number_lenght;
            p->phonebook_text_lenght = text_lenght;
          }

        } else {                /* probably phonebook entry begins */

          if (p->phonebook_listing) {
            int err, entry_id, entry_type;

            char entry_number[256];
            char entry_text[256];

            if (option_debug)
              DEBUGA_AT("phonebook entry: |%s|\n", CELLIAX_P_LOG,
                        p->line_array.result[i]);

            err =
              sscanf(&p->line_array.result[i][7], "%d,\"%255[0-9+]\",%d,\"%255[^\"]\"",
                     &entry_id, entry_number, &entry_type, entry_text);
            if (err < 4) {
              ERRORA
                ("err=%d, phonebook entry: |%s| is not formatted as: |+CPBR: 504,\"+39025458068\",145,\"ciao a tutti\"|\n",
                 CELLIAX_P_LOG, err, p->line_array.result[i]);
            } else {
              //TODO: sanitize entry_text
              if (option_debug)
                DEBUGA_AT("Number: %s, Text: %s, Type: %d\n", CELLIAX_P_LOG, entry_number,
                          entry_text, entry_type);
              /* write entry in phonebook file */
              if (p->phonebook_writing_fp) {
                celliax_dir_entry_extension++;

                fprintf(p->phonebook_writing_fp,
                        "%s  => ,%sSKO,,,hidefromdir=%s|phonebook_direct_calling_ext=%d%s%.4d|phonebook_entry_fromcell=%s|phonebook_entry_owner=%s\n",
                        entry_number, entry_text, "no",
                        p->celliax_dir_entry_extension_prefix, "2",
                        celliax_dir_entry_extension, "yes", "not_specified");
                fprintf(p->phonebook_writing_fp,
                        "%s  => ,%sDO,,,hidefromdir=%s|phonebook_direct_calling_ext=%d%s%.4d|phonebook_entry_fromcell=%s|phonebook_entry_owner=%s\n",
                        entry_number, entry_text, "no",
                        p->celliax_dir_entry_extension_prefix, "3",
                        celliax_dir_entry_extension, "yes", "not_specified");
              }
            }

          }

          if (p->phonebook_listing_received_calls) {
            int err, entry_id, entry_type;

            char entry_number[256] = "";
            char entry_text[256] = "";

            if (option_debug)
              DEBUGA_AT("phonebook entry: |%s|\n", CELLIAX_P_LOG,
                        p->line_array.result[i]);

            err =
              sscanf(&p->line_array.result[i][7], "%d,\"%255[0-9+]\",%d,\"%255[^\"]\"",
                     &entry_id, entry_number, &entry_type, entry_text);
            if (err < 1) {      //we match only on the progressive id, maybe the remote party has not sent its number, and/or there is no corresponding text entry in the phone directory
              ERRORA
                ("err=%d, phonebook entry: |%s| is not formatted as: |+CPBR: 504,\"+39025458068\",145,\"ciao a tutti\"|\n",
                 CELLIAX_P_LOG, err, p->line_array.result[i]);
            } else {
              //TODO: sanitize entry_text

              if (option_debug)
                DEBUGA_AT("Number: %s, Text: %s, Type: %d\n", CELLIAX_P_LOG, entry_number,
                          entry_text, entry_type);
              memset(p->callid_name, 0, sizeof(p->callid_name));
              memset(p->callid_number, 0, sizeof(p->callid_number));
              strncpy(p->callid_name, entry_text, sizeof(p->callid_name));
              strncpy(p->callid_number, entry_number, sizeof(p->callid_number));
              if (option_debug)
                DEBUGA_AT("incoming callid: Text: %s, Number: %s\n", CELLIAX_P_LOG,
                          p->callid_name, p->callid_number);

              DEBUGA_AT("|%s| CPBR INCOMING CALLID: name is %s, number is %s\n",
                        CELLIAX_P_LOG, p->line_array.result[i],
                        p->callid_name[0] != 1 ? p->callid_name : "not available",
                        p->callid_number[0] ? p->callid_number : "not available");

              /* mark the time of RING */
              gettimeofday(&(p->ringtime), NULL);
              p->interface_state = AST_STATE_RING;
              p->phone_callflow = CALLFLOW_INCOMING_RING;

            }

          }

          else {
            DEBUGA_AT("phonebook entry: |%s|\n", CELLIAX_P_LOG, p->line_array.result[i]);

          }
        }

      }

      if ((strncmp(p->line_array.result[i], "*ECAV", 5) == 0) || (strncmp(p->line_array.result[i], "*ECAM", 5) == 0)) { /* sony-ericsson call processing unsolicited messages */
        int res, ccid, ccstatus, calltype, processid, exitcause, number, type;
        res = ccid = ccstatus = calltype = processid = exitcause = number = type = 0;
        res =
          sscanf(&p->line_array.result[i][6], "%d,%d,%d,%d,%d,%d,%d", &ccid, &ccstatus,
                 &calltype, &processid, &exitcause, &number, &type);
        /* only changes the phone_callflow if enought parameters were parsed */
        if (res >= 3) {
          switch (ccstatus) {
          case 0:
            if (p->owner) {
              ast_setstate(p->owner, AST_STATE_DOWN);
              p->owner->hangupcause = AST_CAUSE_NORMAL;
              celliax_queue_control(p->owner, AST_CONTROL_HANGUP);
            }
            p->phone_callflow = CALLFLOW_CALL_IDLE;
            p->interface_state = AST_STATE_DOWN;
            if (option_debug > 1)
              DEBUGA_AT("|%s| Sony-Ericsson *ECAM/*ECAV: IDLE\n", CELLIAX_P_LOG,
                        p->line_array.result[i]);
            break;
          case 1:
            if (option_debug > 1)
              DEBUGA_AT("|%s| Sony-Ericsson *ECAM/*ECAV: CALLING\n", CELLIAX_P_LOG,
                        p->line_array.result[i]);
            break;
          case 2:
            if (p->owner) {
              ast_setstate(p->owner, AST_STATE_DIALING);
            }
            p->interface_state = CALLFLOW_CALL_DIALING;
            if (option_debug > 1)
              DEBUGA_AT("|%s| Sony-Ericsson *ECAM/*ECAV: CONNECTING\n", CELLIAX_P_LOG,
                        p->line_array.result[i]);
            break;
          case 3:
            if (p->owner) {
              ast_setstate(p->owner, AST_STATE_UP);
              celliax_queue_control(p->owner, AST_CONTROL_ANSWER);
            }
            p->phone_callflow = CALLFLOW_CALL_ACTIVE;
            p->interface_state = AST_STATE_UP;
            if (option_debug > 1)
              DEBUGA_AT("|%s| Sony-Ericsson *ECAM/*ECAV: ACTIVE\n", CELLIAX_P_LOG,
                        p->line_array.result[i]);
            break;
          case 4:
            if (option_debug > 1)
              DEBUGA_AT
                ("|%s| Sony-Ericsson *ECAM/*ECAV: don't know how to handle HOLD event\n",
                 CELLIAX_P_LOG, p->line_array.result[i]);
            break;
          case 5:
            if (option_debug > 1)
              DEBUGA_AT
                ("|%s| Sony-Ericsson *ECAM/*ECAV: don't know how to handle WAITING event\n",
                 CELLIAX_P_LOG, p->line_array.result[i]);
            break;
          case 6:
            if (option_debug > 1)
              DEBUGA_AT
                ("|%s| Sony-Ericsson *ECAM/*ECAV: don't know how to handle ALERTING event\n",
                 CELLIAX_P_LOG, p->line_array.result[i]);
            break;
          case 7:
            if (p->owner) {
              ast_setstate(p->owner, AST_STATE_BUSY);
              celliax_queue_control(p->owner, AST_CONTROL_BUSY);
            }
            p->phone_callflow = CALLFLOW_CALL_LINEBUSY;
            p->interface_state = AST_STATE_BUSY;
            if (option_debug > 1)
              DEBUGA_AT("|%s| Sony-Ericsson *ECAM/*ECAV: BUSY\n", CELLIAX_P_LOG,
                        p->line_array.result[i]);
            break;
          }
        } else {
          if (option_debug > 1)
            DEBUGA_AT("|%s| Sony-Ericsson *ECAM/*ECAV: could not parse parameters\n",
                      CELLIAX_P_LOG, p->line_array.result[i]);
        }

      }

      /* at_indicator_* are unsolicited messages sent by the phone to signal us that some of its visual indicators on its screen has changed, based on CIND CMER ETSI docs */
      if ((strcmp(p->line_array.result[i], p->at_indicator_noservice_string) == 0)) {
        if (option_debug > 1)
          ERRORA("|%s| at_indicator_noservice_string\n", CELLIAX_P_LOG,
                 p->line_array.result[i]);
      }

      if ((strcmp(p->line_array.result[i], p->at_indicator_nosignal_string) == 0)) {
        if (option_debug > 1)
          ERRORA("|%s| at_indicator_nosignal_string\n", CELLIAX_P_LOG,
                 p->line_array.result[i]);
      }

      if ((strcmp(p->line_array.result[i], p->at_indicator_lowsignal_string) == 0)) {
        if (option_debug > 1)
          WARNINGA("|%s| at_indicator_lowsignal_string\n", CELLIAX_P_LOG,
                   p->line_array.result[i]);
      }

      if ((strcmp(p->line_array.result[i], p->at_indicator_lowbattchg_string) == 0)) {
        if (option_debug > 1)
          WARNINGA("|%s| at_indicator_lowbattchg_string\n", CELLIAX_P_LOG,
                   p->line_array.result[i]);
      }

      if ((strcmp(p->line_array.result[i], p->at_indicator_nobattchg_string) == 0)) {
        if (option_debug > 1)
          ERRORA("|%s| at_indicator_nobattchg_string\n", CELLIAX_P_LOG,
                 p->line_array.result[i]);
      }

      if ((strcmp(p->line_array.result[i], p->at_indicator_callactive_string) == 0)) {
        if (option_debug > 1)
          DEBUGA_AT("|%s| at_indicator_callactive_string\n", CELLIAX_P_LOG,
                    p->line_array.result[i]);
      }

      if ((strcmp(p->line_array.result[i], p->at_indicator_nocallactive_string) == 0)) {
        if (option_debug > 1)
          DEBUGA_AT("|%s| at_indicator_nocallactive_string\n", CELLIAX_P_LOG,
                    p->line_array.result[i]);
      }

      if ((strcmp(p->line_array.result[i], p->at_indicator_nocallsetup_string) == 0)) {
        if (option_debug > 1)
          DEBUGA_AT("|%s| at_indicator_nocallsetup_string\n", CELLIAX_P_LOG,
                    p->line_array.result[i]);
      }

      if ((strcmp(p->line_array.result[i], p->at_indicator_callsetupincoming_string) ==
           0)) {
        if (option_debug > 1)
          DEBUGA_AT("|%s| at_indicator_callsetupincoming_string\n", CELLIAX_P_LOG,
                    p->line_array.result[i]);
      }

      if ((strcmp(p->line_array.result[i], p->at_indicator_callsetupoutgoing_string) ==
           0)) {
        if (option_debug > 1)
          DEBUGA_AT("|%s| at_indicator_callsetupoutgoing_string\n", CELLIAX_P_LOG,
                    p->line_array.result[i]);
      }

      if ((strcmp(p->line_array.result[i], p->at_indicator_callsetupremoteringing_string)
           == 0)) {
        if (option_debug > 1)
          DEBUGA_AT("|%s| at_indicator_callsetupremoteringing_string\n", CELLIAX_P_LOG,
                    p->line_array.result[i]);
      }

    }

    /* let's look for OK, ERROR and EXPECTED_STRING in the complete lines read so far, without re-looking at the lines that has been yet looked at */
    for (i = la_read; i < la_counter; i++) {
      if (expected_string) {
        if ((strncmp(p->line_array.result[i], expected_string, strlen(expected_string))
             == 0)) {
          if (option_debug > 1)
            DEBUGA_AT("|%s| got what EXPECTED\n", CELLIAX_P_LOG, p->line_array.result[i]);
          at_ack = AT_OK;
        }
      } else {
        //if ((strcmp(p->line_array.result[i], "OK") == 0)) {
        if ((strcmp(p->line_array.result[i], "OK") == 0) || (strcmp(p->line_array.result[i], "NO CARRIER") == 0) ) {
          if (option_debug > 1)
            DEBUGA_AT("got OK\n", CELLIAX_P_LOG);
          at_ack = AT_OK;
        }
      }
      if ((strcmp(p->line_array.result[i], "ERROR") == 0)) {
        if (option_debug > 1)
          DEBUGA_AT("got ERROR\n", CELLIAX_P_LOG);
        at_ack = AT_ERROR;
      }

      /* if we are reading an sms message from memory, put the line into the sms buffer if the line is not "OK" or "ERROR" */
      if (p->reading_sms_msg > 1 && at_ack == -1) {
        int c;
        char sms_body[16000];
        int err;

        if (strncmp(p->line_array.result[i], "+CMGR", 5) == 0) {    /* we are reading the "header" of an SMS */
          char content[512];
          char content2[512];

          memset(content, '\0', sizeof(content));

          int inside_comma = 0;
          int inside_quote = 0;
          int d = 0;

          for (c = 0; c < strlen(p->line_array.result[i]); c++) {
            if (p->line_array.result[i][c] == ','
                && p->line_array.result[i][c - 1] != '\\' && inside_quote == 0) {
              if (inside_comma) {
                inside_comma = 0;
                //NOTICA("inside_comma=%d, inside_quote=%d, we're at=%s\n", CELLIAX_P_LOG, inside_comma, inside_quote, &p->line_array.result[i][c]);
              } else {
                inside_comma = 1;
                //NOTICA("inside_comma=%d, inside_quote=%d, we're at=%s\n", CELLIAX_P_LOG, inside_comma, inside_quote, &p->line_array.result[i][c]);
              }
            }
            if (p->line_array.result[i][c] == '"'
                && p->line_array.result[i][c - 1] != '\\') {
              if (inside_quote) {
                inside_quote = 0;
                //ERRORA("END_CONTENT inside_comma=%d, inside_quote=%d, we're at=%s\n", CELLIAX_P_LOG, inside_comma, inside_quote, &p->line_array.result[i][c]);
                DEBUGA_AT("content=%s\n", CELLIAX_P_LOG, content);

                strncat(p->sms_message, "---",
                        ((sizeof(p->sms_message) - strlen(p->sms_message)) - 1));
                strncat(p->sms_message, content,
                        ((sizeof(p->sms_message) - strlen(p->sms_message)) - 1));
                strncat(p->sms_message, "|||",
                        ((sizeof(p->sms_message) - strlen(p->sms_message)) - 1));

                memset(content2, '\0', sizeof(content2));
                err = ucs2_to_utf8(p, content, content2, sizeof(content2));

                strncat(p->sms_message, "---",
                        ((sizeof(p->sms_message) - strlen(p->sms_message)) - 1));
                if (!err)
                  strncat(p->sms_message, content2,
                          ((sizeof(p->sms_message) - strlen(p->sms_message)) - 1));
                strncat(p->sms_message, "|||",
                        ((sizeof(p->sms_message) - strlen(p->sms_message)) - 1));
                memset(content, '\0', sizeof(content));
                d = 0;
              } else {
                inside_quote = 1;
                //WARNINGA("START_CONTENT inside_comma=%d, inside_quote=%d, we're at=%s\n", CELLIAX_P_LOG, inside_comma, inside_quote, &p->line_array.result[i][c]);
              }
            }
            if (inside_quote && p->line_array.result[i][c] != '"') {

              content[d] = p->line_array.result[i][c];
              d++;

            }

          }
        }                       //it was the +CMGR answer from the cellphone
        else {
          strncat(p->sms_message, "---",
                  ((sizeof(p->sms_message) - strlen(p->sms_message)) - 1));
          strncat(p->sms_message, p->line_array.result[i],
                  ((sizeof(p->sms_message) - strlen(p->sms_message)) - 1));
          strncat(p->sms_message, "|||",
                  ((sizeof(p->sms_message) - strlen(p->sms_message)) - 1));

          memset(sms_body, '\0', sizeof(sms_body));
          err = ucs2_to_utf8(p, p->line_array.result[i], sms_body, sizeof(sms_body));

          strncat(p->sms_message, "---",
                  ((sizeof(p->sms_message) - strlen(p->sms_message)) - 1));
          if (!err)
            strncat(p->sms_message, sms_body,
                    ((sizeof(p->sms_message) - strlen(p->sms_message)) - 1));
          strncat(p->sms_message, "|||",
                  ((sizeof(p->sms_message) - strlen(p->sms_message)) - 1));

          DEBUGA_AT("sms_message=%s\n", CELLIAX_P_LOG, p->sms_message);

        }                       //it was the UCS2 from cellphone

      }                         //we were reading the SMS

    }

    la_read = la_counter;

    if (look_for_ack && at_ack > -1)
      break;

    if (la_counter > AT_MESG_MAX_LINES) {
      ERRORA("Too many lines in result (>%d). Stopping reader.\n", CELLIAX_P_LOG,
             AT_MESG_MAX_LINES);
      at_ack = AT_ERROR;
      break;
    }
  }

  UNLOCKA(&p->controldev_lock);
  POPPA_UNLOCKA(&p->controldev_lock);
  if (select_err == -1) {
    ERRORA("select returned -1 on %s, setting controldev_dead, error was: %s\n",
           CELLIAX_P_LOG, p->controldevice_name, strerror(errno));
    p->controldev_dead = 1;
    close(p->controldevfd);
    if (p->owner)
      celliax_queue_control(p->owner, AST_CONTROL_HANGUP);
    return -1;
  }

  if (p->phone_callflow == CALLFLOW_CALL_INCOMING && p->call_incoming_time.tv_sec) {    //after three sec of CALLFLOW_CALL_INCOMING, we assume the phone is incapable of notifying RING (eg: motorola c350), so we try to answer
    char list_command[64];
    struct timeval call_incoming_timeout;
    gettimeofday(&call_incoming_timeout, NULL);
    call_incoming_timeout.tv_sec -= 3;
    DEBUGA_AT
      ("CALLFLOW_CALL_INCOMING call_incoming_time.tv_sec=%ld, call_incoming_timeout.tv_sec=%ld\n",
       CELLIAX_P_LOG, p->call_incoming_time.tv_sec, call_incoming_timeout.tv_sec);
    if (call_incoming_timeout.tv_sec > p->call_incoming_time.tv_sec) {

      p->call_incoming_time.tv_sec = 0;
      p->call_incoming_time.tv_usec = 0;
      DEBUGA_AT
        ("CALLFLOW_CALL_INCOMING call_incoming_time.tv_sec=%ld, call_incoming_timeout.tv_sec=%ld\n",
         CELLIAX_P_LOG, p->call_incoming_time.tv_sec, call_incoming_timeout.tv_sec);
      res = celliax_serial_write_AT_ack(p, "AT+CPBS=RC");
      if (res) {
        ERRORA
          ("AT+CPBS=RC (select memory of received calls) was not answered by the phone\n",
           CELLIAX_P_LOG);
      }
      p->phonebook_querying = 1;
      res = celliax_serial_write_AT_ack(p, "AT+CPBR=?");
      if (res) {
        ERRORA
          ("AT+CPBS=RC (select memory of received calls) was not answered by the phone\n",
           CELLIAX_P_LOG);
      }
      p->phonebook_querying = 0;
      sprintf(list_command, "AT+CPBR=%d,%d", p->phonebook_first_entry,
              p->phonebook_last_entry);
      p->phonebook_listing_received_calls = 1;
      res = celliax_serial_write_AT_expect_longtime(p, list_command, "OK");
      if (res) {
        WARNINGA("AT+CPBR=%d,%d failed, continue\n", CELLIAX_P_LOG,
                 p->phonebook_first_entry, p->phonebook_last_entry);
      }
      p->phonebook_listing_received_calls = 0;
    }
  }

  if (p->phone_callflow == CALLFLOW_INCOMING_RING) {
    struct timeval call_incoming_timeout;
    gettimeofday(&call_incoming_timeout, NULL);
    call_incoming_timeout.tv_sec -= 10;
    DEBUGA_AT
      ("CALLFLOW_CALL_INCOMING call_incoming_time.tv_sec=%ld, call_incoming_timeout.tv_sec=%ld\n",
       CELLIAX_P_LOG, p->call_incoming_time.tv_sec, call_incoming_timeout.tv_sec);
    if (call_incoming_timeout.tv_sec > p->ringtime.tv_sec) {
      ERRORA("Ringing stopped and I have not answered. Why?\n", CELLIAX_P_LOG);
      DEBUGA_AT
        ("CALLFLOW_CALL_INCOMING call_incoming_time.tv_sec=%ld, call_incoming_timeout.tv_sec=%ld\n",
         CELLIAX_P_LOG, p->call_incoming_time.tv_sec, call_incoming_timeout.tv_sec);
      if (p->owner) {
        celliax_queue_control(p->owner, AST_CONTROL_HANGUP);
        p->owner->hangupcause = AST_CAUSE_FAILURE;
      }
    }
  }
  p->line_array.elemcount = la_counter;
  //NOTICA (" OUTSIDE this celliax_serial_device %s \n", CELLIAX_P_LOG, p->controldevice_name);
  if (look_for_ack)
    return at_ack;
  else
    return 0;
}

int celliax_serial_write_AT(struct celliax_pvt *p, const char *data)
{
  int howmany;
  int i;
  int res;
  int count;

  howmany = strlen(data);

  for (i = 0; i < howmany; i++) {
    res = write(p->controldevfd, &data[i], 1);

    if (res != 1) {
      DEBUGA_AT("Error sending (%.1s): %d (%s)\n", CELLIAX_P_LOG, &data[i], res,
                strerror(errno));
      usleep(100000);
      for (count = 0; count < 10; count++) {
        res = write(p->controldevfd, &data[i], 1);
        if (res == 1) {
          DEBUGA_AT("Successfully RE-sent (%.1s): %d %d (%s)\n", CELLIAX_P_LOG, &data[i],
                    count, res, strerror(errno));
          break;
        } else
          DEBUGA_AT("Error RE-sending (%.1s): %d %d (%s)\n", CELLIAX_P_LOG, &data[i],
                    count, res, strerror(errno));
        usleep(100000);

      }
      if (res != 1) {
        ERRORA("Error RE-sending (%.1s): %d %d (%s)\n", CELLIAX_P_LOG, &data[i], count,
               res, strerror(errno));
        return -1;
      }
    }
    if (option_debug > 1)
      DEBUGA_AT("sent data... (%.1s)\n", CELLIAX_P_LOG, &data[i]);
    usleep(1000);               /* release the cpu */
  }

  res = write(p->controldevfd, "\r", 1);

  if (res != 1) {
    DEBUGA_AT("Error sending (carriage return): %d (%s)\n", CELLIAX_P_LOG, res,
              strerror(errno));
    usleep(100000);
    for (count = 0; count < 10; count++) {
      res = write(p->controldevfd, "\r", 1);

      if (res == 1) {
        DEBUGA_AT("Successfully RE-sent carriage return: %d %d (%s)\n", CELLIAX_P_LOG,
                  count, res, strerror(errno));
        break;
      } else
        DEBUGA_AT("Error RE-sending (carriage return): %d %d (%s)\n", CELLIAX_P_LOG,
                  count, res, strerror(errno));
      usleep(100000);

    }
    if (res != 1) {
      ERRORA("Error RE-sending (carriage return): %d %d (%s)\n", CELLIAX_P_LOG, count,
             res, strerror(errno));
      return -1;
    }
  }
  if (option_debug > 1)
    DEBUGA_AT("sent (carriage return)\n", CELLIAX_P_LOG);
  usleep(1000);                 /* release the cpu */

  return howmany;
}

int celliax_serial_write_AT_nocr(struct celliax_pvt *p, const char *data)
{
  int howmany;
  int i;
  int res;
  int count;

  howmany = strlen(data);

  for (i = 0; i < howmany; i++) {
    res = write(p->controldevfd, &data[i], 1);

    if (res != 1) {
      DEBUGA_AT("Error sending (%.1s): %d (%s)\n", CELLIAX_P_LOG, &data[i], res,
                strerror(errno));
      usleep(100000);
      for (count = 0; count < 10; count++) {
        res = write(p->controldevfd, &data[i], 1);
        if (res == 1)
          break;
        else
          DEBUGA_AT("Error RE-sending (%.1s): %d %d (%s)\n", CELLIAX_P_LOG, &data[i],
                    count, res, strerror(errno));
        usleep(100000);

      }
      if (res != 1) {
        ERRORA("Error RE-sending (%.1s): %d %d (%s)\n", CELLIAX_P_LOG, &data[i], count,
               res, strerror(errno));
        return -1;
      }
    }
    if (option_debug > 1)
      DEBUGA_AT("sent data... (%.1s)\n", CELLIAX_P_LOG, &data[i]);
    usleep(1000);               /* release the cpu */
  }

  usleep(1000);                 /* release the cpu */

  return howmany;
}

int celliax_serial_write_AT_noack(struct celliax_pvt *p, const char *data)
{

  if (option_debug > 1)
    DEBUGA_AT("celliax_serial_write_AT_noack: %s\n", CELLIAX_P_LOG, data);

  PUSHA_UNLOCKA(&p->controldev_lock);
  LOKKA(&p->controldev_lock);
  if (celliax_serial_write_AT(p, data) != strlen(data)) {

    ERRORA("Error sending data... (%s)\n", CELLIAX_P_LOG, strerror(errno));
    UNLOCKA(&p->controldev_lock);
    return -1;
  }
  UNLOCKA(&p->controldev_lock);
  POPPA_UNLOCKA(&p->controldev_lock);

  return 0;
}

int celliax_serial_write_AT_ack(struct celliax_pvt *p, const char *data)
{
  int at_result = AT_ERROR;

  PUSHA_UNLOCKA(&p->controldev_lock);
  LOKKA(&p->controldev_lock);
  if (option_debug > 1)
    DEBUGA_AT("sending: %s\n", CELLIAX_P_LOG, data);
  if (celliax_serial_write_AT(p, data) != strlen(data)) {
    ERRORA("Error sending data... (%s) \n", CELLIAX_P_LOG, strerror(errno));
    UNLOCKA(&p->controldev_lock);
    return -1;
  }

  at_result = celliax_serial_read_AT(p, 1, 500000, 2, NULL, 1); // 2.5 sec timeout
  UNLOCKA(&p->controldev_lock);
  POPPA_UNLOCKA(&p->controldev_lock);

  return at_result;

}

int celliax_serial_write_AT_ack_nocr_longtime(struct celliax_pvt *p, const char *data)
{
  int at_result = AT_ERROR;

  PUSHA_UNLOCKA(&p->controldev_lock);
  LOKKA(&p->controldev_lock);
  if (option_debug > 1)
    DEBUGA_AT("sending: %s\n", CELLIAX_P_LOG, data);
  if (celliax_serial_write_AT_nocr(p, data) != strlen(data)) {
    ERRORA("Error sending data... (%s) \n", CELLIAX_P_LOG, strerror(errno));
    UNLOCKA(&p->controldev_lock);
    return -1;
  }

  at_result = celliax_serial_read_AT(p, 1, 500000, 20, NULL, 1);    // 20.5 sec timeout
  UNLOCKA(&p->controldev_lock);
  POPPA_UNLOCKA(&p->controldev_lock);

  return at_result;

}

int celliax_serial_write_AT_expect1(struct celliax_pvt *p, const char *data,
                                    const char *expected_string, int expect_crlf,
                                    int seconds)
{
  int at_result = AT_ERROR;

  PUSHA_UNLOCKA(&p->controldev_lock);
  LOKKA(&p->controldev_lock);
  if (option_debug > 1)
    DEBUGA_AT("sending: %s, expecting: %s\n", CELLIAX_P_LOG, data, expected_string);
  if (celliax_serial_write_AT(p, data) != strlen(data)) {
    ERRORA("Error sending data... (%s) \n", CELLIAX_P_LOG, strerror(errno));
    UNLOCKA(&p->controldev_lock);
    return -1;
  }

  at_result = celliax_serial_read_AT(p, 1, 500000, seconds, expected_string, expect_crlf);  // 20.5 sec timeout, used for querying the SIM and sending SMSs
  UNLOCKA(&p->controldev_lock);
  POPPA_UNLOCKA(&p->controldev_lock);

  return at_result;

}

int celliax_serial_AT_expect(struct celliax_pvt *p, const char *expected_string,
                             int expect_crlf, int seconds)
{
  int at_result = AT_ERROR;

  PUSHA_UNLOCKA(&p->controldev_lock);
  LOKKA(&p->controldev_lock);
  if (option_debug > 1)
    DEBUGA_AT("expecting: %s\n", CELLIAX_P_LOG, expected_string);

  at_result = celliax_serial_read_AT(p, 1, 500000, seconds, expected_string, expect_crlf);  // 20.5 sec timeout, used for querying the SIM and sending SMSs
  UNLOCKA(&p->controldev_lock);
  POPPA_UNLOCKA(&p->controldev_lock);

  return at_result;

}

int celliax_serial_answer_AT(struct celliax_pvt *p)
{
  int res;

  res = celliax_serial_write_AT_expect(p, p->at_answer, p->at_answer_expect);
  if (res) {
    DEBUGA_AT
      ("at_answer command failed, command used: %s, expecting: %s, trying with AT+CKPD=\"S\"\n",
       CELLIAX_P_LOG, p->at_answer, p->at_answer_expect);

    res = celliax_serial_write_AT_ack(p, "AT+CKPD=\"S\"");
    if (res) {
      ERRORA("at_answer command failed, command used: 'AT+CKPD=\"S\"', giving up\n",
             CELLIAX_P_LOG);
      return -1;
    }
  }
  //p->interface_state = AST_STATE_UP;
  //p->phone_callflow = CALLFLOW_CALL_ACTIVE;
  DEBUGA_AT("AT: call answered\n", CELLIAX_P_LOG);
  return 0;
}

int celliax_serial_hangup_AT(struct celliax_pvt *p)
{
  int res;

  if (p->interface_state != AST_STATE_DOWN) {
    res = celliax_serial_write_AT_expect(p, p->at_hangup, p->at_hangup_expect);
    if (res) {
      DEBUGA_AT
        ("at_hangup command failed, command used: %s, trying to use AT+CKPD=\"EEE\"\n",
         CELLIAX_P_LOG, p->at_hangup);
      res = celliax_serial_write_AT_ack(p, "AT+CKPD=\"EEE\"");
      if (res) {
        ERRORA("at_hangup command failed, command used: 'AT+CKPD=\"EEE\"'\n",
               CELLIAX_P_LOG);
        return -1;
      }
    }
  }
  p->interface_state = AST_STATE_DOWN;
  p->phone_callflow = CALLFLOW_CALL_IDLE;
  return 0;
}

int celliax_serial_config_AT(struct celliax_pvt *p)
{
  int res;

/* initial_pause? */
  if (p->at_initial_pause) {
    DEBUGA_AT("sleeping for %d usec\n", CELLIAX_P_LOG, p->at_initial_pause);
    usleep(p->at_initial_pause);
  }

/* go until first empty preinit string, or last preinit string */
  while (1) {

    if (strlen(p->at_preinit_1)) {
      res = celliax_serial_write_AT_expect(p, p->at_preinit_1, p->at_preinit_1_expect);
      if (res) {
        DEBUGA_AT("%s does not get %s from the phone. Continuing.\n", CELLIAX_P_LOG,
                  p->at_preinit_1, p->at_preinit_1_expect);
      }
    } else {
      break;
    }

    if (strlen(p->at_preinit_2)) {
      res = celliax_serial_write_AT_expect(p, p->at_preinit_2, p->at_preinit_2_expect);
      if (res) {
        DEBUGA_AT("%s does not get %s from the phone. Continuing.\n", CELLIAX_P_LOG,
                  p->at_preinit_2, p->at_preinit_2_expect);
      }
    } else {
      break;
    }

    if (strlen(p->at_preinit_3)) {
      res = celliax_serial_write_AT_expect(p, p->at_preinit_3, p->at_preinit_3_expect);
      if (res) {
        DEBUGA_AT("%s does not get %s from the phone. Continuing.\n", CELLIAX_P_LOG,
                  p->at_preinit_3, p->at_preinit_3_expect);
      }
    } else {
      break;
    }

    if (strlen(p->at_preinit_4)) {
      res = celliax_serial_write_AT_expect(p, p->at_preinit_4, p->at_preinit_4_expect);
      if (res) {
        DEBUGA_AT("%s does not get %s from the phone. Continuing.\n", CELLIAX_P_LOG,
                  p->at_preinit_4, p->at_preinit_4_expect);
      }
    } else {
      break;
    }

    if (strlen(p->at_preinit_5)) {
      res = celliax_serial_write_AT_expect(p, p->at_preinit_5, p->at_preinit_5_expect);
      if (res) {
        DEBUGA_AT("%s does not get %s from the phone. Continuing.\n", CELLIAX_P_LOG,
                  p->at_preinit_5, p->at_preinit_5_expect);
      }
    } else {
      break;
    }

    break;
  }

/* after_preinit_pause? */
  if (p->at_after_preinit_pause) {
    DEBUGA_AT("sleeping for %d usec\n", CELLIAX_P_LOG, p->at_after_preinit_pause);
    usleep(p->at_after_preinit_pause);
  }

  /* phone, brother, art you alive? */
  res = celliax_serial_write_AT_ack(p, "AT");
  if (res) {
    ERRORA("no response to AT\n", CELLIAX_P_LOG);
    return -1;
  }
  /* for motorola, bring it back to "normal" mode if it happens to be in another mode */
  res = celliax_serial_write_AT_ack(p, "AT+mode=0");
  if (res) {
    DEBUGA_AT("AT+mode=0 does not get OK from the phone. If it is NOT Motorola,"
              " no problem.\n", CELLIAX_P_LOG);
  }
  usleep(50000);
  /* for motorola end */

  /* reset AT configuration to phone default */
  res = celliax_serial_write_AT_ack(p, "ATZ");
  if (res) {
    DEBUGA_AT("ATZ failed\n", CELLIAX_P_LOG);
  }

  /* disable AT command echo */
  res = celliax_serial_write_AT_ack(p, "ATE0");
  if (res) {
    DEBUGA_AT("ATE0 failed\n", CELLIAX_P_LOG);
  }

  /* disable extended error reporting */
  res = celliax_serial_write_AT_ack(p, "AT+CMEE=0");
  if (res) {
    DEBUGA_AT("AT+CMEE failed\n", CELLIAX_P_LOG);
  }

  /* various phone manufacturer identifier */
  char at_command[5];
  int i;
  for (i = 0; i < 10; i++) {
    memset(at_command, 0, sizeof(at_command));
    sprintf(at_command, "ATI%d", i);
    res = celliax_serial_write_AT_ack(p, at_command);
    if (res) {
      DEBUGA_AT("ATI%d command failed, continue\n", CELLIAX_P_LOG, i);
    }
  }

  /* phone manufacturer */
  res = celliax_serial_write_AT_ack(p, "AT+CGMI");
  if (res) {
    DEBUGA_AT("AT+CGMI failed\n", CELLIAX_P_LOG);
  }

  /* phone model */
  res = celliax_serial_write_AT_ack(p, "AT+CGMM");
  if (res) {
    DEBUGA_AT("AT+CGMM failed\n", CELLIAX_P_LOG);
  }

  res = celliax_serial_write_AT_ack(p, "AT+CGSN");
  if (res) {
    DEBUGA_AT("AT+CGSN failed\n", CELLIAX_P_LOG);
  }

/* this take a lot of time to complete on devices with slow serial link (eg.: 9600bps) */
#if 0
  /* ask for the list of supported AT commands, useful to implement new models and debugging */
  res = celliax_serial_write_AT_ack(p, "AT+CLAC");
  if (res) {
    DEBUGA_AT("AT+CLAC failed, continue\n", CELLIAX_P_LOG);
  }
#endif
  /* signal incoming SMS with a +CMTI unsolicited msg */
  res = celliax_serial_write_AT_ack(p, "AT+CNMI=3,1,0,0,0");
  if (res) {
    DEBUGA_AT("AT+CNMI=3,1,0,0,0 failed, continue\n", CELLIAX_P_LOG);
    p->sms_cnmi_not_supported = 1;
    p->celliax_serial_sync_period = 30;
  }
  /* what is the Message Center address (number) to which the SMS has to be sent? */
  res = celliax_serial_write_AT_ack(p, "AT+CSCA?");
  if (res) {
    DEBUGA_AT("AT+CSCA? failed, continue\n", CELLIAX_P_LOG);
  }
  /* what is the Message Format of SMSs? */
  res = celliax_serial_write_AT_ack(p, "AT+CMGF?");
  if (res) {
    DEBUGA_AT("AT+CMGF? failed, continue\n", CELLIAX_P_LOG);
  }
  res = celliax_serial_write_AT_ack(p, "AT+CMGF=1");    //TODO: support phones that only accept pdu mode
  if (res) {
    ERRORA("Error setting SMS sending mode to TEXT on the cellphone\n", CELLIAX_P_LOG);
    return RESULT_FAILURE;
  }
  /* what is the Charset of SMSs? */
  res = celliax_serial_write_AT_ack(p, "AT+CSCS?");
  if (res) {
    DEBUGA_AT("AT+CSCS? failed, continue\n", CELLIAX_P_LOG);
  }

  p->no_ucs2 = 0;
  res = celliax_serial_write_AT_ack(p, "AT+CSCS=\"UCS2\"");
  if (res) {
    WARNINGA
      ("AT+CSCS=\"UCS2\" (set TE messages to ucs2)  do not got OK from the phone, let's try with 'GSM'\n",
       CELLIAX_P_LOG);
    p->no_ucs2 = 1;
  }

  if (p->no_ucs2) {
    res = celliax_serial_write_AT_ack(p, "AT+CSCS=\"GSM\"");
    if (res) {
      WARNINGA("AT+CSCS=\"GSM\" (set TE messages to GSM)  do not got OK from the phone\n",
               CELLIAX_P_LOG);
    }
    //res = celliax_serial_write_AT_ack(p, "AT+CSMP=17,167,0,16"); //"flash", class 0  sms 7 bit
    res = celliax_serial_write_AT_ack(p, "AT+CSMP=17,167,0,0"); //normal, 7 bit message
    if (res) {
      WARNINGA("AT+CSMP do not got OK from the phone, continuing\n", CELLIAX_P_LOG);
    }
  } else {
    //res = celliax_serial_write_AT_ack(p, "AT+CSMP=17,167,0,20"); //"flash", class 0 sms 16 bit unicode
    res = celliax_serial_write_AT_ack(p, "AT+CSMP=17,167,0,8"); //unicode, 16 bit message
    if (res) {
      WARNINGA("AT+CSMP do not got OK from the phone, continuing\n", CELLIAX_P_LOG);
    }
  }

  /* is the unsolicited reporting of mobile equipment event supported? */
  res = celliax_serial_write_AT_ack(p, "AT+CMER=?");
  if (res) {
    DEBUGA_AT("AT+CMER=? failed, continue\n", CELLIAX_P_LOG);
  }
  /* request unsolicited reporting of mobile equipment indicators' events, to be screened by categories reported by +CIND=? */
  res = celliax_serial_write_AT_ack(p, "AT+CMER=3,0,0,1");
  if (res) {
    DEBUGA_AT("AT+CMER=? failed, continue\n", CELLIAX_P_LOG);
  }

  /* is the solicited reporting of mobile equipment indications supported? */

  res = celliax_serial_write_AT_ack(p, "AT+CIND=?");
  if (res) {
    DEBUGA_AT("AT+CIND=? failed, continue\n", CELLIAX_P_LOG);
  }

  /* is the unsolicited reporting of call monitoring supported? sony-ericsson specific */
  res = celliax_serial_write_AT_ack(p, "AT*ECAM=?");
  if (res) {
    DEBUGA_AT("AT*ECAM=? failed, continue\n", CELLIAX_P_LOG);
  }
  /* enable the unsolicited reporting of call monitoring. sony-ericsson specific */
  res = celliax_serial_write_AT_ack(p, "AT*ECAM=1");
  if (res) {
    DEBUGA_AT("AT*ECAM=1 failed, continue\n", CELLIAX_P_LOG);
    p->at_has_ecam = 0;
  } else {
    p->at_has_ecam = 1;
  }

  /* disable unsolicited signaling of call list */
  res = celliax_serial_write_AT_ack(p, "AT+CLCC=0");
  if (res) {
    DEBUGA_AT("AT+CLCC=0 failed, continue\n", CELLIAX_P_LOG);
    p->at_has_clcc = 0;
  } else {
    p->at_has_clcc = 1;
  }

  /* give unsolicited caller id when incoming call */
  res = celliax_serial_write_AT_ack(p, "AT+CLIP=1");
  if (res) {
    DEBUGA_AT("AT+CLIP failed, continue\n", CELLIAX_P_LOG);
  }
  /* for motorola */
  res = celliax_serial_write_AT_ack(p, "AT+MCST=1");    /* motorola call control codes
                                                           (to know when call is disconnected (they
                                                           don't give you "no carrier") */
  if (res) {
    DEBUGA_AT("AT+MCST=1 does not get OK from the phone. If it is NOT Motorola,"
              " no problem.\n", CELLIAX_P_LOG);
  }
  /* for motorola end */

/* go until first empty postinit string, or last postinit string */
  while (1) {

    if (strlen(p->at_postinit_1)) {
      res = celliax_serial_write_AT_expect(p, p->at_postinit_1, p->at_postinit_1_expect);
      if (res) {
        DEBUGA_AT("%s does not get %s from the phone. Continuing.\n", CELLIAX_P_LOG,
                  p->at_postinit_1, p->at_postinit_1_expect);
      }
    } else {
      break;
    }

    if (strlen(p->at_postinit_2)) {
      res = celliax_serial_write_AT_expect(p, p->at_postinit_2, p->at_postinit_2_expect);
      if (res) {
        DEBUGA_AT("%s does not get %s from the phone. Continuing.\n", CELLIAX_P_LOG,
                  p->at_postinit_2, p->at_postinit_2_expect);
      }
    } else {
      break;
    }

    if (strlen(p->at_postinit_3)) {
      res = celliax_serial_write_AT_expect(p, p->at_postinit_3, p->at_postinit_3_expect);
      if (res) {
        DEBUGA_AT("%s does not get %s from the phone. Continuing.\n", CELLIAX_P_LOG,
                  p->at_postinit_3, p->at_postinit_3_expect);
      }
    } else {
      break;
    }

    if (strlen(p->at_postinit_4)) {
      res = celliax_serial_write_AT_expect(p, p->at_postinit_4, p->at_postinit_4_expect);
      if (res) {
        DEBUGA_AT("%s does not get %s from the phone. Continuing.\n", CELLIAX_P_LOG,
                  p->at_postinit_4, p->at_postinit_4_expect);
      }
    } else {
      break;
    }

    if (strlen(p->at_postinit_5)) {
      res = celliax_serial_write_AT_expect(p, p->at_postinit_5, p->at_postinit_5_expect);
      if (res) {
        DEBUGA_AT("%s does not get %s from the phone. Continuing.\n", CELLIAX_P_LOG,
                  p->at_postinit_5, p->at_postinit_5_expect);
      }
    } else {
      break;
    }

    break;
  }

  return 0;
}

int celliax_serial_call_AT(struct celliax_pvt *p, char *dstr)
{
  int res;
  char at_command[256];

  if (option_debug)
    DEBUGA_PBX("Dialing %s\n", CELLIAX_P_LOG, dstr);
  memset(at_command, 0, sizeof(at_command));
  p->phone_callflow = CALLFLOW_CALL_DIALING;
  p->interface_state = AST_STATE_DIALING;
  ast_uri_decode(dstr);
  size_t fixdstr = strspn(dstr, AST_DIGIT_ANYDIG);
  if (fixdstr == 0) {
    ERRORA("dial command failed because of invalid dial number. dial string was: %s\n",
           CELLIAX_P_LOG, dstr);
    return -1;
  }
  dstr[fixdstr] = '\0';
  sprintf(at_command, "%s%s%s", p->at_dial_pre_number, dstr, p->at_dial_post_number);
  res = celliax_serial_write_AT_expect(p, at_command, p->at_dial_expect);
  if (res) {
    ERRORA("dial command failed, dial string was: %s\n", CELLIAX_P_LOG, at_command);
    return -1;
  }
  // jet - early audio
  if (p->at_early_audio) {
    ast_queue_control(p->owner, AST_CONTROL_ANSWER);
  }

  return 0;
}

int celliax_console_at(int fd, int argc, char *argv[])
{
  struct celliax_pvt *p = celliax_console_find_desc(celliax_console_active);
  char at_cmd[1024];
  int i, a, c;

  if (argc == 1)
    return RESULT_SHOWUSAGE;
  if (!p) {
    ast_cli(fd,
            "No \"current\" console for celliax_at, please enter 'help celliax_console'\n");
    return RESULT_SUCCESS;
  }
  if (p->controldevprotocol != PROTOCOL_AT) {
    ast_cli(fd,
            "The \"current\" console is not connected to an 'AT modem' (cellphone)\n");
    return RESULT_SUCCESS;
  }

  memset(at_cmd, 0, sizeof(at_cmd));
  c = 0;
  for (i = 1; i < argc; i++) {
    for (a = 0; a < strlen(argv[i]); a++) {
      at_cmd[c] = argv[i][a];
      c++;
      if (c == 1022)
        break;
    }
    if (i != argc - 1) {
      at_cmd[c] = ' ';
      c++;
    }
    if (c == 1023)
      break;
  }
  celliax_serial_write_AT_noack(p, at_cmd);
  return RESULT_SUCCESS;
}

#ifdef ASTERISK_VERSION_1_2
int celliax_manager_sendsms(struct mansession *s, struct message *m)
#endif //ASTERISK_VERSION_1_2
#ifdef ASTERISK_VERSION_1_4
int celliax_manager_sendsms(struct mansession *s, const struct message *m)
#endif //ASTERISK_VERSION_1_4
{
  int ret;
  char command[512];
  const char *interfacename = astman_get_header(m, "Interface");
  const char *destinationnumber = astman_get_header(m, "Number");
  const char *text = astman_get_header(m, "Text");
  const char *action_id = astman_get_header(m, "ActionID");

  if (ast_strlen_zero(interfacename)) {
    astman_send_error(s, m, "Interface: missing.\n");
    return 0;
  }
  if (ast_strlen_zero(destinationnumber)) {
    astman_send_error(s, m, "Number: missing.\n");
    return 0;
  }
  if (ast_strlen_zero(text)) {
    astman_send_error(s, m, "Text: missing.\n");
    return 0;
  }
  if (ast_strlen_zero(action_id)) {
    astman_send_error(s, m, "ActionID: missing.\n");
    return 0;
  }

  memset(command, 0, sizeof(command));

  sprintf(command, "%s/%s|%s|", interfacename, destinationnumber, text);

  ret = celliax_sendsms(NULL, (void *) &command);

#ifndef ASTERISK_VERSION_1_4
  if (!ret) {
    ast_cli(s->fd, "Response: Success\r\n");
    if (!ast_strlen_zero(action_id))
      ast_cli(s->fd, "ActionID: %s\r\n", action_id);
    ast_cli(s->fd, "\r\n");
    return RESULT_SUCCESS;
  } else {
    ast_cli(s->fd, "Response: Error\r\n");
    if (!ast_strlen_zero(action_id))
      ast_cli(s->fd, "ActionID: %s\r\n", action_id);
    ast_cli(s->fd, "Message: celliax_manager_sendsms failed\r\n");
    ast_cli(s->fd, "\r\n");
    return 0;
  }
#else /* ASTERISK_VERSION_1_4 */
  if (!ret) {
    astman_append(s, "Response: Success\r\n");
    if (!ast_strlen_zero(action_id))
      astman_append(s, "ActionID: %s\r\n", action_id);
    astman_append(s, "\r\n");
    return RESULT_SUCCESS;
  } else {
    astman_append(s, "Response: Error\r\n");
    if (!ast_strlen_zero(action_id))
      astman_append(s, "ActionID: %s\r\n", action_id);
    astman_append(s, "Message: celliax_manager_sendsms failed\r\n");
    astman_append(s, "\r\n");
    return 0;
  }
#endif /* ASTERISK_VERSION_1_4 */

  return RESULT_SUCCESS;        //never reached
}

int ucs2_to_utf8(struct celliax_pvt *p, char *ucs2_in, char *utf8_out,
                 size_t outbytesleft)
{
  char converted[16000];
  iconv_t iconv_format;
  int iconv_res;
  char *outbuf;
  char *inbuf;
  size_t inbytesleft;
  int c;
  char stringa[5];
  double hexnum;
  int i = 0;

  memset(converted, '\0', sizeof(converted));

  DEBUGA_AT("ucs2_in=%s\n", CELLIAX_P_LOG, ucs2_in);
  /* cicopet */
  for (c = 0; c < strlen(ucs2_in); c++) {
    sprintf(stringa, "0x%c%c", ucs2_in[c], ucs2_in[c + 1]);
    c++;
    hexnum = strtod(stringa, NULL);
    converted[i] = hexnum;
    i++;
  }

  outbuf = utf8_out;
  inbuf = converted;

  iconv_format = iconv_open("UTF8", "UCS-2BE");
  if (iconv_format == (iconv_t) - 1) {
    ERRORA("error: %s\n", CELLIAX_P_LOG, strerror(errno));
    return -1;
  }

  inbytesleft = i;
  iconv_res = iconv(iconv_format, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
  if (iconv_res == (size_t) - 1) {
    DEBUGA_AT("ciao in=%s, inleft=%d, out=%s, outleft=%d, converted=%s, utf8_out=%s\n",
              CELLIAX_P_LOG, inbuf, inbytesleft, outbuf, outbytesleft, converted,
              utf8_out);
    DEBUGA_AT("error: %s %d\n", CELLIAX_P_LOG, strerror(errno), errno);
    return -1;
  }
  DEBUGA_AT
    ("iconv_res=%d,  in=%s, inleft=%d, out=%s, outleft=%d, converted=%s, utf8_out=%s\n",
     CELLIAX_P_LOG, iconv_res, inbuf, inbytesleft, outbuf, outbytesleft, converted,
     utf8_out);
  iconv_close(iconv_format);

  return 0;
}

int utf_to_ucs2(struct celliax_pvt *p, char *utf_in, size_t inbytesleft, char *ucs2_out,
                size_t outbytesleft)
{
  /* cicopet */
  iconv_t iconv_format;
  int iconv_res;
  char *outbuf;
  char *inbuf;
  char converted[16000];
  int i;
  char stringa[16];
  char stringa2[16];

  memset(converted, '\0', sizeof(converted));

  outbuf = converted;
  inbuf = utf_in;

  iconv_format = iconv_open("UCS-2BE", "UTF8");
  if (iconv_format == (iconv_t) - 1) {
    ERRORA("error: %s\n", CELLIAX_P_LOG, strerror(errno));
    return -1;
  }
  outbytesleft = 16000;

  DEBUGA_AT("in=%s, inleft=%d, out=%s, outleft=%d, utf_in=%s, converted=%s\n",
            CELLIAX_P_LOG, inbuf, inbytesleft, outbuf, outbytesleft, utf_in, converted);
  iconv_res = iconv(iconv_format, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
  if (iconv_res == (size_t) - 1) {
    ERRORA("error: %s %d\n", CELLIAX_P_LOG, strerror(errno), errno);
    return -1;
  }
  DEBUGA_AT
    ("iconv_res=%d,  in=%s, inleft=%d, out=%s, outleft=%d, utf_in=%s, converted=%s\n",
     CELLIAX_P_LOG, iconv_res, inbuf, inbytesleft, outbuf, outbytesleft, utf_in,
     converted);
  iconv_close(iconv_format);

  for (i = 0; i < 16000 - outbytesleft; i++) {
    memset(stringa, '\0', sizeof(stringa));
    memset(stringa2, '\0', sizeof(stringa2));
    sprintf(stringa, "%02X", converted[i]);
    DEBUGA_AT("character is |%02X|\n", CELLIAX_P_LOG, converted[i]);
    stringa2[0] = stringa[strlen(stringa) - 2];
    stringa2[1] = stringa[strlen(stringa) - 1];
    strncat(ucs2_out, stringa2, ((outbytesleft - strlen(ucs2_out)) - 1));   //add the received line to the buffer
    DEBUGA_AT("stringa=%s, stringa2=%s, ucs2_out=%s\n", CELLIAX_P_LOG, stringa, stringa2,
              ucs2_out);
  }
  return 0;
}

int celliax_sendsms(struct ast_channel *c, void *data)
{
  char *idest = data;
  char rdest[256];
  struct celliax_pvt *p = NULL;
  char *device;
  char *dest;
  char *text;
  char *stringp = NULL;
  int found = 0;
  int failed = 0;

  strncpy(rdest, idest, sizeof(rdest) - 1);
  ast_log(LOG_DEBUG, "CelliaxSendsms: %s\n", rdest);
  ast_log(LOG_DEBUG, "START\n");
  /* we can use celliax_request to get the channel, but celliax_request would look for onowned channels, and probably we can send SMSs while a call is ongoing
   *
   */

  stringp = rdest;
  device = strsep(&stringp, "/");
  dest = strsep(&stringp, "|");
  text = strsep(&stringp, "|");

  if (!device) {
    ast_log(LOG_ERROR,
            "CelliaxSendsms app do not recognize '%s'. Requires a destination with slashes (interfacename/destinationnumber, TEXT)\n",
            idest);
    return -1;
  }

  if (!dest) {
    ast_log(LOG_ERROR,
            "CelliaxSendsms app do not recognize '%s'. Requires a destination with slashes (interfacename/destinationnumber, TEXT)\n",
            idest);
    return -1;
  }

  if (!text) {
    ast_log(LOG_ERROR,
            "CelliaxSendsms app do not recognize '%s'. Requires a destination with slashes (interfacename/destinationnumber, TEXT)\n",
            idest);
    return -1;
  }

  ast_log(LOG_DEBUG, "interfacename:%s, destinationnumber:%s, text:%s\n", device, dest,
          text);

  /* lock the interfaces' list */
  LOKKA(&celliax_iflock);
  /* make a pointer to the first interface in the interfaces list */
  p = celliax_iflist;
  /* Search for the requested interface and verify if is unowned */
  //TODO implement groups a la chan_zap
  while (p) {
    size_t length = strlen(p->name);
    /* is this the requested interface? */
    if (strncmp(device, p->name, length) == 0) {
      /* this is the requested interface! */
      if (option_debug)
        DEBUGA_AT("FOUND! interfacename:%s, destinationnumber:%s, text:%s, p->name=%s\n",
                  CELLIAX_P_LOG, device, dest, text, p->name);
      found = 1;
      break;

    }
    /* not yet found, next please */
    p = p->next;
  }
  /* unlock the interfaces' list */
  UNLOCKA(&celliax_iflock);

  if (!found) {
    ast_log(LOG_ERROR, "Interface '%s' requested by CelliaxSendsms NOT FOUND\n", device);
    return RESULT_FAILURE;
  }

  if (p->controldevprotocol != PROTOCOL_AT) {
    ERRORA("CelliaxSendsms supports only AT command cellphones at the moment :-( !\n",
           CELLIAX_P_LOG);
    return RESULT_FAILURE;
  }

  if (p->controldevprotocol == PROTOCOL_AT) {
    int err = 0;
    char smscommand[16000];
    memset(smscommand, '\0', sizeof(smscommand));

    PUSHA_UNLOCKA(&p->controldev_lock);
    LOKKA(&p->controldev_lock);

    if (p->no_ucs2) {
      sprintf(smscommand, "AT+CMGS=\"%s\"", dest);  //TODO: support phones that only accept pdu mode
    } else {
      char dest2[1048];

          err = celliax_serial_write_AT_ack(p, "AT+CSCS=\"UCS2\"");
          if (err) {
            ERRORA
              ("AT+CSCS=\"UCS2\" (set TE messages to ucs2)  do not got OK from the phone\n",
               CELLIAX_P_LOG);
          }

      memset(dest2, '\0', sizeof(dest2));
      utf_to_ucs2(p, dest, strlen(dest), dest2, sizeof(dest2));
      sprintf(smscommand, "AT+CMGS=\"%s\"", dest2); //TODO: support phones that only accept pdu mode
    }
    //TODO: support phones that only accept pdu mode
    //TODO would be better to lock controldev here
    err = celliax_serial_write_AT_noack(p, smscommand);
    if (err) {
      ERRORA("Error sending SMS\n", CELLIAX_P_LOG);
      failed = 1;
      goto uscita;
    }
    err = celliax_serial_AT_expect(p, "> ", 0, 1);  // wait 1.5s for the prompt, no  crlf
#if 1
    if (err) {
      DEBUGA_AT
        ("Error or timeout getting prompt '> ' for sending sms directly to the remote party. BTW, seems that we cannot do that with Motorola c350, so we'll write to cellphone memory, then send from memory\n",
         CELLIAX_P_LOG);

      err = celliax_serial_write_AT_ack(p, "ATE1"); //motorola (at least c350) do not echo the '>' prompt when in ATE0... go figure!!!!
      if (err) {
        ERRORA("Error activating echo from modem\n", CELLIAX_P_LOG);
      }
      p->at_cmgw[0] = '\0';
      sprintf(smscommand, "AT+CMGW=\"%s\"", dest);  //TODO: support phones that only accept pdu mode
      err = celliax_serial_write_AT_noack(p, smscommand);
      if (err) {
        ERRORA("Error writing SMS destination to the cellphone memory\n", CELLIAX_P_LOG);
        failed = 1;
        goto uscita;
      }
      err = celliax_serial_AT_expect(p, "> ", 0, 1);    // wait 1.5s for the prompt, no  crlf
      if (err) {
        ERRORA
          ("Error or timeout getting prompt '> ' for writing sms text in cellphone memory\n",
           CELLIAX_P_LOG);
        failed = 1;
        goto uscita;
      }
    }
#endif

    //sprintf(text,"ciao 123 belè новости לק ראת ﺎﻠﺠﻤﻋﺓ 人大"); //let's test the beauty of utf
    memset(smscommand, '\0', sizeof(smscommand));
    if (p->no_ucs2) {
      sprintf(smscommand, "%s", text);
    } else {
      utf_to_ucs2(p, text, strlen(text), smscommand, sizeof(smscommand));
    }

    smscommand[strlen(smscommand)] = 0x1A;
    DEBUGA_AT("smscommand len is: %d, text is:|||%s|||\n", CELLIAX_P_LOG,
              strlen(smscommand), smscommand);

    err = celliax_serial_write_AT_ack_nocr_longtime(p, smscommand);
    //TODO would be better to unlock controldev here
    if (err) {
      ERRORA("Error writing SMS text to the cellphone memory\n", CELLIAX_P_LOG);
      //return RESULT_FAILURE;
      failed = 1;
      goto uscita;
    }
    if (p->at_cmgw[0]) {
      sprintf(smscommand, "AT+CMSS=%s", p->at_cmgw);
      err = celliax_serial_write_AT_expect_longtime(p, smscommand, "OK");
      if (err) {
        ERRORA("Error sending SMS from the cellphone memory\n", CELLIAX_P_LOG);
        //return RESULT_FAILURE;
        failed = 1;
        goto uscita;
      }

      err = celliax_serial_write_AT_ack(p, "ATE0"); //motorola (at least c350) do not echo the '>' prompt when in ATE0... go figure!!!!
      if (err) {
        ERRORA("Error de-activating echo from modem\n", CELLIAX_P_LOG);
      }
    }
  uscita:
    usleep(1000);

    if (p->at_cmgw[0]) {

      /* let's see what we've sent, just for check TODO: Motorola it's not reliable! Motorola c350 tells that all was sent, but is not true! It just sends how much it fits into one SMS FIXME: need an algorithm to calculate how many ucs2 chars fits into an SMS. It make difference based, probably, on the GSM alphabet translation, or so */
      sprintf(smscommand, "AT+CMGR=%s", p->at_cmgw);
      err = celliax_serial_write_AT_ack(p, smscommand);
      if (err) {
        ERRORA("Error reading SMS back from the cellphone memory\n", CELLIAX_P_LOG);
      }

      /* let's delete from cellphone memory what we've sent */
      sprintf(smscommand, "AT+CMGD=%s", p->at_cmgw);
      err = celliax_serial_write_AT_ack(p, smscommand);
      if (err) {
        ERRORA("Error deleting SMS from the cellphone memory\n", CELLIAX_P_LOG);
      }

      p->at_cmgw[0] = '\0';
    }
    //usleep(500000);             //.5 secs
    UNLOCKA(&p->controldev_lock);
    POPPA_UNLOCKA(&p->controldev_lock);
  }

  ast_log(LOG_DEBUG, "FINISH\n");
  if (failed)
    return -1;
  else
    return RESULT_SUCCESS;
}

#ifdef CELLIAX_DIR
/* For simplicity, I'm keeping the format compatible with the voicemail config,
   but i'm open to suggestions for isolating it */
#define CELLIAX_DIR_CONFIG "directoriax.conf"

/* How many digits to read in */
#define CELLIAX_DIR_NUMDIGITS 3

struct ast_config *celliax_dir_realtime(char *context)
{
  //TODO: all the realtime stuff has to be re-made
  struct ast_config *cfg;
  struct celliax_pvt *p = NULL;
#ifdef ASTERISK_VERSION_1_6_0
  struct ast_flags config_flags = { 0 };
#endif /* ASTERISK_VERSION_1_6_0 */

  /* Load flat file config. */
#ifdef ASTERISK_VERSION_1_6_0
  cfg = ast_config_load(CELLIAX_DIR_CONFIG, config_flags);
#else
  cfg = ast_config_load(CELLIAX_DIR_CONFIG);
#endif /* ASTERISK_VERSION_1_6_0 */

  if (!cfg) {
    /* Loading config failed. */
    WARNINGA
      ("Loading directoriax.conf config file failed. It's not necessary, continuing.\n",
       CELLIAX_P_LOG);
    return NULL;
  }
  return cfg;
}

static char *celliax_dir_convert(char *lastname)
{
  char *tmp;
  int lcount = 0;
  tmp = malloc(CELLIAX_DIR_NUMDIGITS + 1);
  if (tmp) {
    while ((*lastname > 32) && lcount < CELLIAX_DIR_NUMDIGITS) {
      switch (toupper(*lastname)) {
      case '1':
        tmp[lcount++] = '1';
        break;
      case '2':
      case 'A':
      case 'B':
      case 'C':
        tmp[lcount++] = '2';
        break;
      case '3':
      case 'D':
      case 'E':
      case 'F':
        tmp[lcount++] = '3';
        break;
      case '4':
      case 'G':
      case 'H':
      case 'I':
        tmp[lcount++] = '4';
        break;
      case '5':
      case 'J':
      case 'K':
      case 'L':
        tmp[lcount++] = '5';
        break;
      case '6':
      case 'M':
      case 'N':
      case 'O':
        tmp[lcount++] = '6';
        break;
      case '7':
      case 'P':
      case 'Q':
      case 'R':
      case 'S':
        tmp[lcount++] = '7';
        break;
      case '8':
      case 'T':
      case 'U':
      case 'V':
        tmp[lcount++] = '8';
        break;
      case '9':
      case 'W':
      case 'X':
      case 'Y':
      case 'Z':
        tmp[lcount++] = '9';
        break;
      }
      lastname++;
    }
    tmp[lcount] = '\0';
  }
  return tmp;
}

int celliax_console_celliax_dir_export(int fd, int argc, char *argv[])
{
  struct ast_config *cfg;

  struct ast_variable *v;
  char *start, *pos, *stringp, *space, *options = NULL, *conv = NULL;
  struct celliax_pvt *p = celliax_console_find_desc(celliax_console_active);
  char *context = "default";
  char *s;
  char *var, *value;
  int fromcell = 0;
  int fromskype = 0;
  char name[256] = "";
  char phonebook_direct_calling_ext[7] = "";
  char write_entry_command[256] = "";
  char entry_number[256] = "";
  char entry_text[256] = "";
  char final_entry_text[256] = "";
  int res;
  int tocell = 0;
#ifdef CELLIAX_LIBCSV
  int tocsv = 0;
  int tovcf = 0;
#endif /* CELLIAX_LIBCSV */

  if (argc < 3 || argc > 4)
    return RESULT_SHOWUSAGE;
  if (!p) {
    ast_cli(fd, "No \"current\" console ???, please enter 'help celliax_console'\n");
    return RESULT_SUCCESS;
  }

  if (!strcasecmp(argv[1], "tocell"))
    tocell = 1;
#ifdef CELLIAX_LIBCSV
  else if (!strcasecmp(argv[1], "tocsv"))
    tocsv = 1;
  else if (!strcasecmp(argv[1], "tovcf"))
    tovcf = 1;
#endif /* CELLIAX_LIBCSV */
  else {
    ast_cli(fd,
#ifdef CELLIAX_LIBCSV
            "\n\nYou have neither specified 'tocell' nor 'tocsv'\n\n");
#else /* CELLIAX_LIBCSV */
            "\n\nYou have not specified 'tocell'\n\n");
#endif /* CELLIAX_LIBCSV */
    return RESULT_SHOWUSAGE;
  }
  if (tocell)
    if (p->controldevprotocol != PROTOCOL_AT) {
      ast_cli(fd,
              "Exporting to the cellphone phonebook is currently supported only on \"AT\" cellphones :( !\n");
      return RESULT_SUCCESS;
    }
#ifdef CELLIAX_LIBCSV
  if (tocsv || tovcf)
    if (argc != 4) {
      ast_cli(fd, "\n\nYou have to specify a filename with 'tocsv'\n\n");
      return RESULT_SHOWUSAGE;
    }
#endif /* CELLIAX_LIBCSV */

  if (option_debug)
    NOTICA("celliax_cellphonenumber is: %s\n", CELLIAX_P_LOG, argv[2]);

#ifdef CELLIAX_LIBCSV
  if (tocsv) {
    if (option_debug)
      NOTICA("filename is: %s\n", CELLIAX_P_LOG, argv[3]);
    //ast_cli(fd, "\n\nnot yet implemented :P \n");
    //return RESULT_SUCCESS;
  }
  if (tovcf) {
    if (option_debug)
      NOTICA("filename is: %s\n", CELLIAX_P_LOG, argv[3]);
    ast_cli(fd, "\n\nnot yet implemented :P \n");
    return RESULT_SUCCESS;
  }
#endif /* CELLIAX_LIBCSV */

  cfg = celliax_dir_realtime(context);
  if (!cfg) {
    return -1;
  }

  if (tocell) {
    /* which phonebook to use, use the SIM  */
    res = celliax_serial_write_AT_ack(p, "AT+CPBS=SM");
    if (res) {
      WARNINGA("AT+CPBS=SM failed, continue\n", CELLIAX_P_LOG);
    }
    /* which phonebook to use, trying to use phone, not SIM  */
    res = celliax_serial_write_AT_ack(p, "AT+CPBS=ME");
    if (res) {
      WARNINGA("AT+CPBS=ME failed, continue\n", CELLIAX_P_LOG);
    }
    /* retrieve the fields lenght in the selected phonebook  */
    p->phonebook_querying = 1;
    res = celliax_serial_write_AT_ack(p, "AT+CPBR=?");
    if (res) {
      WARNINGA("AT+CPBR=? failed, continue\n", CELLIAX_P_LOG);
    }
    p->phonebook_querying = 0;

    v = ast_variable_browse(cfg, context);
    /* Find all candidate extensions */
    while (v) {
      /* Find a candidate extension */
      start = strdup(v->value);
      if (strcasestr(start, "fromcell=yes")) {
        fromcell = 1;
        fromskype = 0;

      }
      if (strcasestr(start, "fromskype=yes")) {
        fromcell = 0;
        fromskype = 1;

      }

      if (start && !strcasestr(start, "hidefromdir=yes")) {
        memset(name, 0, sizeof(name));
        memset(phonebook_direct_calling_ext, 0, sizeof(phonebook_direct_calling_ext));
        memset(write_entry_command, 0, sizeof(write_entry_command));
        memset(entry_number, 0, sizeof(entry_number));
        memset(entry_text, 0, sizeof(entry_text));
        memset(final_entry_text, 0, sizeof(final_entry_text));

        DEBUGA_AT("v->name=%s\n", CELLIAX_P_LOG, v->name);
        DEBUGA_AT("v->value=%s\n", CELLIAX_P_LOG, v->value);

        stringp = start;
        strsep(&stringp, ",");
        pos = strsep(&stringp, ",");
        if (pos) {
          ast_copy_string(name, pos, sizeof(name));
          if (strchr(pos, ' ')) {
            space = strchr(pos, ' ');
            *space = '\0';
          }
          if (pos) {
            conv = celliax_dir_convert(pos);
            DEBUGA_AT("<pos=>%s<conv=>%s<\n", CELLIAX_P_LOG, pos, conv);

            options = strdup(v->value);
            strsep(&options, ",");
            strsep(&options, ",");
            strsep(&options, ",");
            strsep(&options, ",");
            DEBUGA_AT("options=%s\n", CELLIAX_P_LOG, options);

            while ((s = strsep(&options, "|"))) {
              value = s;
              if ((var = strsep(&value, "=")) && value) {
                DEBUGA_AT("var=%s value=%s\n", CELLIAX_P_LOG, var, value);
                if (!strcmp(var, "phonebook_direct_calling_ext"))
                  strncpy(phonebook_direct_calling_ext, value, 6);
              }
            }

            res =
              snprintf(entry_number, p->phonebook_number_lenght + 1, "%s%s%d%s%s",
                       argv[2], "p", p->celliax_dir_prefix, "p",
                       phonebook_direct_calling_ext);
            if (res == (p->phonebook_number_lenght + 1)
                || res > (p->phonebook_number_lenght + 1)) {
              ERRORA("entry_number truncated, was: '%s%s%d%s%s', now is: '%s'\n",
                     CELLIAX_P_LOG, argv[2], "p", p->celliax_dir_prefix, "p",
                     phonebook_direct_calling_ext, entry_number);
              //FIXME: abort ???

            }

            res = snprintf(final_entry_text, p->phonebook_text_lenght + 1, "%s", name); //FIXME result not checked

            res =
              snprintf(write_entry_command, sizeof(write_entry_command) - 1,
                       "AT+CPBW=,\"%s\",,\"%s\"", entry_number, final_entry_text);
            if (res == (sizeof(write_entry_command) - 1)
                || res > (sizeof(write_entry_command) - 1)) {
              WARNINGA
                ("write_entry_command truncated, was supposed: 'AT+CPBW=,\"%s\",,\"%s\"', now is: '%s'\n",
                 CELLIAX_P_LOG, entry_number, final_entry_text, write_entry_command);
            }
            //if (option_debug)
            NOTICA("%s\n", CELLIAX_P_LOG, write_entry_command);
          }
        }
        if (conv)
          free(conv);
        if (start)
          free(start);
        if (options)
          free(options);
      }
      v = v->next;
    }
  }
#ifdef CELLIAX_LIBCSV
  if (tocsv) {

    v = ast_variable_browse(cfg, context);
    /* Find all candidate extensions */
    while (v) {
      /* Find a candidate extension */
      start = strdup(v->value);
      if (strcasestr(start, "fromcell=yes")) {
        fromcell = 1;
        fromskype = 0;

      }
      if (strcasestr(start, "fromskype=yes")) {
        fromcell = 0;
        fromskype = 1;

      }

      if (start && !strcasestr(start, "hidefromdir=yes")) {
        memset(name, 0, sizeof(name));
        memset(phonebook_direct_calling_ext, 0, sizeof(phonebook_direct_calling_ext));
        memset(write_entry_command, 0, sizeof(write_entry_command));
        memset(entry_number, 0, sizeof(entry_number));
        memset(entry_text, 0, sizeof(entry_text));
        memset(final_entry_text, 0, sizeof(final_entry_text));

        DEBUGA_AT("v->name=%s\n", CELLIAX_P_LOG, v->name);
        DEBUGA_AT("v->value=%s\n", CELLIAX_P_LOG, v->value);

        stringp = start;
        strsep(&stringp, ",");
        pos = strsep(&stringp, ",");
        if (pos) {
          ast_copy_string(name, pos, sizeof(name));
          if (strchr(pos, ' ')) {
            space = strchr(pos, ' ');
            *space = '\0';
          }
          if (pos) {
            conv = celliax_dir_convert(pos);
            DEBUGA_AT("<pos=>%s<conv=>%s<\n", CELLIAX_P_LOG, pos, conv);

            options = strdup(v->value);
            strsep(&options, ",");
            strsep(&options, ",");
            strsep(&options, ",");
            strsep(&options, ",");
            DEBUGA_AT("options=%s\n", CELLIAX_P_LOG, options);

            while ((s = strsep(&options, "|"))) {
              value = s;
              if ((var = strsep(&value, "=")) && value) {
                DEBUGA_AT("var=%s value=%s\n", CELLIAX_P_LOG, var, value);
                if (!strcmp(var, "phonebook_direct_calling_ext"))
                  strncpy(phonebook_direct_calling_ext, value, 6);
              }
            }

            //FIXME choose a logic for fields maximum lenght
            res =
              snprintf(entry_number, sizeof(entry_number) - 1, "%s%s%d%s%s", argv[2], "p",
                       p->celliax_dir_prefix, "p", phonebook_direct_calling_ext);
            if (res == (sizeof(entry_number) - 1)
                || res > (sizeof(entry_number) - 1)) {
              ERRORA("entry_number truncated, was: '%s%s%d%s%s', now is: '%s'\n",
                     CELLIAX_P_LOG, argv[2], "p", p->celliax_dir_prefix, "p",
                     phonebook_direct_calling_ext, entry_number);
              //FIXME: abort ???

            }

            res = snprintf(final_entry_text, sizeof(final_entry_text) - 1, "%s", name); //FIXME result not checked

            int i, a;

            a = 0;
            for (i = 0; i < p->csv_complete_name_pos - 1; i++) {
              if (p->csv_separator_is_semicolon)
                write_entry_command[a] = ';';
              else
                write_entry_command[a] = ',';
              a++;
            }
            //NOTICA("i=%d a=%d\n", CELLIAX_P_LOG, i, a);

            write_entry_command[a] = '"';
            a++;
            //NOTICA("i=%d a=%d\n", CELLIAX_P_LOG, i, a);
            for (i = 0; i < strlen(final_entry_text); i++) {
              write_entry_command[a] = final_entry_text[i];
              a++;
            }
            //NOTICA("i=%d a=%d\n", CELLIAX_P_LOG, i, a);
            write_entry_command[a] = '"';
            a++;
            //NOTICA("i=%d a=%d\n", CELLIAX_P_LOG, i, a);
            for (i = 0; i < (p->csv_business_phone_pos - p->csv_complete_name_pos); i++) {
              if (p->csv_separator_is_semicolon)
                write_entry_command[a] = ';';
              else
                write_entry_command[a] = ',';
              a++;
            }

            //NOTICA("i=%d a=%d\n", CELLIAX_P_LOG, i, a);

            write_entry_command[a] = '"';
            a++;
            //NOTICA("i=%d a=%d\n", CELLIAX_P_LOG, i, a);
            for (i = 0; i < strlen(entry_number); i++) {
              write_entry_command[a] = entry_number[i];
              a++;
            }
            //NOTICA("i=%d a=%d\n", CELLIAX_P_LOG, i, a);
            write_entry_command[a] = '"';
            a++;
            //NOTICA("i=%d a=%d\n", CELLIAX_P_LOG, i, a);

            if (option_debug)
              NOTICA("%s\n", CELLIAX_P_LOG, write_entry_command);
          }
        }
        if (conv)
          free(conv);
        if (start)
          free(start);
        if (options)
          free(options);
      }
      v = v->next;
    }

  }
  if (tovcf) {
//TODO implementation here
  }
#endif /*  CELLIAX_LIBCSV */
  ast_config_destroy(cfg);
  return 0;
}

#ifdef CELLIAX_LIBCSV

void celliax_cb1(char *s, size_t len, void *data)
{
  struct celliax_pvt *p = data;
  char field_content[256];

  p->csv_fields++;
  memset(field_content, 0, sizeof(field_content));
  strncpy(field_content, s,
          sizeof(field_content) > (len + 1) ? len : (sizeof(field_content) - 1));
  if (p->csv_fields == p->csv_complete_name_pos) {
    strncpy(p->csv_complete_name, field_content, sizeof(p->csv_complete_name) - 1);
  }
  if (p->csv_fields == p->csv_email_pos) {
    strncpy(p->csv_email, field_content, sizeof(p->csv_email) - 1);
  }
  if (p->csv_fields == p->csv_home_phone_pos) {
    strncpy(p->csv_home_phone, field_content, sizeof(p->csv_home_phone) - 1);
  }
  if (p->csv_fields == p->csv_mobile_phone_pos) {
    strncpy(p->csv_mobile_phone, field_content, sizeof(p->csv_mobile_phone) - 1);
  }
  if (p->csv_fields == p->csv_business_phone_pos) {
    strncpy(p->csv_business_phone, field_content, sizeof(p->csv_business_phone) - 1);
  }
}

void celliax_cb2(char c, void *data)
{
  struct celliax_pvt *p = data;

  p->csv_rows++;
  p->csv_fields = 0;

  if (p->csv_first_row_is_title && p->csv_rows == 1) {
    //do nothing
  } else {
    if (strlen(p->csv_complete_name)) {
      if (option_debug)
        NOTICA
          ("ROW %d ENDED, complete_name=%s, email=%s, home_phone=%s, mobile_phone=%s, business_phone=%s\n",
           CELLIAX_P_LOG, p->csv_rows,
           strlen(p->csv_complete_name) ? p->csv_complete_name : "N/A",
           strlen(p->csv_email) ? p->csv_email : "N/A",
           strlen(p->csv_home_phone) ? p->csv_home_phone : "N/A",
           strlen(p->csv_mobile_phone) ? p->csv_mobile_phone : "N/A",
           strlen(p->csv_business_phone) ? p->csv_business_phone : "N/A");
    }

    /* write entries in phonebook file */
    if (p->phonebook_writing_fp) {
      celliax_dir_entry_extension++;

      if (strlen(p->csv_complete_name)) {
        /* let's start with home_phone */
        if (strlen(p->csv_home_phone)) {
          fprintf(p->phonebook_writing_fp,
                  "%s  => ,%s %sSKO,,,hidefromdir=%s|phonebook_direct_calling_ext=%d%s%.4d|phonebook_entry_fromcsv=%s|phonebook_entry_owner=%s\n",
                  p->csv_home_phone, p->csv_complete_name, "HOME", "no",
                  p->celliax_dir_entry_extension_prefix, "2", celliax_dir_entry_extension,
                  "yes", "not_specified");
          fprintf(p->phonebook_writing_fp,
                  "%s  => ,%s %sDO,,,hidefromdir=%s|phonebook_direct_calling_ext=%d%s%.4d|phonebook_entry_fromcsv=%s|phonebook_entry_owner=%s\n",
                  p->csv_home_phone, p->csv_complete_name, "HOME", "no",
                  p->celliax_dir_entry_extension_prefix, "3", celliax_dir_entry_extension,
                  "yes", "not_specified");
        }

        /* now business_phone */
        if (strlen(p->csv_business_phone)) {
          fprintf(p->phonebook_writing_fp,
                  "%s  => ,%s %sSKO,,,hidefromdir=%s|phonebook_direct_calling_ext=%d%s%.4d|phonebook_entry_fromcsv=%s|phonebook_entry_owner=%s\n",
                  p->csv_business_phone, p->csv_complete_name, "BIZ", "no",
                  p->celliax_dir_entry_extension_prefix, "2", celliax_dir_entry_extension,
                  "yes", "not_specified");
          fprintf(p->phonebook_writing_fp,
                  "%s  => ,%s %sDO,,,hidefromdir=%s|phonebook_direct_calling_ext=%d%s%.4d|phonebook_entry_fromcsv=%s|phonebook_entry_owner=%s\n",
                  p->csv_business_phone, p->csv_complete_name, "BIZ", "no",
                  p->celliax_dir_entry_extension_prefix, "3", celliax_dir_entry_extension,
                  "yes", "not_specified");
        }

        /* let's end with mobile_phone */
        if (strlen(p->csv_mobile_phone)) {
          fprintf(p->phonebook_writing_fp,
                  "%s  => ,%s %sSKO,,,hidefromdir=%s|phonebook_direct_calling_ext=%d%s%.4d|phonebook_entry_fromcsv=%s|phonebook_entry_owner=%s\n",
                  p->csv_mobile_phone, p->csv_complete_name, "CELL", "no",
                  p->celliax_dir_entry_extension_prefix, "2", celliax_dir_entry_extension,
                  "yes", "not_specified");
          fprintf(p->phonebook_writing_fp,
                  "%s  => ,%s %sDO,,,hidefromdir=%s|phonebook_direct_calling_ext=%d%s%.4d|phonebook_entry_fromcsv=%s|phonebook_entry_owner=%s\n",
                  p->csv_mobile_phone, p->csv_complete_name, "CELL", "no",
                  p->celliax_dir_entry_extension_prefix, "3", celliax_dir_entry_extension,
                  "yes", "not_specified");
        }
      }

    }

  }
}

#endif /* CELLIAX_LIBCSV */

int celliax_console_celliax_dir_import(int fd, int argc, char *argv[])
{
  int res;
  struct celliax_pvt *p = celliax_console_find_desc(celliax_console_active);
  char list_command[64];
  char fn[256];
  char date[256] = "";
  time_t t;
  char *configfile = CELLIAX_DIR_CONFIG;
  int add_to_celliax_dir_conf = 1;
  //int fromskype = 0;
  int fromcell = 0;
#ifdef CELLIAX_LIBCSV
  int fromcsv = 0;
  int fromvcf = 0;
#endif /* CELLIAX_LIBCSV */

  if (argc < 3 || argc > 4)
    return RESULT_SHOWUSAGE;
  if (!p) {
    ast_cli(fd, "No \"current\" console ???, please enter 'help celliax_console'\n");
    return RESULT_SUCCESS;
  }

  if (!strcasecmp(argv[1], "add"))
    add_to_celliax_dir_conf = 1;
  else if (!strcasecmp(argv[1], "replace"))
    add_to_celliax_dir_conf = 0;
  else {
    ast_cli(fd, "\n\nYou have neither specified 'add' nor 'replace'\n\n");
    return RESULT_SHOWUSAGE;
  }

  //if (!strcasecmp(argv[2], "fromskype"))
  //fromskype = 1;
  //else 

  if (!strcasecmp(argv[2], "fromcell"))
    fromcell = 1;
#ifdef CELLIAX_LIBCSV
  else if (!strcasecmp(argv[2], "fromcsv"))
    fromcsv = 1;
  else if (!strcasecmp(argv[2], "fromvcf"))
    fromvcf = 1;
#endif /* CELLIAX_LIBCSV */
  else {
    ast_cli(fd, "\n\nYou have neither specified 'fromcell' neither 'fromcsv'\n\n");
    return RESULT_SHOWUSAGE;
  }

#ifdef CELLIAX_LIBCSV
  if (fromcsv || fromvcf)
    if (argc != 4) {
      ast_cli(fd,
              "\n\nYou have to specify a filename with 'fromcsv' or with 'fromvcf'\n\n");
      return RESULT_SHOWUSAGE;
    }
#endif /* CELLIAX_LIBCSV */
  if (fromcell)
    if (p->controldevprotocol != PROTOCOL_AT) {
      ast_cli(fd,
              "Importing from cellphone is currently supported only on \"AT\" cellphones :( !\n");
      //fclose(p->phonebook_writing_fp);
      //celliax_dir_create_extensions();
      return RESULT_SUCCESS;
    }

  if (fromcell)
    if (argc != 3) {
      ast_cli(fd, "\n\nYou don't have to specify a filename with 'fromcell'\n\n");
      return RESULT_SHOWUSAGE;
    }
#ifdef CELLIAX_LIBCSV
  if (fromvcf) {
    if (option_debug)
      NOTICA("filename is: %s\n", CELLIAX_P_LOG, argv[3]);
    ast_cli(fd, "\n\nnot yet implemented :P \n");
    return RESULT_SUCCESS;
  }
#endif /* CELLIAX_LIBCSV */

  /*******************************************************************************************/

  if (configfile[0] == '/') {
    ast_copy_string(fn, configfile, sizeof(fn));
  } else {
    snprintf(fn, sizeof(fn), "%s/%s", ast_config_AST_CONFIG_DIR, configfile);
  }
  if (option_debug)
    NOTICA("Opening '%s'\n", CELLIAX_P_LOG, fn);
  time(&t);
  ast_copy_string(date, ctime(&t), sizeof(date));

  if (add_to_celliax_dir_conf)
    p->phonebook_writing_fp = fopen(fn, "a+");
  else
    p->phonebook_writing_fp = fopen(fn, "w+");

  if (p->phonebook_writing_fp) {
    if (add_to_celliax_dir_conf) {
      if (option_debug)
        NOTICA("Opened '%s' for appending \n", CELLIAX_P_LOG, fn);
      fprintf(p->phonebook_writing_fp, ";!\n");
      fprintf(p->phonebook_writing_fp, ";! Update Date: %s", date);
      fprintf(p->phonebook_writing_fp, ";! Updated by: %s, %d\n", __FILE__, __LINE__);
      fprintf(p->phonebook_writing_fp, ";!\n");
    } else {
      if (option_debug)
        NOTICA("Opened '%s' for writing \n", CELLIAX_P_LOG, fn);
      fprintf(p->phonebook_writing_fp, ";!\n");
      fprintf(p->phonebook_writing_fp, ";! Automatically generated configuration file\n");
      fprintf(p->phonebook_writing_fp, ";! Filename: %s (%s)\n", configfile, fn);
      fprintf(p->phonebook_writing_fp, ";! Creation Date: %s", date);
      fprintf(p->phonebook_writing_fp, ";! Generated by: %s, %d\n", __FILE__, __LINE__);
      fprintf(p->phonebook_writing_fp, ";!\n");
      fprintf(p->phonebook_writing_fp, "[general]\n\n");
      fprintf(p->phonebook_writing_fp, "[default]\n");
    }

#ifdef CELLIAX_LIBCSV
    //FIXME: if add_to_celliax_dir_conf parse the "old" config file, so to have the correct next entry id-exten
    if (fromcsv) {
      if (option_debug)
        NOTICA("filename is: %s\n", CELLIAX_P_LOG, argv[3]);

/************************/
      FILE *fp;
      struct csv_parser *csvp;
      char buf[1024];
      size_t bytes_read;
      unsigned char options = 0;

      p->csv_rows = 0;
      p->csv_fields = 0;

      if (p->csv_separator_is_semicolon) {
        if (csv_init(&csvp, options | CSV_USE_SEMICOLON_SEPARATOR) != 0) {
          ERRORA("Failed to initialize csv parser\n", CELLIAX_P_LOG);
          return RESULT_SUCCESS;
        }
      } else {
        if (csv_init(&csvp, options) != 0) {
          ERRORA("Failed to initialize csv parser\n", CELLIAX_P_LOG);
          return RESULT_SUCCESS;
        }

      }

      fp = fopen(argv[3], "rb");
      if (!fp) {
        ERRORA("Failed to open %s: %s\n", CELLIAX_P_LOG, argv[3], strerror(errno));
        return RESULT_SUCCESS;
      }
      while ((bytes_read = fread(buf, 1, 1024, fp)) > 0) {
        if (csv_parse(csvp, buf, bytes_read, celliax_cb1, celliax_cb2, p) != bytes_read) {
          ERRORA("Error while parsing file: %s\n", CELLIAX_P_LOG,
                 csv_strerror(csv_error(csvp)));
        }
      }

      csv_fini(csvp, celliax_cb1, celliax_cb2, p);

      if (ferror(fp)) {
        ERRORA("Error while reading file %s\n", CELLIAX_P_LOG, argv[3]);
        fclose(fp);
        return RESULT_SUCCESS;
      }

      fclose(fp);
      if (option_debug)
        NOTICA("%s: %d fields, %d rows\n", CELLIAX_P_LOG, argv[3], p->csv_fields,
               p->csv_rows);

      csv_free(csvp);

    /**************************/
    }
#endif /* CELLIAX_LIBCSV */

  /*******************************************************************************************/
    //if (fromskype) {
    //ast_cli(fd,
    //"Skype not supported in celliax_dir. Load chan_skypiax and use skypiax_dir!\n");
    //}

  /*******************************************************************************************/
    if (fromcell) {
      /* which phonebook to use, use the SIM  */
      res = celliax_serial_write_AT_ack(p, "AT+CPBS=SM");
      if (res) {
        WARNINGA("AT+CPBS=SM failed, continue\n", CELLIAX_P_LOG);
      }
      /* which phonebook to use, trying to use combined phone+SIM  */
      res = celliax_serial_write_AT_ack(p, "AT+CPBS=MT");
      if (res) {
        WARNINGA("AT+CPBS=MT failed, continue\n", CELLIAX_P_LOG);
      }
      /* How many entries in phonebook  */
      p->phonebook_querying = 1;
      res = celliax_serial_write_AT_ack(p, "AT+CPBR=?");
      if (res) {
        WARNINGA("AT+CPBR=? failed, continue\n", CELLIAX_P_LOG);
      }
      p->phonebook_querying = 0;
      /* list entries in phonebook, give the SIM the time to answer  */
      WARNINGA
        ("About to querying the cellphone phonebook, if the SIM do not answer may stuck here for 20 seconds... Don't worry.\n",
         CELLIAX_P_LOG);
      sprintf(list_command, "AT+CPBR=%d,%d", p->phonebook_first_entry,
              p->phonebook_last_entry);
      p->phonebook_listing = 1;
      res = celliax_serial_write_AT_expect_longtime(p, list_command, "OK");
      if (res) {
        WARNINGA("AT+CPBR=%d,%d failed, continue\n", CELLIAX_P_LOG,
                 p->phonebook_first_entry, p->phonebook_last_entry);
      }
      p->phonebook_listing = 0;
    }
  /*******************************************************************************************/
#ifdef CELLIAX_LIBCSV
    if (fromvcf) {
      //TODO implementation here
    }
#endif /* CELLIAX_LIBCSV */

  } else {
    ast_cli(fd, "\n\nfailed to open the directoriax.conf configuration file: %s\n", fn);
    ERRORA("failed to open the directoriax.conf configuration file: %s\n", CELLIAX_P_LOG,
           fn);
    return RESULT_FAILURE;
  }

  fclose(p->phonebook_writing_fp);
  //celliax_dir_create_extensions();

  return RESULT_SUCCESS;
}

#endif /* CELLIAX_DIR */

#ifdef CELLIAX_FBUS2

int celliax_serial_getstatus_FBUS2(struct celliax_pvt *p)
{
  unsigned char MsgBuffer[7];
  int res;
  int how_many_reads = 0;

  PUSHA_UNLOCKA(&p->controldev_lock);
  LOKKA(&p->controldev_lock);

  MsgBuffer[0] = FBUS2_COMMAND_BYTE_1;
  MsgBuffer[1] = FBUS2_COMMAND_BYTE_2;
  MsgBuffer[2] = 0x00;
  MsgBuffer[3] = 0x03;
  MsgBuffer[4] = 0x00;
  MsgBuffer[5] = FBUS2_IS_LAST_FRAME;
  MsgBuffer[6] = celliax_serial_get_seqnum_FBUS2(p);

  if (option_debug > 1)
    DEBUGA_FBUS2("asking model, outseqnum %.2X \n", CELLIAX_P_LOG, MsgBuffer[6]);
  celliax_serial_write_FBUS2(p, MsgBuffer, 7, FBUS2_TYPE_MODEL_ASK);
  usleep(1000);
  res = celliax_serial_read_FBUS2(p);   //we don't have no monitor neither do_controldev_thread
  if (res == -1) {
    ERRORA("failed celliax_serial_read_FBUS2\n", CELLIAX_P_LOG);
    UNLOCKA(&p->controldev_lock);
    return -1;
  }
  while (res != MsgBuffer[6] && res != FBUS2_TYPE_MODEL_ANSWER) {
    usleep(1000);
    res = celliax_serial_read_FBUS2(p);
    how_many_reads++;
    if (res == -1) {
      ERRORA("failed celliax_serial_read_FBUS2\n", CELLIAX_P_LOG);
      UNLOCKA(&p->controldev_lock);
      return -1;
    }
    if (how_many_reads > 10) {
      ERRORA("no expected results in %d celliax_serial_read_FBUS2\n", CELLIAX_P_LOG,
             how_many_reads);
      UNLOCKA(&p->controldev_lock);
      return -1;
    }
  }

  UNLOCKA(&p->controldev_lock);
  POPPA_UNLOCKA(&p->controldev_lock);
  return 0;
}

int celliax_serial_sync_FBUS2(struct celliax_pvt *p)
{
  unsigned char initc = 0x55;   /* FBUS2 initialization char */
  int c, rt;
  PUSHA_UNLOCKA(&p->controldev_lock);
  LOKKA(&p->controldev_lock);
  /*  init the link (sync receive uart) */
  for (c = 0; c < 55; c++) {    /* 55 times */
    usleep(10000);
    rt = write(p->controldevfd, &initc, 1);
    if (rt != 1) {
      ERRORA("serial error: %s", CELLIAX_P_LOG, strerror(errno));
      UNLOCKA(&p->controldev_lock);
      return -1;
    }
  }
  time(&p->celliax_serial_synced_timestamp);
  UNLOCKA(&p->controldev_lock);
  POPPA_UNLOCKA(&p->controldev_lock);
  return 0;
}

int celliax_serial_answer_FBUS2(struct celliax_pvt *p)
{
  unsigned char MsgBuffer[6];

  celliax_serial_security_command_FBUS2(p);

  MsgBuffer[0] = FBUS2_COMMAND_BYTE_1;
  MsgBuffer[1] = FBUS2_COMMAND_BYTE_2;
  MsgBuffer[2] = FBUS2_SECURIY_CALL_COMMANDS;
  MsgBuffer[3] = FBUS2_SECURIY_CALL_COMMAND_ANSWER;
  MsgBuffer[4] = FBUS2_IS_LAST_FRAME;
  MsgBuffer[5] = celliax_serial_get_seqnum_FBUS2(p);
  if (option_debug > 1)
    DEBUGA_FBUS2("celliax_serial_answer_FBUS2, outseqnum %.2X \n", CELLIAX_P_LOG,
                 MsgBuffer[5]);
  celliax_serial_write_FBUS2(p, MsgBuffer, 6, FBUS2_TYPE_SECURITY);
  DEBUGA_FBUS2("FBUS2: sent commands to answer the call\n", CELLIAX_P_LOG);
  p->interface_state = AST_STATE_UP;    //FIXME

  return 0;
}

int celliax_serial_call_FBUS2(struct celliax_pvt *p, char *dstr)
{
  unsigned char MsgBufferNum[255];
  int i;

  celliax_serial_security_command_FBUS2(p);

  MsgBufferNum[0] = FBUS2_COMMAND_BYTE_1;
  MsgBufferNum[1] = FBUS2_COMMAND_BYTE_2;
  MsgBufferNum[2] = FBUS2_SECURIY_CALL_COMMANDS;
  MsgBufferNum[3] = FBUS2_SECURIY_CALL_COMMAND_CALL;
  for (i = 0; i < strlen(dstr); i++) {
    MsgBufferNum[4 + i] = dstr[i];
  }
  MsgBufferNum[4 + strlen(dstr)] = 0x00;    /* required by FBUS2 prot */
  MsgBufferNum[4 + strlen(dstr) + 1] = FBUS2_IS_LAST_FRAME;
  MsgBufferNum[4 + strlen(dstr) + 2] = celliax_serial_get_seqnum_FBUS2(p);
  if (option_debug > 1)
    DEBUGA_FBUS2("celliax_serial_call_FBUS2, outseqnum %.2X \n", CELLIAX_P_LOG,
                 MsgBufferNum[4 + strlen(dstr) + 2]);
  celliax_serial_write_FBUS2(p, MsgBufferNum, 5 + strlen(dstr) + 2, FBUS2_TYPE_SECURITY);

  p->phone_callflow = CALLFLOW_CALL_DIALING;
  p->interface_state = AST_STATE_DIALING;
  if (option_debug)
    DEBUGA_FBUS2("FBUS2: sent commands to call\n", CELLIAX_P_LOG);
  return 0;
}

int celliax_serial_hangup_FBUS2(struct celliax_pvt *p)
{
  unsigned char MsgBuffer[6];

  if (p->interface_state != AST_STATE_DOWN) {
    celliax_serial_security_command_FBUS2(p);

    MsgBuffer[0] = FBUS2_COMMAND_BYTE_1;
    MsgBuffer[1] = FBUS2_COMMAND_BYTE_2;
    MsgBuffer[2] = FBUS2_SECURIY_CALL_COMMANDS;
    MsgBuffer[3] = FBUS2_SECURIY_CALL_COMMAND_RELEASE;
    MsgBuffer[4] = FBUS2_IS_LAST_FRAME;
    MsgBuffer[5] = celliax_serial_get_seqnum_FBUS2(p);

    if (option_debug > 1)
      DEBUGA_FBUS2("celliax_serial_hangup_FBUS2, outseqnum %.2X \n", CELLIAX_P_LOG,
                   MsgBuffer[5]);
    celliax_serial_write_FBUS2(p, MsgBuffer, 6, FBUS2_TYPE_SECURITY);

    DEBUGA_FBUS2("FBUS2: sent commands to hangup the call\n", CELLIAX_P_LOG);

  }
  p->interface_state = AST_STATE_DOWN;  //FIXME
  p->phone_callflow = CALLFLOW_CALL_IDLE;   //FIXME
  return 0;
}

int celliax_serial_config_FBUS2(struct celliax_pvt *p)
{
  unsigned char MsgBuffer[6];
  int res;
  int how_many_reads = 0;

  MsgBuffer[0] = FBUS2_COMMAND_BYTE_1;
  MsgBuffer[1] = FBUS2_COMMAND_BYTE_2;
  MsgBuffer[2] = FBUS2_SECURIY_EXTENDED_COMMANDS;
  MsgBuffer[3] = FBUS2_SECURIY_EXTENDED_COMMAND_ON;
  MsgBuffer[4] = FBUS2_IS_LAST_FRAME;
  MsgBuffer[5] = celliax_serial_get_seqnum_FBUS2(p);

  if (option_debug > 1)
    DEBUGA_FBUS2("activating security commands for getting IMEI, outseqnum %.2X \n",
                 CELLIAX_P_LOG, MsgBuffer[5]);
  celliax_serial_write_FBUS2(p, MsgBuffer, 6, FBUS2_TYPE_SECURITY);
  res = celliax_serial_read_FBUS2(p);   //we don't have no monitor neither do_controldev_thread
  if (res == -1) {
    ERRORA("failed celliax_serial_read_FBUS2\n", CELLIAX_P_LOG);
    return -1;
  }
  while (res != MsgBuffer[5] && res != FBUS2_SECURIY_EXTENDED_COMMAND_ON) {
    usleep(1000);
    res = celliax_serial_read_FBUS2(p);
    how_many_reads++;
    if (res == -1) {
      ERRORA("failed celliax_serial_read_FBUS2\n", CELLIAX_P_LOG);
      return -1;
    }
    if (how_many_reads > 10) {
      ERRORA("no expected results in %d celliax_serial_read_FBUS2\n", CELLIAX_P_LOG,
             how_many_reads);
      return -1;
    }
  }

  MsgBuffer[0] = FBUS2_COMMAND_BYTE_1;
  MsgBuffer[1] = FBUS2_COMMAND_BYTE_2;
  MsgBuffer[2] = FBUS2_SECURIY_IMEI_COMMANDS;
  MsgBuffer[3] = FBUS2_SECURIY_IMEI_COMMAND_GET;
  MsgBuffer[4] = FBUS2_IS_LAST_FRAME;
  MsgBuffer[5] = celliax_serial_get_seqnum_FBUS2(p);
  if (option_debug > 1)
    DEBUGA_FBUS2("celliax_serial_get_IMEI_init_FBUS2, outseqnum %.2X \n", CELLIAX_P_LOG,
                 MsgBuffer[5]);
  celliax_serial_write_FBUS2(p, MsgBuffer, 6, FBUS2_TYPE_SECURITY);
  res = celliax_serial_read_FBUS2(p);   //we don't have no monitor neither do_controldev_thread
  if (res == -1) {
    ERRORA("failed celliax_serial_read_FBUS2\n", CELLIAX_P_LOG);
    return -1;
  }
  how_many_reads = 0;
  while (res != MsgBuffer[5] && res != CALLFLOW_GOT_IMEI) {
    usleep(1000);
    res = celliax_serial_read_FBUS2(p);
    how_many_reads++;
    if (res == -1) {
      ERRORA("failed celliax_serial_read_FBUS2\n", CELLIAX_P_LOG);
      return -1;
    }
    if (how_many_reads > 10) {
      ERRORA("no expected results in %d celliax_serial_read_FBUS2\n", CELLIAX_P_LOG,
             how_many_reads);
      //FIXME return -1;
      return 0;
    }
  }

  if (option_debug > 1)
    DEBUGA_FBUS2("xxxxx GOT IMEI xxxxx res=%d %.2X \n", CELLIAX_P_LOG, res, res);

  return 0;
}

int celliax_serial_get_seqnum_FBUS2(struct celliax_pvt *p)
{
  if (p->seqnumfbus > FBUS2_SEQNUM_MAX || p->seqnumfbus < FBUS2_SEQNUM_MIN) {
    ERRORA("p->seqnumfbus: %2.X\n", CELLIAX_P_LOG, p->seqnumfbus);
    p->seqnumfbus = FBUS2_SEQNUM_MIN;
  }

  if (p->seqnumfbus == FBUS2_SEQNUM_MAX) {
    p->seqnumfbus = FBUS2_SEQNUM_MIN;
  } else {
    p->seqnumfbus++;
  }
  if (option_debug > 10)
    DEBUGA_FBUS2("sqnum: %2.X\n", CELLIAX_P_LOG, p->seqnumfbus);
  return p->seqnumfbus;
}

int celliax_serial_security_command_FBUS2(struct celliax_pvt *p)
{
  unsigned char MsgBuffer[6];

  MsgBuffer[0] = FBUS2_COMMAND_BYTE_1;
  MsgBuffer[1] = FBUS2_COMMAND_BYTE_2;
  MsgBuffer[2] = FBUS2_SECURIY_EXTENDED_COMMANDS;
  MsgBuffer[3] = FBUS2_SECURIY_EXTENDED_COMMAND_ON;
  MsgBuffer[4] = FBUS2_IS_LAST_FRAME;
  MsgBuffer[5] = celliax_serial_get_seqnum_FBUS2(p);

  if (option_debug > 1)
    DEBUGA_FBUS2("activating security commands, outseqnum %.2X \n", CELLIAX_P_LOG,
                 MsgBuffer[5]);
  celliax_serial_write_FBUS2(p, MsgBuffer, 6, FBUS2_TYPE_SECURITY);
  return 0;
}

/*!
 * \brief Write on the serial port for all the FBUS2 (old Nokia) functions
 * \param p celliax_pvt
 * \param len lenght of buffer2
 * \param buffer2 chars to be written
 *
 * Write on the serial port for all the FBUS2 (old Nokia) functions
 *
 * \return the number of chars written on the serial, 
 * that can be different from len (or negative) in case of errors.
 */
int celliax_serial_send_FBUS2(struct celliax_pvt *p, int len, unsigned char *mesg_ptr)
{
  int ret;
  size_t actual = 0;
  unsigned char *mesg_ptr2 = mesg_ptr;
  PUSHA_UNLOCKA(&p->controldev_lock);
  LOKKA(&p->controldev_lock);
  do {
    ret = write(p->controldevfd, mesg_ptr, len - actual);
    if (ret < 0 && errno == EAGAIN)
      continue;
    if (ret < 0) {
      if (actual != len)
        ERRORA("celliax_serial_write error: %s", CELLIAX_P_LOG, strerror(errno));
      UNLOCKA(&p->controldev_lock);
      return -1;
    }
    actual += ret;
    mesg_ptr += ret;
    usleep(10000);
  } while (actual < len);

  UNLOCKA(&p->controldev_lock);
  POPPA_UNLOCKA(&p->controldev_lock);
  if (option_debug > 10) {
    int i;
    char debug_buf[1024];
    char *debug_buf_pos;

    memset(debug_buf, 0, 1024);
    debug_buf_pos = debug_buf;

    for (i = 0; i < len; i++) {
      debug_buf_pos += sprintf(debug_buf_pos, "[%.2X] ", mesg_ptr2[i]);
      if (debug_buf_pos > ((char *) &debug_buf + 1000))
        break;
    }
    DEBUGA_FBUS2("%s was sent down the wire\n", CELLIAX_P_LOG, debug_buf);
  }

  return 0;
}

/*!
 * \brief Flags as acknowledged an FBUS2 message previously sent
 * \param p celliax_pvt
 * \param seqnum identifier of the message to be acknowledged
 *
 * Called upon receiving an FBUS2 acknoledgement message, browse the fbus2_outgoing_list 
 * looking for the seqnum sent FBUS2 message, and flags it as acknowledged.
 * (if an outgoing FBUS2 message is not aknowledged by the cellphone in a while,
 * it will be retransmitted)
 *
 * \return 0 on error, 1 otherwise
 */
int celliax_serial_list_acknowledge_FBUS2(struct celliax_pvt *p, int seqnum)
{
  struct fbus2_msg *ptr;

  ptr = p->fbus2_outgoing_list;
  if (ptr == NULL) {
    ERRORA("fbus2_outgoing_list is NULL ?\n", CELLIAX_P_LOG);
    return -1;
  }
  PUSHA_UNLOCKA(&p->fbus2_outgoing_list_lock);
  LOKKA(&p->fbus2_outgoing_list_lock);
  while (ptr->next != NULL)
    ptr = ptr->next;
  while (ptr->acknowledged == 0) {
    if (ptr->seqnum == seqnum) {
      ptr->acknowledged = 1;
      if (option_debug > 1)
        DEBUGA_FBUS2("Acknowledgment to %.2X\n", CELLIAX_P_LOG, seqnum);

      DEBUGA_FBUS2("PREFREE OUTGOING list:\n", CELLIAX_P_LOG);
      celliax_serial_list_print_FBUS2(p, p->fbus2_outgoing_list);
      if (ptr->previous) {
        if (ptr->next) {
          ptr->previous->next = ptr->next;
        } else {
          ptr->previous->next = NULL;
        }
      }
      if (ptr->next) {
        if (ptr->previous) {
          ptr->next->previous = ptr->previous;
        } else {
          ptr->next->previous = NULL;
        }
      }

      if ((NULL == ptr->next) && (NULL == ptr->previous)) { /* bug catched by Wojciech Andralojc */
        if (option_debug > 1)
          DEBUGA_FBUS2("FREEING LAST\n", CELLIAX_P_LOG);
        p->fbus2_outgoing_list = NULL;
        p->fbus2_outgoing_list = celliax_serial_list_init_FBUS2(p);
      }

      free(ptr);
      DEBUGA_FBUS2("POSTFREE OUTGOING list:\n", CELLIAX_P_LOG);
      celliax_serial_list_print_FBUS2(p, p->fbus2_outgoing_list);

      break;
    }
    if (ptr->previous != NULL) {
      ptr = ptr->previous;
    } else {
      ERRORA
        ("The phone sent us an acknowledgement referring to a msg with a seqnum that is not in our sent list: %.2X\n",
         CELLIAX_P_LOG, seqnum);
      break;
    }
  }
  UNLOCKA(&p->fbus2_outgoing_list_lock);
  POPPA_UNLOCKA(&p->fbus2_outgoing_list_lock);
  return 0;
}

/*!
 * \brief Sends an FBUS2 message or resends it if it was not acknowledged
 * \param p celliax_pvt
 *
 * Called by celliax_serial_read_FBUS2, browse the fbus2_outgoing_list looking for FBUS2 messages to be sent, 
 * or for FBUS2 messages previously sent but not yet acknoledged.
 * (if an outgoing FBUS2 message is not aknowledged by the cellphone in a while,
 * it will be retransmitted)
 *
 * \return 0 on error, 1 otherwise
 */
int celliax_serial_send_if_time_FBUS2(struct celliax_pvt *p)
{
  struct fbus2_msg *ptr;
  struct timeval tv;
  struct timezone tz;

  gettimeofday(&tv, &tz);
  ptr = p->fbus2_outgoing_list;
  if (ptr == NULL) {
    ERRORA("fbus2_outgoing_list is NULL ?\n", CELLIAX_P_LOG);
    return -1;
  }
  while (ptr->next != NULL) {
    WARNINGA("fbus2_outgoing_list->next is not null ?\n", CELLIAX_P_LOG);
    ptr = ptr->next;            //FIXME what to do?
  }
  while (ptr->sent == 0 && ptr->acknowledged == 0) {
    if (ptr->previous != NULL) {
      ptr = ptr->previous;
    } else
      break;
  }
  while (ptr->sent == 1 && ptr->acknowledged == 0) {
    if (ptr->previous != NULL) {
      ptr = ptr->previous;
    } else
      break;
  }
  if (ptr->sent == 1 && ptr->acknowledged == 1) {
    if (ptr->next != NULL) {
      ptr = ptr->next;
    }
  }
  if (ptr->sent == 1 && ptr->acknowledged == 0 && ptr->msg > 0) {
    if ((tv.tv_sec * 1000 + tv.tv_usec / 1000) >
        ((ptr->tv_sec * 1000 + ptr->tv_usec / 1000) + 1000)) {

      PUSHA_UNLOCKA(&p->fbus2_outgoing_list_lock);
      LOKKA(&p->fbus2_outgoing_list_lock);

      if (ptr->sent == 1 && ptr->acknowledged == 0 && ptr->msg > 0) {   //retest, maybe has been changed?
        if ((tv.tv_sec * 1000 + tv.tv_usec / 1000) > ((ptr->tv_sec * 1000 + ptr->tv_usec / 1000) + 1000)) { //retest, maybe has been changed?

          if (option_debug > 1)
            DEBUGA_FBUS2("RESEND %.2X, passed %ld ms, sent %d times\n", CELLIAX_P_LOG,
                         ptr->seqnum,
                         ((tv.tv_sec * 1000 + tv.tv_usec / 1000) -
                          (ptr->tv_sec * 1000 + ptr->tv_usec / 1000)),
                         ptr->how_many_sent);
          if (ptr->how_many_sent > 9) {
            ERRORA("RESEND %.2X, passed %ld ms, sent %d times\n", CELLIAX_P_LOG,
                   ptr->seqnum,
                   ((tv.tv_sec * 1000 + tv.tv_usec / 1000) -
                    (ptr->tv_sec * 1000 + ptr->tv_usec / 1000)), ptr->how_many_sent);

            UNLOCKA(&p->fbus2_outgoing_list_lock);
            return -1;
          }

          celliax_serial_send_FBUS2(p, ptr->len, ptr->buffer);
          if (ptr->buffer[3] == FBUS2_ACK_BYTE) {
            if (option_debug > 1)
              DEBUGA_FBUS2("RESEND ACK, passed %ld ms, sent %d times\n", CELLIAX_P_LOG,
                           ((tv.tv_sec * 1000 + tv.tv_usec / 1000) -
                            (ptr->tv_sec * 1000 + ptr->tv_usec / 1000)),
                           ptr->how_many_sent);
            ptr->acknowledged = 1;
            ptr->msg = FBUS2_OUTGOING_ACK;
          }
          ptr->tv_sec = tv.tv_sec;
          ptr->tv_usec = tv.tv_usec;
          ptr->sent = 1;
          ptr->how_many_sent++;
          if (option_debug > 1) {
            DEBUGA_FBUS2("OUTGOING list:\n", CELLIAX_P_LOG);
            celliax_serial_list_print_FBUS2(p, p->fbus2_outgoing_list);
            DEBUGA_FBUS2("OUTGOING list END\n", CELLIAX_P_LOG);
          }

        }
      }

      UNLOCKA(&p->fbus2_outgoing_list_lock);
      POPPA_UNLOCKA(&p->fbus2_outgoing_list_lock);
    }
  }
  if (ptr->sent == 0 && ptr->acknowledged == 0 && ptr->msg > 0) {

    PUSHA_UNLOCKA(&p->fbus2_outgoing_list_lock);
    LOKKA(&p->fbus2_outgoing_list_lock);

    if (ptr->sent == 0 && ptr->acknowledged == 0 && ptr->msg > 0) { //retest, maybe has been changed?

      if (option_debug > 1)
        DEBUGA_FBUS2("SENDING 1st TIME %.2X\n", CELLIAX_P_LOG, ptr->seqnum);
      celliax_serial_send_FBUS2(p, ptr->len, ptr->buffer);
      if (ptr->buffer[3] == FBUS2_ACK_BYTE) {
        if (option_debug > 1)
          DEBUGA_FBUS2("SENDING 1st TIME ACK\n", CELLIAX_P_LOG);
        ptr->acknowledged = 1;
        ptr->msg = FBUS2_OUTGOING_ACK;
      }
      ptr->tv_sec = tv.tv_sec;
      ptr->tv_usec = tv.tv_usec;
      ptr->sent = 1;
      ptr->how_many_sent++;
      if (option_debug > 1) {
        DEBUGA_FBUS2("OUTGOING list:\n", CELLIAX_P_LOG);
        celliax_serial_list_print_FBUS2(p, p->fbus2_outgoing_list);
        DEBUGA_FBUS2("OUTGOING list END\n", CELLIAX_P_LOG);
      }

    }

    UNLOCKA(&p->fbus2_outgoing_list_lock);
    POPPA_UNLOCKA(&p->fbus2_outgoing_list_lock);

  }
  return 0;
}

int celliax_serial_write_FBUS2(struct celliax_pvt *p, unsigned char *MsgBuffer,
                               int MsgLength, unsigned char MsgType)
{
  unsigned char buffer2[FBUS2_MAX_TRANSMIT_LENGTH + 10];
  unsigned char checksum = 0;
  int i, len;
  struct timeval tv;
  struct timezone tz;

  buffer2[0] = FBUS2_SERIAL_FRAME_ID;
  buffer2[1] = FBUS2_DEVICE_PHONE;  /* destination */
  buffer2[2] = FBUS2_DEVICE_PC; /* source */
  buffer2[3] = MsgType;
  buffer2[4] = 0x00;            /* required by protocol */
  buffer2[5] = MsgLength;

  memcpy(buffer2 + 6, MsgBuffer, MsgLength);
  len = MsgLength + 6;

  /* Odd messages require additional padding 0x00 byte */
  if (MsgLength % 2)
    buffer2[len++] = 0x00;      /* optional PaddingByte */

  checksum = 0;
  for (i = 0; i < len; i += 2)
    checksum ^= buffer2[i];
  buffer2[len++] = checksum;    /* ChkSum1 */

  checksum = 0;
  for (i = 1; i < len; i += 2)
    checksum ^= buffer2[i];
  buffer2[len++] = checksum;    /* ChkSum2 */

  if (option_debug > 10) {
    int i;
    char debug_buf[1024];
    char *debug_buf_pos;

    memset(debug_buf, 0, 1024);
    debug_buf_pos = debug_buf;

    for (i = 0; i < len; i++) {
      debug_buf_pos += sprintf(debug_buf_pos, "[%.2X] ", buffer2[i]);
      if (debug_buf_pos > (char *) (&debug_buf + 1000))
        break;
    }
    if (buffer2[3] == FBUS2_ACK_BYTE) {
      DEBUGA_FBUS2("%s to be written, ACK\n", CELLIAX_P_LOG, debug_buf);
    } else {
      DEBUGA_FBUS2("%s to be written\n", CELLIAX_P_LOG, debug_buf);
    }
  }

  gettimeofday(&tv, &tz);

  if (buffer2[3] != FBUS2_ACK_BYTE) {
    p->fbus2_outgoing_list = celliax_serial_list_init_FBUS2(p);
    p->fbus2_outgoing_list->msg = 11;

    p->fbus2_outgoing_list->len = len;
    for (i = 0; i < len; i++) {
      p->fbus2_outgoing_list->buffer[i] = buffer2[i];
    }
    p->fbus2_outgoing_list->seqnum = MsgBuffer[MsgLength - 1];
    if (option_debug > 1) {
      DEBUGA_FBUS2("OUTGOING LIST seqnum is %2.X\n", CELLIAX_P_LOG,
                   MsgBuffer[MsgLength - 1]);

      DEBUGA_FBUS2("OUTGOING list:\n", CELLIAX_P_LOG);
      celliax_serial_list_print_FBUS2(p, p->fbus2_outgoing_list);
      DEBUGA_FBUS2("OUTGOING list END\n", CELLIAX_P_LOG);
    }
  } else {
    usleep(100);
    celliax_serial_send_FBUS2(p, len, buffer2);
  }

  return 0;
}

int celliax_serial_send_ack_FBUS2(struct celliax_pvt *p, unsigned char MsgType,
                                  unsigned char MsgSequence)
{
  unsigned char buffer2[2];

  buffer2[0] = MsgType;
  buffer2[1] = (MsgSequence - FBUS2_SEQNUM_MIN);

  if (option_debug > 1)
    DEBUGA_FBUS2("SENDING ACK to %2.X, seqack %2.X \n", CELLIAX_P_LOG, MsgSequence,
                 (MsgSequence - FBUS2_SEQNUM_MIN));
  /* Sending to phone */
  return celliax_serial_write_FBUS2(p, buffer2, 2, FBUS2_ACK_BYTE);
}

struct fbus2_msg *celliax_serial_list_init_FBUS2(struct celliax_pvt *p)
{
  struct fbus2_msg *list;
  list = p->fbus2_outgoing_list;

  PUSHA_UNLOCKA(&p->fbus2_outgoing_list_lock);
  LOKKA(&p->fbus2_outgoing_list_lock);
  if (list == NULL) {
    list = malloc(sizeof(*(list)));
    list->msg = 0;
    list->seqnum = 0;
    list->len = 0;
    list->acknowledged = 0;
    list->how_many_sent = 0;
    list->sent = 0;
    list->tv_sec = 0;
    list->tv_usec = 0;
    list->next = NULL;
    list->previous = NULL;
  }
  if (list->msg != 0) {
    struct fbus2_msg *new;
    new = malloc(sizeof(*new));
    new->msg = 0;
    new->seqnum = 0;
    new->len = 0;
    new->acknowledged = 0;
    new->how_many_sent = 0;
    new->sent = 0;
    new->tv_sec = 0;
    new->tv_usec = 0;
    new->next = NULL;
    new->previous = list;
    list->next = new;
    list = new;
  }
  UNLOCKA(&p->fbus2_outgoing_list_lock);
  POPPA_UNLOCKA(&p->fbus2_outgoing_list_lock);
  return list;
}

int celliax_serial_list_print_FBUS2(struct celliax_pvt *p, struct fbus2_msg *list)
{
  struct fbus2_msg *ptr;
  ptr = list;
  while (ptr) {
    if (option_debug > 3)
      DEBUGA_FBUS2
        ("PTR msg is: %d, seqnum is %.2X, tv_sec is %d, tv_usec is %d, acknowledged is: %d,"
         " sent is:%d, how_many_sent is: %d\n", CELLIAX_P_LOG, ptr->msg, ptr->seqnum,
         ptr->tv_sec, ptr->tv_usec, ptr->acknowledged, ptr->sent, ptr->how_many_sent);
    ptr = ptr->previous;
  }
  return 0;
}

int celliax_serial_read_FBUS2(struct celliax_pvt *p)
{
  int read_count;
  int select_err;
  fd_set read_fds;
  struct timeval timeout;
  int fbus_mesg = 0;
  int i;

  FD_ZERO(&read_fds);
  FD_SET(p->controldevfd, &read_fds);
  timeout.tv_sec = 0;
  timeout.tv_usec = 50000;

  if ((select_err = select(p->controldevfd + 1, &read_fds, NULL, NULL, &timeout)) > 0) {
    timeout.tv_sec = 0;         //reset the timeout, linux modify it
    timeout.tv_usec = 50000;    //reset the timeout, linux modify it
    PUSHA_UNLOCKA(&p->controldev_lock);
    LOKKA(&p->controldev_lock);
    while ((select_err =
            select(p->controldevfd + 1, &read_fds, NULL, NULL, &timeout)) > 0) {
      gettimeofday(&p->fbus2_list_tv, &p->fbus2_list_tz);
      read_count = read(p->controldevfd, p->rxm, 255);

      if (read_count == 0) {
        ERRORA
          ("read 0 bytes!!! Nenormalno! Marking this celliax_serial_device %s as dead, andif it is owned by a channel, hanging up. Maybe the phone is stuck, switched off, power down or battery exhausted\n",
           CELLIAX_P_LOG, p->controldevice_name);
        p->controldev_dead = 1;
        close(p->controldevfd);
        UNLOCKA(&p->controldev_lock);
        if (p->owner) {
          celliax_queue_control(p->owner, AST_CONTROL_HANGUP);
          p->owner->hangupcause = AST_CAUSE_FAILURE;
        }
        return -1;
      }
      if (option_debug > 10) {
        int c;
        char debug_buf[1024];
        char *debug_buf_pos;

        memset(debug_buf, 0, 1024);
        debug_buf_pos = debug_buf;
        for (c = 0; c < read_count; c++) {
          debug_buf_pos += sprintf(debug_buf_pos, "[%.2X] ", p->rxm[c]);
          if (debug_buf_pos > (char *) (&debug_buf + 1000))
            break;
        }
        DEBUGA_FBUS2("%s READ AT seconds=%ld usec=%6ld read_count=%d\n", CELLIAX_P_LOG,
                     debug_buf, p->fbus2_list_tv.tv_sec, p->fbus2_list_tv.tv_usec,
                     read_count);
      }

      for (i = 0; i < read_count; i++) {
        if (p->rxm[i] == FBUS2_DEVICE_PHONE && p->rxm[i - 1] == FBUS2_DEVICE_PC
            && p->rxm[i - 2] == FBUS2_SERIAL_FRAME_ID) {
          /* if we have identified the start of an fbus2 frame sent to us by the phone */
          /* clean the array, copy into it the beginning of the frame, move the counter in the array after the last byte copied */
          memset(p->array, 0, 255);
          p->array[0] = FBUS2_SERIAL_FRAME_ID;
          p->array[1] = FBUS2_DEVICE_PC;
          p->arraycounter = 2;
        }
        if (p->rxm[i] == FBUS2_SERIAL_FRAME_ID && read_count == 1) {    /* quick hack to try to identify the lone char 
                                                                           at the beginning a frame, often returned by 
                                                                           ark3116 based datacables */
          /* if we have identified the start of an fbus2 frame sent to us by the phone */
          /* clean the array, copy into it the beginning of the frame, move the counter in the array after the last byte copied */
          memset(p->array, 0, 255);
          p->arraycounter = 0;
        }

        /* continue copying into the array, until... */
        p->array[p->arraycounter] = p->rxm[i];
        /* we reach the end of the incoming frame, its lenght is in the p->array[5] byte, plus overhead */
        if (p->arraycounter == p->array[5] + 7) {
          /* start categorizing frames */
          int seqnum;
          int known = 0;

          /* ACK frames are always of lenght 10, without padding */
          seqnum = p->array[p->arraycounter - 2];
          /* first step in categorizing frames, look at the general kind of frame, in p->array[3] */
          switch (p->array[3]) {
/****************************************************************/
          case FBUS2_ACK_BYTE:
            /* this is an ACKnowledgement frame sent to us in reply to an item we sent, take note we were ACKnowledged, no need to resend the item */
            if (option_debug > 1)
              DEBUGA_FBUS2("INCOMING ACK, seqack %.2X \n", CELLIAX_P_LOG, seqnum);
            if (seqnum == 0x80) {   /* reset */
              seqnum = 0x00;
              DEBUGA_FBUS2
                ("seqack was 0x80, interpreting as 0x00, first acknowledgement (session begin?) of our first sent item 0x40\n",
                 CELLIAX_P_LOG);
            }
            /* an ACK frame has the same seqnum as the item it acknowledge, minus 0x40, so here we obtain the seqnum of the item that has been ACKnowledged */
            fbus_mesg = seqnum + FBUS2_SEQNUM_MIN;
            /* take note that the item sent was ACKnowledged, so no need to resend it */
            celliax_serial_list_acknowledge_FBUS2(p, fbus_mesg);
            /* this frame has been categorized, bail out from the loop */
            known = 1;
            break;
/****************************************************************/
          case FBUS2_TYPE_CALL_DIVERT:
            if (option_debug > 1)
              DEBUGA_FBUS2("CALL DIVERT SIGNALING seqnum %.2X \n", CELLIAX_P_LOG, seqnum);
            fbus_mesg = FBUS2_TYPE_CALL_DIVERT;
            /* this signal us that we have some settings in line divert, let's use it as activation of the line when we call */
            if (p->interface_state == AST_STATE_DIALING) {
              p->interface_state = AST_STATE_UP;
              p->phone_callflow = CALLFLOW_CALL_ACTIVE;
              ast_setstate(p->owner, AST_STATE_RINGING);
              celliax_queue_control(p->owner, AST_CONTROL_ANSWER);
              if (option_debug)
                DEBUGA_FBUS2
                  ("call is active, I know it's not yet true, but 3310 do not give us remote answer signaling\n",
                   CELLIAX_P_LOG);
            }
            /* this frame has been categorized, bail out from the loop */
            known = 1;
            break;

/****************************************************************/
            /* this kind of frames is an answer to "ask model" actions */
          case FBUS2_TYPE_MODEL_ANSWER:
            if (1) {
              int newline = 0;
              int c = i = 0;
              unsigned char model[10];
              for (i = 10; i < p->arraycounter; i++) {
                if (p->array[i] == '\n')
                  newline++;
                if (newline == 2) {
                  if (p->array[i] != '\n') {
                    model[c] = p->array[i];
                    c++;
                  }
                }
                if (newline == 3) {
                  break;
                }
                if (c == 9)
                  break;
              }
              model[c] = '\0';
              DEBUGA_FBUS2("FBUS2 PHONE MODEL is: %s, inseqnum %.2X \n", CELLIAX_P_LOG,
                           model, seqnum);
            }
            known = 1;
            fbus_mesg = FBUS2_TYPE_MODEL_ANSWER;
            break;
/****************************************************************/
            /* this kind of frames is an answer to "security enabled" actions */
          case FBUS2_TYPE_SECURITY:
            switch (p->array[8]) {
              /* this subkind of frames is an answer to "security enabled" CALL actions */
            case FBUS2_SECURIY_CALL_COMMANDS:
              switch (p->array[9]) {
                /* this sub-subkind of frames tell us that we answered the call */
              case FBUS2_SECURIY_CALL_COMMAND_ANSWER:
                p->interface_state = AST_STATE_UP;
                p->phone_callflow = CALLFLOW_CALL_ACTIVE;

                /* set the channel state to UP, we've answered */
                if (ast_setstate(p->owner, AST_STATE_UP)) {
                  ERRORA("ast_setstate failed, BAD\n", CELLIAX_P_LOG);
                }

                if (option_debug > 1)
                  DEBUGA_FBUS2("ANSWERED CALL, inseqnum %.2X \n", CELLIAX_P_LOG, seqnum);
                known = 1;
                break;
                /* this sub-subkind of frames tell us that we released the call */
              case FBUS2_SECURIY_CALL_COMMAND_RELEASE:
                p->interface_state = AST_STATE_DOWN;
                p->phone_callflow = CALLFLOW_CALL_IDLE;
                if (option_debug > 1)
                  DEBUGA_FBUS2("RELEASED CALL, inseqnum %.2X \n", CELLIAX_P_LOG, seqnum);
                fbus_mesg = CALLFLOW_CALL_RELEASED;
                known = 1;
                break;
              }
              break;
              /* this subkind of frames is an answer to "enable security commands" action */
            case FBUS2_SECURIY_EXTENDED_COMMANDS:
              if (option_debug > 1)
                DEBUGA_FBUS2("SECURITY EXTENDED COMMANDS ON, inseqnum %.2X \n",
                             CELLIAX_P_LOG, seqnum);
              fbus_mesg = FBUS2_SECURIY_EXTENDED_COMMAND_ON;
              known = 1;
              break;
              /* this subkind of frames is an answer to "get IMEI" action */
            case FBUS2_SECURIY_IMEI_COMMANDS:
              if (option_debug > 1)
                DEBUGA_FBUS2("CALLFLOW_GOT_IMEI, inseqnum %.2X \n", CELLIAX_P_LOG,
                             seqnum);
              fbus_mesg = CALLFLOW_GOT_IMEI;
              known = 1;
              break;
            }
            break;
/****************************************************************/
            /* this kind of frames is about SMSs */
          case FBUS2_TYPE_SMS:
            switch (p->array[9]) {
              /* this subkind of frames is about an INCOMING SMS */
            case FBUS2_SMS_INCOMING:
              if (option_debug > 1)
                DEBUGA_FBUS2("SMS, inseqnum %.2X \n", CELLIAX_P_LOG, seqnum);
              known = 1;
              break;
            }
            break;
/****************************************************************/
            /* this kind of frames is about PHONE CALLs */
          case FBUS2_TYPE_CALL:
            switch (p->array[9]) {
              int a;
              /* this subkind of frame is about the CALL has been HUNGUP */
            case FBUS2_CALL_HANGUP:
              p->interface_state = AST_STATE_DOWN;
              p->phone_callflow = CALLFLOW_CALL_IDLE;
              if (option_debug > 1)
                DEBUGA_FBUS2("REMOTE PARTY HANG UP, inseqnum %.2X \n", CELLIAX_P_LOG,
                             seqnum);
              fbus_mesg = CALLFLOW_INCOMING_HANGUP;
              known = 1;
              break;
              /* this subkind of frame is about the remote CALLID (not signaled by 3310) */
            case FBUS2_CALL_CALLID:
              if (option_debug > 1)
                DEBUGA_FBUS2("CALLID, inseqnum %.2X \n", CELLIAX_P_LOG, seqnum);
              memset(p->callid_name, 0, sizeof(p->callid_name));
              memset(p->callid_number, 0, sizeof(p->callid_number));
              for (a = 0; a < p->array[12]; a++) {
                p->callid_number[a] = p->array[12 + a + 1];
              }
              for (a = 0; a < p->array[12 + 1 + p->array[12]] + 1; a++) {
                p->callid_name[a] = p->array[12 + 1 + a + p->array[12] + 1];
              }
              if (option_debug > 1)
                DEBUGA_FBUS2("CALLFLOW_INCOMING_CALLID: name is %s, number is %s\n",
                             CELLIAX_P_LOG,
                             p->callid_name[0] != 1 ? p->callid_name : "not available",
                             p->callid_number[0] ? p->callid_number : "not available");
              fbus_mesg = CALLFLOW_INCOMING_CALLID;
              p->phone_callflow = CALLFLOW_INCOMING_RING;
              p->interface_state = AST_STATE_RING;
              known = 1;
              break;
            }
            break;
/****************************************************************/
            /* this kind of frames is about NETWORK STATUS */
          case FBUS2_TYPE_NETWORK_STATUS:
            switch (p->array[9]) {
              /* this subkind of frames is NETWORK STATUS REGISTERED */
            case FBUS2_NETWORK_STATUS_REGISTERED:
              if (option_debug > 1)
                DEBUGA_FBUS2("NETWORK STATUS REGISTERED, inseqnum %.2X \n", CELLIAX_P_LOG,
                             seqnum);
              if (p->callid_name[0] == 0 && p->owner
                  && p->interface_state != AST_STATE_DOWN) {
                p->interface_state = AST_STATE_DOWN;
                p->phone_callflow = CALLFLOW_CALL_IDLE;
                if (option_debug)
                  NOTICA("We think we are using a nokia3310, so NETWORK STATUS REGISTERED"
                         " is interpreted as REMOTE PARTY HANG UP during a call, because"
                         " Nokia 3310 give no hint about remote hangup. Nokia 3310"
                         " does not signal the CALLID, while other nokias at least put"
                         " callid_name[0]=1 (also if no callid was transmitted by remote"
                         " party), we use this lack of CALLID as a sign of 3310nness."
                         " Outside a call, or when CALLID has been signaled, NETWORK STATUS"
                         " REGISTERED is ignored.\n", CELLIAX_P_LOG);
                fbus_mesg = CALLFLOW_INCOMING_HANGUP;
              }
              known = 1;
              break;
            }
            break;
/****************************************************************/
            /* this kind of frames is about CALL STATUS */
          case FBUS2_TYPE_CALL_STATUS:
            switch (p->array[12]) {
              /* this subkind of frames is about CALL STATUS OFF */
            case FBUS2_CALL_STATUS_OFF:
              p->interface_state = AST_STATE_DOWN;
              p->phone_callflow = CALLFLOW_CALL_IDLE;
              if (option_debug > 1)
                DEBUGA_FBUS2("STATUS call in progress OFF, inseqnum %.2X \n",
                             CELLIAX_P_LOG, seqnum);
              fbus_mesg = CALLFLOW_INCOMING_HANGUP;
              known = 1;
              break;
              /* this subkind of frames is about CALL STATUS ON */
            case FBUS2_CALL_STATUS_ON:
              if (option_debug > 1)
                DEBUGA_FBUS2("STATUS call in progress ON, inseqnum %.2X \n",
                             CELLIAX_P_LOG, seqnum);
              known = 1;
              break;
            }
/****************************************************************/
            break;
          }

          /* categorization of frame is ended, if it has not been recognized, whine */
          if (!known) {
            WARNINGA("FBUS2 MSG UNKNOWN, inseqnum %.2X\n", CELLIAX_P_LOG, seqnum);
          }

          /* let's print our frame */
          if (option_debug > 1) {
            int i;
            char debug_buf[1024];
            char *debug_buf_pos;

            memset(debug_buf, 0, 1024);
            debug_buf_pos = debug_buf;
            for (i = 0; i < p->arraycounter + 1; i++) {
              debug_buf_pos += sprintf(debug_buf_pos, "[%.2X] ", p->array[i]);
              if (debug_buf_pos > (char *) (&debug_buf + 1000))
                break;
            }
            DEBUGA_FBUS2("%s is the RECEIVED FRAME inseqnum %.2X\n", CELLIAX_P_LOG,
                         debug_buf, seqnum);
          }

          /* if the frame we received is not an ACK frame, let's ACKnowledge it */
          if (p->array[0] == FBUS2_SERIAL_FRAME_ID && p->array[3] != FBUS2_ACK_BYTE) {
            celliax_serial_send_ack_FBUS2(p, p->array[3], seqnum);
          }
        }
        p->arraycounter++;
      }
    }
    UNLOCKA(&p->controldev_lock);
    POPPA_UNLOCKA(&p->controldev_lock);
  }
  /* oooops, select returned error, got a kill/cancel or problems with the serial file descriptor */
  if (select_err == -1) {
    if (errno != EINTR) {
      ERRORA
        ("select returned -1 on %s, marking controldev as dead, errno was: %d, error was: %s\n",
         CELLIAX_P_LOG, p->controldevice_name, errno, strerror(errno));
      p->controldev_dead = 1;
      close(p->controldevfd);
      if (p->owner)
        celliax_queue_control(p->owner, AST_CONTROL_HANGUP);
      return -1;
    } else {
      WARNINGA("select returned -1 on %s, errno was: %d, EINTR, error was: %s\n",
               CELLIAX_P_LOG, p->controldevice_name, errno, strerror(errno));
      return 0;
    }
  }
  /* OK, reading done, let's browse the list of pending frames to be sent, and act on it */
  if (celliax_serial_send_if_time_FBUS2(p)) {
    ERRORA("celliax_serial_send_if_time_FBUS2 failed!\n", CELLIAX_P_LOG);
    return -1;
  }

  if (fbus_mesg == CALLFLOW_INCOMING_HANGUP) {
    if (p->owner) {
      celliax_queue_control(p->owner, AST_CONTROL_HANGUP);
      DEBUGA_FBUS2("phone call ended\n", CELLIAX_P_LOG);
    }
  }

  return fbus_mesg;             //FIXME breaks the convention of returning 0 on success
}

#endif /* CELLIAX_FBUS2 */

#ifdef CELLIAX_CVM

int celliax_serial_sync_CVM_BUSMAIL(struct celliax_pvt *p)
{
  usleep(1000);                 /* 1msec */
  time(&p->celliax_serial_synced_timestamp);
  return 0;
}

int celliax_serial_answer_CVM_BUSMAIL(struct celliax_pvt *p)
{
  if (AST_STATE_RING == p->interface_state) {
    DEBUGA_CVM("Sending commands to answer an incomming call...\n", CELLIAX_P_LOG);
    celliax_serial_send_info_frame_CVM_BUSMAIL(p, API_PP_CONNECT_REQ, 0, NULL);

  } else {
    DEBUGA_CVM
      ("SKIPPING Sending commands to answer an incomming call, because: !AST_STATE_RING\n",
       CELLIAX_P_LOG);
  }

  return 0;
}

int celliax_serial_call_CVM_BUSMAIL(struct celliax_pvt *p, char *dstr)
{
  unsigned char bCallType = 0x01;   /* INTERNAL */

  unsigned char DialReqBuff[2];

  celliax_serial_send_info_frame_CVM_BUSMAIL(p, API_PP_SETUP_REQ, sizeof(bCallType),
                                             &bCallType);

  while (AST_STATE_DOWN == p->interface_state) {
    usleep(10000);              //10msec
  }

  if (AST_STATE_DIALING == p->interface_state) {
    /* as for now, we only support internal calls */
    /* "0" - call speaker phone */
    /* "1" - call handset #1 */
    /* "2" - call handset #2 */
    /* ... */

    DialReqBuff[0] = 1;         /* number of digits to send */
    DialReqBuff[1] = dstr[0];   /* digit to send */

    celliax_serial_send_info_frame_CVM_BUSMAIL(p, API_PP_KEYPAD_REQ, 2, DialReqBuff);
  }

  if (option_debug)
    NOTICA("CVM_BUSMAIL: sent commands to call\n", CELLIAX_P_LOG);
  return 0;
}

int celliax_serial_hangup_CVM_BUSMAIL(struct celliax_pvt *p)
{
  unsigned char bReason = 0x0;  /* Normal hang-up */

  if (p->interface_state != AST_STATE_DOWN) {
    celliax_serial_send_info_frame_CVM_BUSMAIL(p, API_PP_RELEASE_REQ, sizeof(bReason),
                                               &bReason);

    DEBUGA_CVM("CVM_BUSMAIL: sent commands to hangup the call\n", CELLIAX_P_LOG);

  } else {
    DEBUGA_CVM("CVM_BUSMAIL: sent commands to hangup skipped because: AST_STATE_DOWN\n",
               CELLIAX_P_LOG);
  }

  return 0;
}

int celliax_serial_config_CVM_BUSMAIL(struct celliax_pvt *p)
{
  int res;
  int how_many_reads = 0;
  unsigned char SubcriptionNo = p->cvm_subsc_no;
  unsigned char RegistartionData[5];

  p->cvm_lock_state = CVM_UNKNOWN_LOCK_STATE;
  p->cvm_register_state = CVM_UNKNOWN_REGISTER_STATE;

  PUSHA_UNLOCKA(&p->controldev_lock);
  CVM_LOKKA(&p->controldev_lock);

  if (option_debug > 1)
    DEBUGA_CVM("Try to init communication with CVM...\n", CELLIAX_P_LOG);

  /* CVM after reset sends SABM CTRL frame, let's assume that CVM already sent it, that's the reply... */
  celliax_serial_send_ctrl_frame_CVM_BUSMAIL(p,
                                             BUSMAIL_HEADER_CTRL_FRAME |
                                             BUSMAIL_HEADER_CTRL_UN_FRAME |
                                             BUSMAIL_HEADER_UNID_SABM);
  /*  usleep(10000);   *//* 10ms */

  /* Now we are sending SABM CTRL frame, if CVM is out there, it should reply... */
  celliax_serial_send_ctrl_frame_CVM_BUSMAIL(p,
                                             BUSMAIL_HEADER_CTRL_FRAME |
                                             BUSMAIL_HEADER_CTRL_UN_FRAME |
                                             BUSMAIL_HEADER_UNID_SABM |
                                             (BUSMAIL_HEADER_PF_BIT_MASK & 0xFF));
//  usleep(1000);

  res = celliax_serial_read_CVM_BUSMAIL(p); //we don't have no monitor neither do_controldev_thread

  DEBUGA_CVM("celliax_serial_read_CVM_BUSMAIL res= %X, expected %X\n", CELLIAX_P_LOG, res,
             (BUSMAIL_HEADER_CTRL_FRAME | BUSMAIL_HEADER_CTRL_UN_FRAME |
              BUSMAIL_HEADER_SABM));

  if (res == -1) {
    ERRORA("failed celliax_serial_read_CVM_BUSMAIL\n", CELLIAX_P_LOG);
    CVM_UNLOCKA(&p->controldev_lock);
    return -1;
  }

  how_many_reads = 0;

  while ((res & 0xF0) !=
         (BUSMAIL_HEADER_CTRL_FRAME | BUSMAIL_HEADER_CTRL_UN_FRAME | BUSMAIL_HEADER_SABM))
  {

    usleep(1000);
    res = celliax_serial_read_CVM_BUSMAIL(p);
    how_many_reads++;

    if (res == -1) {
      ERRORA("failed celliax_serial_read_CVM_BUSMAIL\n", CELLIAX_P_LOG);
      CVM_UNLOCKA(&p->controldev_lock);
      return -1;
    }

    if (how_many_reads > 10) {
      ERRORA("no expected results in %d celliax_serial_read_CVM_BUSMAIL\n", CELLIAX_P_LOG,
             how_many_reads);

      ERRORA("Unable to initialize cmmunication with CVM...\n", CELLIAX_P_LOG);

      CVM_UNLOCKA(&p->controldev_lock);
      return -1;
    }
  }

  DEBUGA_CVM("Communication with CVM initialized successfully...\n", CELLIAX_P_LOG);

  DEBUGA_CVM("Attempt to lock to FP...\n", CELLIAX_P_LOG);

  /* Try to connect to FP, try to lock to FP, maybe we registered with it in the past... */
  /* CVM can hold up to 2 subscriptions in its EEPROM, celliax.conf contains number we should try */
  /* eg. cvm_subscription_no = 1 */

  celliax_serial_send_info_frame_CVM_BUSMAIL(p, API_PP_LOCK_REQ, sizeof(SubcriptionNo),
                                             &SubcriptionNo);

  usleep(10000);

  res = celliax_serial_read_CVM_BUSMAIL(p); //we don't have no monitor neither do_controldev_thread

  if (res == -1) {
    ERRORA("failed celliax_serial_read_CVM_BUSMAIL\n", CELLIAX_P_LOG);
    CVM_UNLOCKA(&p->controldev_lock);
    return -1;
  }

  how_many_reads = 0;

  while (CVM_UNKNOWN_LOCK_STATE == p->cvm_lock_state) {

/*    
    if (0 == (how_many_reads % 10))
    {
      DEBUGA_CVM("Attempt to lock to FP... %d\n", CELLIAX_P_LOG, how_many_reads/10 );
      celliax_serial_send_info_frame_CVM_BUSMAIL(p, API_PP_LOCK_REQ, sizeof(SubcriptionNo) ,&SubcriptionNo);
    }
*/

    usleep(100000);

    res = celliax_serial_read_CVM_BUSMAIL(p);
    how_many_reads++;

    if (res == -1) {
      ERRORA("failed celliax_serial_read_CVM_BUSMAIL\n", CELLIAX_P_LOG);
      CVM_UNLOCKA(&p->controldev_lock);
      return -1;
    }

    if (how_many_reads > 50) {
      ERRORA("no expected results in %d celliax_serial_read_CVM_BUSMAIL\n", CELLIAX_P_LOG,
             how_many_reads);

      ERRORA("Unable to lock to FP...\n", CELLIAX_P_LOG);
      break;
    }
  }

  if (CVM_LOCKED_TO_FP == p->cvm_lock_state) {
    DEBUGA_CVM("CVM locked to FP successfully...\n", CELLIAX_P_LOG);
  } else {
    DEBUGA_CVM("Lock to FP failed, Attempt to register to FP...\n", CELLIAX_P_LOG);

    RegistartionData[0] = SubcriptionNo;
    RegistartionData[1] = 0xFF;
    RegistartionData[2] = 0xFF;

    if (1 == SubcriptionNo) {
      RegistartionData[3] =
        (((p->cvm_subsc_1_pin[3] - 0x30) & 0x0F) << 4) | ((p->cvm_subsc_1_pin[2] -
                                                           0x30) & 0x0F);
      RegistartionData[4] =
        (((p->cvm_subsc_1_pin[1] - 0x30) & 0x0F) << 4) | ((p->cvm_subsc_1_pin[0] -
                                                           0x30) & 0x0F);
    } else {
      RegistartionData[3] =
        (((p->cvm_subsc_2_pin[3] - 0x30) & 0x0F) << 4) | ((p->cvm_subsc_2_pin[2] -
                                                           0x30) & 0x0F);
      RegistartionData[4] =
        (((p->cvm_subsc_2_pin[1] - 0x30) & 0x0F) << 4) | ((p->cvm_subsc_2_pin[0] -
                                                           0x30) & 0x0F);
    }

    celliax_serial_send_info_frame_CVM_BUSMAIL(p, API_PP_ACCESS_RIGHTS_REQ,
                                               sizeof(RegistartionData),
                                               RegistartionData);

    usleep(100000);

    res = celliax_serial_read_CVM_BUSMAIL(p);   //we don't have no monitor neither do_controldev_thread

    if (res == -1) {
      ERRORA("failed celliax_serial_read_CVM_BUSMAIL\n", CELLIAX_P_LOG);
      CVM_UNLOCKA(&p->controldev_lock);
      return -1;
    }

    how_many_reads = 0;

    while (CVM_UNKNOWN_REGISTER_STATE == p->cvm_register_state) {

      if (0 == (how_many_reads % 50)) {
        DEBUGA_CVM("Attempt to register to FP... %d\n", CELLIAX_P_LOG,
                   how_many_reads / 10);
        celliax_serial_send_info_frame_CVM_BUSMAIL(p, API_PP_ACCESS_RIGHTS_REQ,
                                                   sizeof(RegistartionData),
                                                   RegistartionData);
      }

      /* up to 5 minutes for registration.... */
      usleep(1000000);
      res = celliax_serial_read_CVM_BUSMAIL(p);
      how_many_reads++;

      if (res == -1) {
        ERRORA("failed celliax_serial_read_CVM_BUSMAIL\n", CELLIAX_P_LOG);
        CVM_UNLOCKA(&p->controldev_lock);
        return -1;
      }

      if (how_many_reads > 300) {
        ERRORA("no expected results in %d celliax_serial_read_CVM_BUSMAIL\n",
               CELLIAX_P_LOG, how_many_reads);

        ERRORA("Unable to communication with CVM...\n", CELLIAX_P_LOG);

        CVM_UNLOCKA(&p->controldev_lock);
        return -1;
      }
    }

    if (CVM_REGISTERED_TO_FP != p->cvm_register_state) {
      ERRORA("Unable to register to FP...\n", CELLIAX_P_LOG);

      CVM_UNLOCKA(&p->controldev_lock);
      return -1;

    } else {
      DEBUGA_CVM("CVM registered to FP successfully...\n", CELLIAX_P_LOG);
      DEBUGA_CVM("Attempt to lock to FP...\n", CELLIAX_P_LOG);

      /* Try to connect to FP, try to lock to FP, maybe we registered with it in the past... */
      /* CVM can hold up to 2 subscriptions in its EEPROM, celliax.conf contains number we should try */
      /* eg. cvm_subscription_no = 1 */

      celliax_serial_send_info_frame_CVM_BUSMAIL(p, API_PP_LOCK_REQ,
                                                 sizeof(SubcriptionNo), &SubcriptionNo);

      usleep(10000);

      res = celliax_serial_read_CVM_BUSMAIL(p); //we don't have no monitor neither do_controldev_thread

      if (res == -1) {
        ERRORA("failed celliax_serial_read_CVM_BUSMAIL\n", CELLIAX_P_LOG);
        CVM_UNLOCKA(&p->controldev_lock);
        return -1;
      }

      how_many_reads = 0;

      while (CVM_UNKNOWN_LOCK_STATE == p->cvm_lock_state) {

        if (0 == (how_many_reads % 10)) {
          DEBUGA_CVM("Attempt to lock to FP... %d\n", CELLIAX_P_LOG, how_many_reads / 10);
          celliax_serial_send_info_frame_CVM_BUSMAIL(p, API_PP_ACCESS_RIGHTS_REQ,
                                                     sizeof(RegistartionData),
                                                     RegistartionData);
        }

        usleep(10000);
        res = celliax_serial_read_CVM_BUSMAIL(p);
        how_many_reads++;

        if (res == -1) {
          ERRORA("failed celliax_serial_read_CVM_BUSMAIL\n", CELLIAX_P_LOG);
          CVM_UNLOCKA(&p->controldev_lock);
          return -1;
        }

        if (how_many_reads > 100) {
          ERRORA("no expected results in %d celliax_serial_read_CVM_BUSMAIL\n",
                 CELLIAX_P_LOG, how_many_reads);

          ERRORA("Unable to communication with CVM...\n", CELLIAX_P_LOG);

          CVM_UNLOCKA(&p->controldev_lock);
          return -1;
        }
      }

      if (CVM_LOCKED_TO_FP != p->cvm_lock_state) {
        ERRORA("Unable to lock to FP...\n", CELLIAX_P_LOG);

        CVM_UNLOCKA(&p->controldev_lock);
        return -1;
      } else {
        DEBUGA_CVM("CVM locked to FP successfully...\n", CELLIAX_P_LOG);
      }
    }
  }

  usleep(100000);

  CVM_UNLOCKA(&p->controldev_lock);
  POPPA_UNLOCKA(&p->controldev_lock);
  return 0;

}

int celliax_serial_read_CVM_BUSMAIL(struct celliax_pvt *p)
{
  int read_count;
  int select_err;
  fd_set read_fds;
  struct timeval timeout;
  int cvm_busmail_mesg = 0;
  unsigned char busmail_crc = 0;
  unsigned char MsgCrc = 0;
  unsigned char MsgHeader = 0;
  unsigned char MsgTxSeqNo = 0;
  unsigned char MsgRxSeqNo = 0;
  unsigned char MsgTaskId = 0;
  unsigned char MsgProgId = 0;
  unsigned char MsgPrimitiveLSB = 0;
  unsigned char MsgPrimitiveMSB = 0;
  unsigned int MsgPrimitive = 0;

  int i = 0;

  FD_ZERO(&read_fds);
  FD_SET(p->controldevfd, &read_fds);
  timeout.tv_sec = 0;
  timeout.tv_usec = 10000;

  if ((select_err = select(p->controldevfd + 1, &read_fds, NULL, NULL, &timeout)) > 0) {
    timeout.tv_sec = 0;         //reset the timeout, linux modify it
    timeout.tv_usec = 10000;    //reset the timeout, linux modify it
    PUSHA_UNLOCKA(&p->controldev_lock);
    CVM_LOKKA(&p->controldev_lock);

    while ((select_err =
            select(p->controldevfd + 1, &read_fds, NULL, NULL, &timeout)) > 0) {
      gettimeofday(&p->cvm_busmail_list_tv, &p->cvm_busmail_list_tz);
      read_count = read(p->controldevfd, p->rxm, 255);

      if (read_count == 0) {
        ERRORA
          ("read 0 bytes!!! Nenormalno! Marking this celliax_serial_device %s as dead, andif it is owned by a channel, hanging up. Maybe the CVM is stuck, switched off or power down.\n",
           CELLIAX_P_LOG, p->controldevice_name);

        p->controldev_dead = 1;
        close(p->controldevfd);
        CVM_UNLOCKA(&p->controldev_lock);

        if (p->owner)
          celliax_queue_control(p->owner, AST_CONTROL_HANGUP);
        return -1;
      }

      if (option_debug > 10) {
        char debug_buf[1024];
        char *debug_buf_pos;

        memset(debug_buf, 0, 1024);
        debug_buf_pos = debug_buf;
        for (i = 0; i < read_count; i++) {
          debug_buf_pos += sprintf(debug_buf_pos, "[%.2X] ", p->rxm[i]);
          if (debug_buf_pos > (char *) (&debug_buf + 1000))
            break;
        }

        DEBUGA_CVM("%s READ AT seconds=%ld usec=%6ld read_count=%d\n", CELLIAX_P_LOG,
                   debug_buf, p->cvm_busmail_list_tv.tv_sec,
                   p->cvm_busmail_list_tv.tv_usec, read_count);
      }

      for (i = 0; i < read_count; i++) {
        if (p->rxm[i] == BUSMAIL_SOF) {
          /* if we have identified the start of an busmail frame sent to us by the CVM */
          /* clean the array, copy into it the beginning of the frame, move the counter in the array after the last byte copied */
          memset(p->array, 0, 255);
          p->array[0] = p->rxm[i];
          p->arraycounter = 1;
        }

        /* buffer overload protection */
        if (255 == p->arraycounter) {
          p->arraycounter = 1;
        }

        /* continue copying into the array, until... */
        p->array[p->arraycounter - 1] = p->rxm[i];

        /* we reach the end of the incoming frame, its lenght is in the p->array[BUSMAIL_OFFSET_LEN_LSB] byte, plus overhead */
        if (p->arraycounter == p->array[BUSMAIL_OFFSET_LEN_LSB] + 4) {

          tcflush(p->controldevfd, TCIFLUSH);   /* PL2303HX bug? */
          /* start categorizing frames */

          if (option_debug > 10) {
            char debug_buf[1024];
            char *debug_buf_pos;

            memset(debug_buf, 0, 1024);
            debug_buf_pos = debug_buf;

            for (i = 0; i < p->arraycounter; i++) {
              debug_buf_pos += sprintf(debug_buf_pos, "[%.2X] ", p->array[i]);
              if (debug_buf_pos > (char *) (&debug_buf + 1000))
                break;
            }

            DEBUGA_CVM("%s was received, Starting to categorize this frame\n",
                       CELLIAX_P_LOG, debug_buf);
          }

          int known = 0;
          int j = 0;

          busmail_crc = 0;
          MsgCrc = p->array[p->arraycounter - 1];

          busmail_crc = (unsigned char) (p->array[BUSMAIL_OFFSET_HEADER] + busmail_crc);

          for (j = BUSMAIL_OFFSET_MAIL; j < (p->arraycounter - 1); j++) {
            busmail_crc = (unsigned char) (p->array[j] + busmail_crc);
          }

          if (busmail_crc != MsgCrc) {
            WARNINGA("BUSMAIL MSG CRC FAILED!, MsgCrc %.2X, calcd %.2X, dropping frame\n",
                     CELLIAX_P_LOG, MsgCrc, busmail_crc);
          } else {
            /* first step in categorizing frames, look at the general kind of frame, in p->array[BUSMAIL_OFFSET_HEADER] */
            if (option_debug > 1)
              DEBUGA_CVM("BUSMAIL MSG CRC, MsgCrc %.2X, calcd %.2X...\n", CELLIAX_P_LOG,
                         MsgCrc, busmail_crc);

            MsgHeader = p->array[BUSMAIL_OFFSET_HEADER];
            cvm_busmail_mesg = MsgHeader;

            switch (MsgHeader & BUSMAIL_HEADER_IC_BIT_MASK) {
            case BUSMAIL_HEADER_INFO_FRAME:
              /* analyzis of frame header */
              MsgTxSeqNo = ((MsgHeader & BUSMAIL_HEADER_TXSEQ_MASK) >> 4);
              MsgRxSeqNo = ((MsgHeader & BUSMAIL_HEADER_RXSEQ_MASK));

              if (option_debug > 1)
                DEBUGA_CVM("BUSMAIL_HEADER_INFO_FRAME TxSeq %X, RxSeq %X\n",
                           CELLIAX_P_LOG, MsgTxSeqNo, MsgRxSeqNo);

              if (((p->busmail_rxseq_cvm_last + 1) & 0x7) != MsgTxSeqNo) {
                /* some CVM frames are missing, TxSeq of this frame is higher then expected */
                /* reject, I expected p->busmail_rxseq_cvm_last + 1, resend it to me, please */

                WARNINGA("CVM TxSeq %X, does not match expected value %X\n",
                         CELLIAX_P_LOG, MsgTxSeqNo,
                         (p->busmail_rxseq_cvm_last + 1) & 0x7);

//                celliax_serial_send_ctrl_frame_CVM_BUSMAIL(p, BUSMAIL_HEADER_CTRL_FRAME | BUSMAIL_HEADER_CTRL_SU_FRAME | BUSMAIL_HEADER_SUID_REJ);

                if (((p->busmail_rxseq_cvm_last) & 0x7) == MsgTxSeqNo) {

                  WARNINGA
                    ("It looks like our ACK to this frame was MIA :), lets ACK the frame one more time...\n",
                     CELLIAX_P_LOG);

                  /* if the frame we received informs us that other side is waiting for ACK, let's ACK it */
                  /* even if it is unknown to us */
                  if (p->array[BUSMAIL_OFFSET_HEADER] & BUSMAIL_HEADER_PF_BIT_MASK) {
                    if (BUSMAIL_HEADER_SABM ==
                        (p->array[BUSMAIL_OFFSET_HEADER] & BUSMAIL_HEADER_SABM_MASK)) {
                      celliax_serial_send_ctrl_frame_CVM_BUSMAIL(p, (unsigned char)
                                                                 (BUSMAIL_HEADER_CTRL_FRAME | BUSMAIL_HEADER_CTRL_UN_FRAME | BUSMAIL_HEADER_UNID_SABM));
                    } else {
                      celliax_serial_send_ctrl_frame_CVM_BUSMAIL(p, (unsigned char)
                                                                 (BUSMAIL_HEADER_CTRL_FRAME | BUSMAIL_HEADER_CTRL_SU_FRAME | BUSMAIL_HEADER_SUID_RR));
                    }
                  }
                }

                break;
              } else {
                /* we expected packet with this seq no. */
                /* CVM ACKed our frames with info frame */
                celliax_serial_list_acknowledge_CVM_BUSMAIL(p, MsgRxSeqNo);

                /* save it but limit it to 3 bits only (valid values: 0-7) */
                p->busmail_rxseq_cvm_last = MsgTxSeqNo;
                p->busmail_rxseq_cvm_last &= 0x7;

                /* if the frame we received informs us that other side is waiting for ACK, let's ACK it */
                /* even if it is unknown to us */
                if (p->array[BUSMAIL_OFFSET_HEADER] & BUSMAIL_HEADER_PF_BIT_MASK) {
                  if (BUSMAIL_HEADER_SABM ==
                      (p->array[BUSMAIL_OFFSET_HEADER] & BUSMAIL_HEADER_SABM_MASK)) {
                    celliax_serial_send_ctrl_frame_CVM_BUSMAIL(p, (unsigned char)
                                                               (BUSMAIL_HEADER_CTRL_FRAME
                                                                |
                                                                BUSMAIL_HEADER_CTRL_UN_FRAME
                                                                |
                                                                BUSMAIL_HEADER_UNID_SABM));
                  } else {
                    celliax_serial_send_ctrl_frame_CVM_BUSMAIL(p, (unsigned char)
                                                               (BUSMAIL_HEADER_CTRL_FRAME
                                                                |
                                                                BUSMAIL_HEADER_CTRL_SU_FRAME
                                                                |
                                                                BUSMAIL_HEADER_SUID_RR));
                  }
                }

              }

              /* frame header OK, let's see what's inside mail field */
              MsgTaskId = p->array[BUSMAIL_OFFSET_MAIL_TASK_ID];
              MsgProgId = p->array[BUSMAIL_OFFSET_MAIL_PROGRAM_ID];
              MsgPrimitiveLSB = p->array[BUSMAIL_OFFSET_MAIL_PRIMITIVE_LSB];
              MsgPrimitiveMSB = p->array[BUSMAIL_OFFSET_MAIL_PRIMITIVE_MSB];
              MsgPrimitive = MsgPrimitiveMSB << 8 | MsgPrimitiveLSB;

              if (option_debug > 1)
                DEBUGA_CVM
                  ("BUSMAIL_HEADER_INFO_FRAME ProgId %X, TaskId %X, Primitive %X %X\n",
                   CELLIAX_P_LOG, MsgProgId, MsgTaskId, MsgPrimitiveMSB, MsgPrimitiveLSB);

              switch (MsgPrimitive) {

              case API_PP_ACCESS_RIGHTS_REJ:
                /* FP rejected our registration... */
                WARNINGA("API_PP_ACCESS_RIGHTS_REJ, FP rejected our registration...\n",
                         CELLIAX_P_LOG);

                p->cvm_register_state = CVM_UNREGISTERED_TO_FP;
                p->cvm_lock_state = CVM_UNKNOWN_LOCK_STATE;
                known = 1;
                break;

              case API_PP_ACCESS_RIGHTS_CFM:
                /* FP accepted our registration... */
                if (option_debug > 1)
                  DEBUGA_CVM
                    ("API_PP_ACCESS_RIGHTS_CFM, FP accepted our registration...\n",
                     CELLIAX_P_LOG);

                p->cvm_register_state = CVM_REGISTERED_TO_FP;
                p->cvm_lock_state = CVM_UNKNOWN_LOCK_STATE;
                p->cvm_handset_no = p->array[BUSMAIL_OFFSET_MAIL_PARAMS + 0];
                p->cvm_fp_is_cvm = p->array[BUSMAIL_OFFSET_MAIL_PARAMS + 1];

                if (option_debug > 1)
                  DEBUGA_CVM
                    ("API_PP_ACCESS_RIGHTS_CFM, FP accepted our registration, Our handset no. is %d, CVM? %X\n",
                     CELLIAX_P_LOG, p->cvm_handset_no, p->cvm_fp_is_cvm);

                known = 1;
                break;

              case API_PP_LOCKED_IND:
                /* CVM is connected to FP */
                if (option_debug > 1)
                  DEBUGA_CVM("API_PP_LOCKED_IND, Connection to FP completed...\n",
                             CELLIAX_P_LOG);

                p->cvm_register_state = CVM_REGISTERED_TO_FP;
                p->cvm_lock_state = CVM_LOCKED_TO_FP;
                known = 1;
                break;

              case API_PP_UNLOCKED_IND:
                /* CVM is unlocked with FP, Out of service */
                WARNINGA
                  ("API_PP_UNLOCKED_IND, CVM is unlocked with FP, Out of service !!!\n",
                   CELLIAX_P_LOG);

                p->cvm_lock_state = CVM_UNLOCKED_TO_FP;
                known = 1;
                break;

              case API_PP_SETUP_ACK_IND:
                /* Outgoing call, connection to FP established, FP is waiting for a number to dial */
                if (option_debug > 1)
                  DEBUGA_CVM
                    ("API_PP_SETUP_ACK_IND, connection to FP established, FP is waiting for a numer to dial...\n",
                     CELLIAX_P_LOG);

                if (AST_STATE_DOWN == p->interface_state) {
                  p->interface_state = AST_STATE_DIALING;
                }

                known = 1;
                break;

              case API_PP_ALERT_IND:
                /* Outgoing call, Remote end is ringing */
                if (option_debug > 1)
                  DEBUGA_CVM("API_PP_ALERT_IND, remote end is ringing...\n",
                             CELLIAX_P_LOG);

                if (AST_STATE_DIALING == p->interface_state) {
                  p->interface_state = AST_STATE_RINGING;
                }

                known = 1;
                break;

              case API_PP_CONNECT_IND:
                /* Outgoing call, the remote end answered our call */
                if (option_debug > 1)
                  DEBUGA_CVM("API_PP_CONNECT_IND, our call was answered...\n",
                             CELLIAX_P_LOG);

                if (AST_STATE_RINGING == p->interface_state) {

                  /* let's open audio and have a chat */
                  celliax_serial_send_info_frame_CVM_BUSMAIL(p, CVM_PP_AUDIO_OPEN_REQ, 0,
                                                             NULL);

                  unsigned char volume = (unsigned char) p->cvm_volume_level;
                  celliax_serial_send_info_frame_CVM_BUSMAIL(p,
                                                             CVM_PP_AUDIO_SET_VOLUME_REQ,
                                                             sizeof(volume), &volume);

                  /* let's unmute mic and have a chat */
                  celliax_serial_send_info_frame_CVM_BUSMAIL(p,
                                                             CVM_PP_AUDIO_UNMUTE_MIC_REQ,
                                                             0, NULL);

                  /* let's switch to headset, because we fried normal output.... */
/*                  unsigned char headset_on = (unsigned char) 1;
                  celliax_serial_send_info_frame_CVM_BUSMAIL(p, CVM_PP_AUDIO_HS_PLUG_IND, sizeof(headset_on), &headset_on);
*/
                  p->interface_state = AST_STATE_UP;
                  ast_setstate(p->owner, AST_STATE_RINGING);
                  celliax_queue_control(p->owner, AST_CONTROL_ANSWER);
                }

                known = 1;
                break;

              case API_PP_REJECT_IND:
                /* Outgoing/Incoming call, FP rejected our connection... */
                if (option_debug > 1)
                  DEBUGA_CVM
                    ("API_PP_REJECT_IND, FP or ther PP rejected our connection...\n",
                     CELLIAX_P_LOG);

                if (AST_STATE_RING == p->interface_state && p->owner) {
                  /* Attempt to answer incoming call rejected by FP or PP */
                  if (option_debug > 1)
                    DEBUGA_CVM("Was it PAGE_ALL CALL, that we should not answered?\n",
                               CELLIAX_P_LOG);

                  p->interface_state = AST_STATE_DOWN;
                  celliax_queue_control(p->owner, AST_CONTROL_HANGUP);

                } else if (AST_STATE_DOWN != p->interface_state && p->owner) {
                  /* Outgoing call rejected by other PP or FP */
                  p->interface_state = AST_STATE_BUSY;
                  ast_setstate(p->owner, AST_STATE_BUSY);
                  celliax_queue_control(p->owner, AST_CONTROL_BUSY);
                }

                known = 1;
                break;

              case API_PP_SIGNAL_ON_IND:
                /* Ringback, ignore it... */
                if (option_debug > 1)
                  DEBUGA_CVM("API_PP_SIGNAL_ON_IND, Ringback, ignore it...\n",
                             CELLIAX_P_LOG);

                known = 1;
                break;

              case API_PP_SIGNAL_OFF_IND:
                /* Ringback, ignore it... */
                if (option_debug > 1)
                  DEBUGA_CVM("API_PP_SIGNAL_OFF_IND, Ringback, ignore it...\n",
                             CELLIAX_P_LOG);

                known = 1;
                break;

              case API_PP_SETUP_IND:
                /* Incoming call, Somebody is calling us */

                if (option_debug > 1)
                  DEBUGA_CVM("API_PP_SETUP_IND, somebody is calling us...\n",
                             CELLIAX_P_LOG);

                if (AST_STATE_DOWN == p->interface_state) {

                  if (API_PP_SETUP_IND_CALL_INT ==
                      p->array[BUSMAIL_OFFSET_MAIL_PARAMS +
                               API_PP_SETUP_IND_CALL_TYPE_OFFSET]) {
                    DEBUGA_CVM("INTERNAL CALL, receive it...\n", CELLIAX_P_LOG);

                    p->interface_state = AST_STATE_RING;

                    /* inform calling end, that we know about his call, and that we are alerting */
                    celliax_serial_send_info_frame_CVM_BUSMAIL(p, API_PP_ALERT_REQ, 0,
                                                               NULL);

                    /* let's open audio before valid mac, to remove noise... */
                    celliax_serial_send_info_frame_CVM_BUSMAIL(p,
                                                               CVM_PP_AUDIO_OPEN_ADPCM_OFF_REQ,
                                                               0, NULL);

                  } else {
                    DEBUGA_CVM("NOT an INTERNAL CALL, CALL TYPE %X, just ignore it...\n",
                               CELLIAX_P_LOG,
                               p->array[BUSMAIL_OFFSET_MAIL_PARAMS +
                                        API_PP_SETUP_IND_CALL_TYPE_OFFSET]);

                    /* inform calling end, that we know about his call, and that we are alerting OR not :) */
                    /* probably it is needed so FP does not remove us from PP list :) */
                    celliax_serial_send_info_frame_CVM_BUSMAIL(p, API_PP_ALERT_REQ, 0,
                                                               NULL);
                  }

                } else {
                  WARNINGA
                    ("Ignore incoming call, Wrong interface state, current state %X\n",
                     CELLIAX_P_LOG, p->interface_state);
                }

                known = 1;
                break;

              case API_PP_ALERT_OFF_IND:
                /* Incoming call, We should stop alerting about incoming call... */
                if (option_debug > 1)
                  DEBUGA_CVM
                    ("API_PP_ALERT_OFF_IND, Ringback, stop alerting about incoming call...\n",
                     CELLIAX_P_LOG);

                known = 1;
                break;

              case API_PP_ALERT_ON_IND:
                /* Incoming call, We should stop alerting about incoming call... */
                if (option_debug > 1)
                  DEBUGA_CVM
                    ("API_PP_ALERT_ON_IND, Ringback, start alerting about incoming call...\n",
                     CELLIAX_P_LOG);
/*
                if (AST_STATE_DOWN == p->interface_state) {
                  DEBUGA_CVM("Somebody is calling us, we see a PP_ALERT_ON_IND, receive it...\n", CELLIAX_P_LOG);
                  p->interface_state = AST_STATE_RING;
                }
*/
                known = 1;
                break;

              case API_PP_CONNECT_CFM:
                /* Incoming call, Confirmation for request to answer incoming call... */
                if (option_debug > 1)
                  DEBUGA_CVM
                    ("API_PP_CONNECT_CFM, Confirmation for request to answer incoming call...\n",
                     CELLIAX_P_LOG);

                if (AST_STATE_RING == p->interface_state && p->owner) {

                  p->interface_state = AST_STATE_UP;
                  ast_setstate(p->owner, AST_STATE_UP);

                  /* let's open audio and have a chat */
//                  celliax_serial_send_info_frame_CVM_BUSMAIL(p, CVM_PP_AUDIO_OPEN_ADPCM_OFF_REQ, 0, NULL);

                  /* let's open audio and have a chat */
                  celliax_serial_send_info_frame_CVM_BUSMAIL(p, CVM_PP_AUDIO_OPEN_REQ, 0,
                                                             NULL);

                  unsigned char volume = (unsigned char) p->cvm_volume_level;
                  celliax_serial_send_info_frame_CVM_BUSMAIL(p,
                                                             CVM_PP_AUDIO_SET_VOLUME_REQ,
                                                             sizeof(volume), &volume);

                  /* let's unmute mic and have a chat */
                  celliax_serial_send_info_frame_CVM_BUSMAIL(p,
                                                             CVM_PP_AUDIO_UNMUTE_MIC_REQ,
                                                             0, NULL);

                  /* let's switch to headset, because we fried normal output.... */
/*                  unsigned char headset_on = (unsigned char) 1;
                  celliax_serial_send_info_frame_CVM_BUSMAIL(p, CVM_PP_AUDIO_HS_PLUG_IND, sizeof(headset_on), &headset_on);
*/
                } else {
                  WARNINGA
                    ("Ignore connection cfm, Wrong interface state, current state %X\n",
                     CELLIAX_P_LOG, p->interface_state);
                }

                known = 1;
                break;

              case API_PP_RELEASE_CFM:
                /* Confirmation for request to hangup a call... */
                if (option_debug > 1)
                  DEBUGA_CVM
                    ("API_PP_RELEASE_CFM, Confirmation for request to hangup a call..\n",
                     CELLIAX_P_LOG);

                if (AST_STATE_UP == p->interface_state) {
                  /* let's close audio */
                  celliax_serial_send_info_frame_CVM_BUSMAIL(p, CVM_PP_AUDIO_CLOSE_REQ, 0,
                                                             NULL);

                  /* let's unmute mic and have a chat */
                  celliax_serial_send_info_frame_CVM_BUSMAIL(p, CVM_PP_AUDIO_MUTE_MIC_REQ,
                                                             0, NULL);
                }

                p->interface_state = AST_STATE_DOWN;

                known = 1;
                break;

              case API_PP_RELEASE_IND:
                /* FP releases connection to CVM... */
                if (option_debug > 1)
                  DEBUGA_CVM("API_PP_RELEASE_IND, FP releases connection to CVM...\n",
                             CELLIAX_P_LOG);

                if (AST_STATE_UP == p->interface_state && p->owner) {
                  /* let's close audio */
                  celliax_serial_send_info_frame_CVM_BUSMAIL(p, CVM_PP_AUDIO_CLOSE_REQ, 0,
                                                             NULL);
                  p->interface_state = AST_STATE_DOWN;
                  celliax_queue_control(p->owner, AST_CONTROL_HANGUP);

                } else if (AST_STATE_RING == p->interface_state && p->owner) {
                  /* workaround for PAGE ALL CALL, FIXME!!!! */
                  if (option_debug > 1)
                    DEBUGA_CVM("WAS IT A PAGE ALL ???...\n", CELLIAX_P_LOG);

                  p->interface_state = AST_STATE_UP;
                  usleep(100000);

                  p->interface_state = AST_STATE_DOWN;
                  celliax_queue_control(p->owner, AST_CONTROL_HANGUP);
                }

                /* we need to ACK release */
                celliax_serial_send_info_frame_CVM_BUSMAIL(p, API_PP_RELEASE_RES, 0,
                                                           NULL);

                known = 1;
                break;

              case API_PP_READ_RSSI_CFM:
                if (option_debug > 1)
                  DEBUGA_CVM("API_PP_READ_RSSI_CFM, RSSI readout...\n", CELLIAX_P_LOG);

                p->cvm_rssi = p->array[BUSMAIL_OFFSET_MAIL_PARAMS + 0];
                int rssi_percent = p->cvm_rssi * 100 / 0x3F;
                if (option_debug > 1)
                  DEBUGA_CVM("RSSI is %X, %d%%...\n", CELLIAX_P_LOG, p->cvm_rssi,
                             rssi_percent);

                known = 1;
                break;
              default:
                WARNINGA("UNKNOWN MsgPrimitive!!! %X\n", CELLIAX_P_LOG, MsgPrimitive);
                break;
              }

              break;

            case BUSMAIL_HEADER_CTRL_FRAME:
              if (option_debug > 1)
                DEBUGA_CVM("BUSMAIL_HEADER_CTRL_FRAME\n", CELLIAX_P_LOG);

              switch (p->array[BUSMAIL_OFFSET_HEADER] & BUSMAIL_HEADER_SU_BIT_MASK) {
              case BUSMAIL_HEADER_CTRL_SU_FRAME:
                if (option_debug > 1)
                  DEBUGA_CVM("BUSMAIL_HEADER_CTRL_SU_FRAME\n", CELLIAX_P_LOG);

                switch (p->array[BUSMAIL_OFFSET_HEADER] & BUSMAIL_HEADER_SUID_MASK) {
                case BUSMAIL_HEADER_SUID_REJ:
                  /* CVM Reject, CVM missed one of our packets, it will be resend, do nothing */
                  MsgRxSeqNo =
                    ((p->array[BUSMAIL_OFFSET_HEADER] & BUSMAIL_HEADER_RXSEQ_MASK));

                  if (option_debug > 1)
                    DEBUGA_CVM("BUSMAIL_HEADER_SUID_REJ, RxSeq %X\n", CELLIAX_P_LOG,
                               MsgRxSeqNo);

                  /* Even that it is CVM Reject packet, it still ACKs some packets */
                  celliax_serial_list_acknowledge_CVM_BUSMAIL(p, MsgRxSeqNo);

                  known = 1;
                  break;
                case BUSMAIL_HEADER_SUID_RNR:
                  /* CVM Receiver Not Ready, answer to packet that we sent, do nothing, it will be resend later */
                  MsgRxSeqNo =
                    ((p->array[BUSMAIL_OFFSET_HEADER] & BUSMAIL_HEADER_RXSEQ_MASK));

                  if (option_debug > 1)
                    DEBUGA_CVM("BUSMAIL_HEADER_SUID_RNR, RxSeq %X\n", CELLIAX_P_LOG,
                               MsgRxSeqNo);

                  known = 1;
                  break;
                case BUSMAIL_HEADER_SUID_RR:
                  /* CVM ACKs our packets */
                  MsgRxSeqNo =
                    ((p->array[BUSMAIL_OFFSET_HEADER] & BUSMAIL_HEADER_RXSEQ_MASK));

                  if (option_debug > 1)
                    DEBUGA_CVM("BUSMAIL_HEADER_SUID_RR, RxSeq %X\n", CELLIAX_P_LOG,
                               MsgRxSeqNo);

                  /* CVM ACKed our frames with RR frame */
                  celliax_serial_list_acknowledge_CVM_BUSMAIL(p, MsgRxSeqNo);

                  known = 1;
                  break;

                default:
                  WARNINGA("BUSMAIL_HEADER_SUID_UNKNOWN!!!\n", CELLIAX_P_LOG);
                  break;
                }
                break;

              case BUSMAIL_HEADER_CTRL_UN_FRAME:
                if (option_debug > 1)
                  DEBUGA_CVM("BUSMAIL_HEADER_CTRL_UN_FRAME\n", CELLIAX_P_LOG);

                switch (p->array[BUSMAIL_OFFSET_HEADER] & BUSMAIL_HEADER_UNID_MASK) {

                case BUSMAIL_HEADER_UNID_SABM:
                  if (option_debug > 1)
                    DEBUGA_CVM("BUSMAIL_HEADER_UNID_SABM\n", CELLIAX_P_LOG);
                  /* reset seq counters */
                  p->busmail_txseq_celliax_last = 0xFF;
                  p->busmail_rxseq_cvm_last = 0xFF;

                  celliax_serial_lists_free_CVM_BUSMAIL(p);
                  /* if needed, reply will be send by code at the end of switch statements */
                  known = 1;
                  break;

                default:
                  WARNINGA("BUSMAIL_HEADER_UNID_UNKNOWN!!!\n", CELLIAX_P_LOG);
                  break;
                }
                break;

              default:
                WARNINGA("BUSMAIL_HEADER_CTRL_UNKNOWN!!!\n", CELLIAX_P_LOG);
                break;
              }
              break;

            default:
              WARNINGA("BUSMAIL_HEADER_UNKNOWN!!!\n", CELLIAX_P_LOG);
              break;
            }

          }

          /* categorization of frame is ended, if it has not been recognized, whine */
          if (!known) {
            WARNINGA("BUSMAIL MSG UNKNOWN or REJECTED!\n", CELLIAX_P_LOG);
          }
        }
        p->arraycounter++;
      }
    }
    CVM_UNLOCKA(&p->controldev_lock);
    POPPA_UNLOCKA(&p->controldev_lock);
  }

  /* oooops, select returned error, got a kill/cancel or problems with the serial file descriptor */
  if (select_err == -1) {
    if (errno != EINTR) {
      ERRORA
        ("select returned -1 on %s, marking controldev as dead, errno was: %d, error was: %s\n",
         CELLIAX_P_LOG, p->controldevice_name, errno, strerror(errno));

      p->controldev_dead = 1;
      close(p->controldevfd);

      if (p->owner)
        celliax_queue_control(p->owner, AST_CONTROL_HANGUP);
      return -1;

    } else {
      WARNINGA("select returned -1 on %s, errno was: %d, EINTR, error was: %s\n",
               CELLIAX_P_LOG, p->controldevice_name, errno, strerror(errno));
      return 0;
    }
  }
  /* OK, reading done, let's browse the list of pending frames to be sent, and act on it */
  if (celliax_serial_send_if_time_CVM_BUSMAIL(p)) {
    ERRORA("celliax_serial_send_if_time_CVM_BUSMAIL failed!\n", CELLIAX_P_LOG);
    return -1;
  }

  return cvm_busmail_mesg;      //FIXME breaks the convention of returning 0 on success
}

int celliax_serial_getstatus_CVM_BUSMAIL(struct celliax_pvt *p)
{
  int res;
  int how_many_reads = 0;

  PUSHA_UNLOCKA(&p->controldev_lock);
  CVM_LOKKA(&p->controldev_lock);

  if (option_debug > 1)
    DEBUGA_CVM("Sending RR CTRL frame wit PF bit set\n", CELLIAX_P_LOG);

  /* this ctrl frame can be used as low level keep alive */
  celliax_serial_send_ctrl_frame_CVM_BUSMAIL(p,
                                             BUSMAIL_HEADER_CTRL_FRAME |
                                             BUSMAIL_HEADER_CTRL_SU_FRAME |
                                             BUSMAIL_HEADER_SUID_RR |
                                             (BUSMAIL_HEADER_PF_BIT_MASK & 0xFF));

  //usleep(1000);

  res = celliax_serial_read_CVM_BUSMAIL(p); //we don't have no monitor neither do_controldev_thread

  if (res == -1) {
    ERRORA("failed celliax_serial_read_CVM_BUSMAIL\n", CELLIAX_P_LOG);
    CVM_UNLOCKA(&p->controldev_lock);
    return -1;
  }

  while ((res & 0xF0) !=
         (BUSMAIL_HEADER_CTRL_FRAME | BUSMAIL_HEADER_CTRL_SU_FRAME |
          BUSMAIL_HEADER_SUID_RR)) {

    usleep(1000);
    res = celliax_serial_read_CVM_BUSMAIL(p);
    how_many_reads++;

    if (res == -1) {
      ERRORA("failed celliax_serial_read_CVM_BUSMAIL\n", CELLIAX_P_LOG);
      CVM_UNLOCKA(&p->controldev_lock);
      return -1;
    }

    if (how_many_reads > 10) {
      ERRORA("no expected results in %d celliax_serial_read_CVM_BUSMAIL\n", CELLIAX_P_LOG,
             how_many_reads);
      CVM_UNLOCKA(&p->controldev_lock);
      return -1;
    }
  }

  //celliax_serial_send_info_frame_CVM_BUSMAIL(p, API_PP_READ_RSSI_REQ, 0, NULL);

  CVM_UNLOCKA(&p->controldev_lock);
  POPPA_UNLOCKA(&p->controldev_lock);

  return 0;

}

/*!
 * \brief Write on the serial port for all the CVM_BUSMAIL functions
 * \param p celliax_pvt
 * \param len lenght of buffer2
 * \param buffer2 chars to be written
 *
 * Write on the serial port for all the CVM_BUSMAIL functions
 *
 * \return the number of chars written on the serial, 
 * that can be different from len (or negative) in case of errors.
 */
int celliax_serial_send_CVM_BUSMAIL(struct celliax_pvt *p, int len,
                                    unsigned char *mesg_ptr)
{
  int ret;
  size_t actual = 0;
  unsigned char *mesg_ptr2 = mesg_ptr;
  PUSHA_UNLOCKA(&p->controldev_lock);
  CVM_LOKKA(&p->controldev_lock);
  do {
    ret = write(p->controldevfd, mesg_ptr, len - actual);
    if (ret < 0 && errno == EAGAIN)
      continue;
    if (ret < 0) {
      if (actual != len)
        ERRORA("celliax_serial_write error: %s", CELLIAX_P_LOG, strerror(errno));
      CVM_UNLOCKA(&p->controldev_lock);
      return -1;
    }
    actual += ret;
    mesg_ptr += ret;
    usleep(10000);
//    usleep(p->cvm_celliax_serial_delay*1000);
  } while (actual < len);

  usleep(p->cvm_celliax_serial_delay * 1000);

//  tcdrain(p->controldevfd);

  CVM_UNLOCKA(&p->controldev_lock);
  POPPA_UNLOCKA(&p->controldev_lock);

  if (option_debug > 10) {
    int i;
    char debug_buf[1024];
    char *debug_buf_pos;

    memset(debug_buf, 0, 1024);
    debug_buf_pos = debug_buf;

    for (i = 0; i < len; i++) {
      debug_buf_pos += sprintf(debug_buf_pos, "[%.2X] ", mesg_ptr2[i]);
      if (debug_buf_pos > ((char *) &debug_buf + 1000))
        break;
    }
    DEBUGA_CVM("%s was sent down the wire\n", CELLIAX_P_LOG, debug_buf);
  }

  return 0;
}

/*!
 * \brief Flags as acknowledged an BUSMAIL message previously sent
 * \param p celliax_pvt
 * \param seqnum identifier of the message to be acknowledged
 *
 * Called upon receiving an BUSMAIL acknoledgement message, browse the cvm_busmail_outgoing_list 
 * looking for the seqnum sent BUSMAIL message, and flags it as acknowledged.
 * (if an outgoing BUSMAIL message is not aknowledged by the cellphone in a while,
 * it will be retransmitted)
 *
 * \return 0 on error, 1 otherwise
 */
int celliax_serial_list_acknowledge_CVM_BUSMAIL(struct celliax_pvt *p,
                                                unsigned char AckTxSeqNo)
{
  struct cvm_busmail_msg *ptr = NULL;
  struct cvm_busmail_msg *old = NULL;

  unsigned char MsgTxSeqNo;
  unsigned char MsgRxSeqNo;

  ptr = p->cvm_busmail_outgoing_list;

  if (ptr == NULL) {
    ERRORA("cvm_busmail_outgoing_list is NULL ?\n", CELLIAX_P_LOG);
    return -1;
  }

  PUSHA_UNLOCKA(&p->cvm_busmail_outgoing_list_lock);
  CVM_LOKKA(&p->cvm_busmail_outgoing_list_lock);
/*
  DEBUGA_CVM("PREFREE OUTGOING list:\n", CELLIAX_P_LOG);
  celliax_serial_list_print_CVM_BUSMAIL(p, p->cvm_busmail_outgoing_list);
*/
  while (ptr->next != NULL)
    ptr = ptr->next;

  while (ptr) {

    if ((1 == ptr->valid) && (0 == ptr->acknowledged) && (0 != ptr->sent)) {
      MsgTxSeqNo =
        ((ptr->busmail_msg_buffer[BUSMAIL_OFFSET_HEADER] & BUSMAIL_HEADER_TXSEQ_MASK) >>
         4);
      MsgRxSeqNo =
        ((ptr->busmail_msg_buffer[BUSMAIL_OFFSET_HEADER] & BUSMAIL_HEADER_RXSEQ_MASK));

/*
    if (option_debug > 1) 
      DEBUGA_CVM("OUTGOING LIST TxSeq is %X, RxSeq is %X\n", CELLIAX_P_LOG, MsgTxSeqNo, MsgRxSeqNo);
*/
      unsigned char TxToAck = 0;

      if (0 == AckTxSeqNo) {
        TxToAck = 7;
      } else {
        TxToAck = AckTxSeqNo - 1;
      }

      if (MsgTxSeqNo <= TxToAck) {

        if (option_debug > 1)
          DEBUGA_CVM("Msg with TxSeq=%X ACKed with CvmRxSeq=%X\n", CELLIAX_P_LOG,
                     MsgTxSeqNo, AckTxSeqNo);

        old = ptr;
        old->acknowledged = 1;
        old->valid = 0;
        ptr = old->previous;

        if (old->previous) {
          if (old->next) {
            old->previous->next = old->next;
          } else {
            old->previous->next = NULL;
          }
        }

        if (old->next) {
          if (old->previous) {
            old->next->previous = old->previous;
          } else {
            old->next->previous = NULL;
          }
        }

        if ((NULL == old->next) && (NULL == old->previous)) {
          if (option_debug > 1) {
            DEBUGA_CVM("FREEING LAST\n", CELLIAX_P_LOG);
          }

          p->cvm_busmail_outgoing_list = NULL;
          p->cvm_busmail_outgoing_list = celliax_serial_list_init_CVM_BUSMAIL(p);
        }

/*
      if (option_debug > 1)
        DEBUGA_CVM("FREEING TxSeq is %X, RxSeq is %X\n", CELLIAX_P_LOG, MsgTxSeqNo, MsgRxSeqNo);
*/

        free(old);

      } else {
        ptr = ptr->previous;
      }

    } else {
      ptr = ptr->previous;
    }
  }

/*
  DEBUGA_CVM("POSTFREE OUTGOING list:\n", CELLIAX_P_LOG);
  celliax_serial_list_print_CVM_BUSMAIL(p, p->cvm_busmail_outgoing_list);
*/

  CVM_UNLOCKA(&p->cvm_busmail_outgoing_list_lock);
  POPPA_UNLOCKA(&p->cvm_busmail_outgoing_list_lock);
  return 0;
}

/*!
 * \brief Sends an FBUS2 message or resends it if it was not acknowledged
 * \param p celliax_pvt
 *
 * Called by celliax_serial_read_CVM_BUSMAIL, browse the fbus2_outgoing_list looking for FBUS2 messages to be sent, 
 * or for FBUS2 messages previously sent but not yet acknoledged.
 * (if an outgoing FBUS2 message is not aknowledged by the cellphone in a while,
 * it will be retransmitted)
 *
 * \return 0 on error, 1 otherwise
 */
int celliax_serial_send_if_time_CVM_BUSMAIL(struct celliax_pvt *p)
{
  struct cvm_busmail_msg *ptr;
  struct timeval tv;
  struct timezone tz;

  gettimeofday(&tv, &tz);
  ptr = p->cvm_busmail_outgoing_list;

  if (ptr == NULL) {
/*    ERRORA("cvm_busmail_outgoing_list is NULL ?\n", CELLIAX_P_LOG); */
    WARNINGA("cvm_busmail_outgoing_list is NULL, nothing to send...\n", CELLIAX_P_LOG);

/*    return -1; */
    return 0;

  }

  while (ptr->next != NULL) {
    WARNINGA("cvm_busmail_outgoing_list->next is not null ?\n", CELLIAX_P_LOG);
    ptr = ptr->next;            //FIXME what to do?
  }

  while (ptr->sent == 0 && ptr->acknowledged == 0) {
    if (ptr->previous != NULL) {
      ptr = ptr->previous;
    } else
      break;
  }

  while (ptr->sent == 1 && ptr->acknowledged == 0) {
    if (ptr->previous != NULL) {
      ptr = ptr->previous;
    } else
      break;
  }

  if (ptr->sent == 1 && ptr->acknowledged == 1) {
    if (ptr->next != NULL) {
      ptr = ptr->next;
    }
  }

  if (ptr->sent == 1 && ptr->acknowledged == 0 && ptr->valid == 1) {
    if ((tv.tv_sec * 1000 + tv.tv_usec / 1000) >
        ((ptr->tv_sec * 1000 + ptr->tv_usec / 1000) + 1000)) {

      PUSHA_UNLOCKA(&p->cvm_busmail_outgoing_list_lock);
      CVM_LOKKA(&p->cvm_busmail_outgoing_list_lock);

      if (ptr->sent == 1 && ptr->acknowledged == 0 && ptr->valid == 1) {    //retest, maybe has been changed?
        if ((tv.tv_sec * 1000 + tv.tv_usec / 1000) > ((ptr->tv_sec * 1000 + ptr->tv_usec / 1000) + 1000)) { //retest, maybe has been changed?

          if (option_debug > 1)
            DEBUGA_CVM("RESEND TxSeq=%X, passed %ld ms, sent %d times\n", CELLIAX_P_LOG,
                       ptr->txseqno,
                       ((tv.tv_sec * 1000 + tv.tv_usec / 1000) -
                        (ptr->tv_sec * 1000 + ptr->tv_usec / 1000)), ptr->how_many_sent);

          if (ptr->how_many_sent > 9) {
            ERRORA("RESEND TxSeq=%X, passed %ld ms, sent %d times\n", CELLIAX_P_LOG,
                   ptr->txseqno,
                   ((tv.tv_sec * 1000 + tv.tv_usec / 1000) -
                    (ptr->tv_sec * 1000 + ptr->tv_usec / 1000)), ptr->how_many_sent);

            CVM_UNLOCKA(&p->cvm_busmail_outgoing_list_lock);
            return -1;
          }

          celliax_serial_send_CVM_BUSMAIL(p, ptr->busmail_msg_len,
                                          ptr->busmail_msg_buffer);

          ptr->tv_sec = tv.tv_sec;
          ptr->tv_usec = tv.tv_usec;
          ptr->sent = 1;
          ptr->how_many_sent++;
/*
          if (option_debug > 1) {
            DEBUGA_CVM("OUTGOING list:\n", CELLIAX_P_LOG);
            celliax_serial_list_print_CVM_BUSMAIL(p, p->cvm_busmail_outgoing_list);
            DEBUGA_CVM("OUTGOING list END\n", CELLIAX_P_LOG);
          }
*/
        }
      }

      CVM_UNLOCKA(&p->cvm_busmail_outgoing_list_lock);
      POPPA_UNLOCKA(&p->cvm_busmail_outgoing_list_lock);
    }
  }

  if (ptr->sent == 0 && ptr->acknowledged == 0 && ptr->valid == 1) {

    PUSHA_UNLOCKA(&p->cvm_busmail_outgoing_list_lock);
    CVM_LOKKA(&p->cvm_busmail_outgoing_list_lock);

    if (ptr->sent == 0 && ptr->acknowledged == 0 && ptr->valid == 1) {  //retest, maybe has been changed?

      if (option_debug > 1)
        DEBUGA_CVM("SENDING 1st TIME TxSeq=%X\n", CELLIAX_P_LOG, ptr->txseqno);

      celliax_serial_send_CVM_BUSMAIL(p, ptr->busmail_msg_len, ptr->busmail_msg_buffer);

      ptr->tv_sec = tv.tv_sec;
      ptr->tv_usec = tv.tv_usec;
      ptr->sent = 1;
      ptr->how_many_sent++;
/*
      if (option_debug > 1) {
        DEBUGA_CVM("OUTGOING list:\n", CELLIAX_P_LOG);
        celliax_serial_list_print_CVM_BUSMAIL(p, p->cvm_busmail_outgoing_list);
        DEBUGA_CVM("OUTGOING list END\n", CELLIAX_P_LOG);	
      }
*/
    }

    CVM_UNLOCKA(&p->cvm_busmail_outgoing_list_lock);
    POPPA_UNLOCKA(&p->cvm_busmail_outgoing_list_lock);

  }
  return 0;
}

int celliax_serial_write_CVM_BUSMAIL(struct celliax_pvt *p,
                                     struct cvm_busmail_frame *busmail_frame)
{
  unsigned char buffer2[BUSMAIL_MAX_FRAME_LENGTH];
  int i = 0;
  int len = 0;
  unsigned int busmail_len_total = 0;

  busmail_frame->busmail_sof = BUSMAIL_SOF;
  busmail_frame->busmail_crc = 0;

/* because of Rx Tx SEQ HEADER fields problem, update when these fields are filled with correct data
  busmail_frame->busmail_crc = (unsigned char)(busmail_frame->busmail_header + busmail_frame->busmail_crc);
*/

  buffer2[BUSMAIL_OFFSET_SOF] = busmail_frame->busmail_sof;
  buffer2[BUSMAIL_OFFSET_HEADER] = busmail_frame->busmail_header;

  if ((buffer2[BUSMAIL_OFFSET_HEADER] & BUSMAIL_HEADER_IC_BIT_MASK) ==
      BUSMAIL_HEADER_INFO_FRAME) {
    len =
      BUSMAIL_OFFSET_MAIL_PARAMS + busmail_frame->busmail_mail_params_buffer_len +
      sizeof(busmail_frame->busmail_crc);
    busmail_len_total =
      busmail_frame->busmail_mail_params_buffer_len +
      sizeof(busmail_frame->busmail_header)
      + sizeof(busmail_frame->busmail_mail_program_id) +
      sizeof(busmail_frame->busmail_mail_task_id) + 2;

    if (option_debug > 1)
      DEBUGA_CVM("INFO frame to send\n", CELLIAX_P_LOG);

    buffer2[BUSMAIL_OFFSET_MAIL_PROGRAM_ID] = busmail_frame->busmail_mail_program_id;
    buffer2[BUSMAIL_OFFSET_MAIL_TASK_ID] = busmail_frame->busmail_mail_task_id;
    buffer2[BUSMAIL_OFFSET_MAIL_PRIMITIVE_LSB] =
      busmail_frame->busmail_mail_primitive[BUSMAIL_MAIL_PRIMITIVE_LSB];
    buffer2[BUSMAIL_OFFSET_MAIL_PRIMITIVE_MSB] =
      busmail_frame->busmail_mail_primitive[BUSMAIL_MAIL_PRIMITIVE_MSB];

    if (busmail_frame->busmail_mail_params_buffer_len) {
      memcpy(buffer2 + BUSMAIL_OFFSET_MAIL_PARAMS,
             busmail_frame->busmail_mail_params_buffer,
             busmail_frame->busmail_mail_params_buffer_len);
    }

    for (i = 0; i < busmail_frame->busmail_mail_params_buffer_len; i++) {
      busmail_frame->busmail_crc =
        (unsigned char) (busmail_frame->busmail_mail_params_buffer[i] +
                         busmail_frame->busmail_crc);
    }

    busmail_frame->busmail_crc += busmail_frame->busmail_mail_program_id;
    busmail_frame->busmail_crc += busmail_frame->busmail_mail_task_id;
    busmail_frame->busmail_crc +=
      busmail_frame->busmail_mail_primitive[BUSMAIL_MAIL_PRIMITIVE_LSB];
    busmail_frame->busmail_crc +=
      busmail_frame->busmail_mail_primitive[BUSMAIL_MAIL_PRIMITIVE_MSB];
  } else {
    busmail_len_total = sizeof(busmail_frame->busmail_header);
    len = BUSMAIL_OFFSET_MAIL + sizeof(busmail_frame->busmail_crc);

    if (option_debug > 1)
      DEBUGA_CVM("CTRL frame to send\n", CELLIAX_P_LOG);
  }

/*
  DEBUGA_CVM("Its len=%d\n", CELLIAX_P_LOG, len);      
*/

  busmail_frame->busmail_len[BUSMAIL_LEN_LSB] =
    (unsigned char) (busmail_len_total & 0xFF);
  busmail_frame->busmail_len[BUSMAIL_LEN_MSB] = (unsigned char) (busmail_len_total >> 8);
  buffer2[BUSMAIL_OFFSET_LEN_MSB] = busmail_frame->busmail_len[BUSMAIL_LEN_MSB];
  buffer2[BUSMAIL_OFFSET_LEN_LSB] = busmail_frame->busmail_len[BUSMAIL_LEN_LSB];

/*
  buffer2[len-1] = busmail_frame->busmail_crc;
*/
  buffer2[len - 1] = 0xFF;

  if ((buffer2[BUSMAIL_OFFSET_HEADER] & BUSMAIL_HEADER_IC_BIT_MASK) ==
      BUSMAIL_HEADER_INFO_FRAME) {
    /* if it is INFO frame, queue it */

    /* update TxSeq and RxSeq bits */
    /* clear TxSeq and RxSeq bits */
    buffer2[BUSMAIL_OFFSET_HEADER] &=
      ~(BUSMAIL_HEADER_RXSEQ_MASK | BUSMAIL_HEADER_TXSEQ_MASK);

    buffer2[BUSMAIL_OFFSET_HEADER] |=
      (p->busmail_rxseq_cvm_last + 1) & BUSMAIL_HEADER_RXSEQ_MASK;

    p->busmail_txseq_celliax_last++;
    p->busmail_txseq_celliax_last &= 0x07;

    buffer2[BUSMAIL_OFFSET_HEADER] |=
      ((p->busmail_txseq_celliax_last) << 4) & BUSMAIL_HEADER_TXSEQ_MASK;

    /* update CRC */
    busmail_frame->busmail_crc += buffer2[BUSMAIL_OFFSET_HEADER];
    buffer2[len - 1] = busmail_frame->busmail_crc;

    p->cvm_busmail_outgoing_list = celliax_serial_list_init_CVM_BUSMAIL(p);
    p->cvm_busmail_outgoing_list->busmail_msg_len = len;

    for (i = 0; i < len; i++) {
      p->cvm_busmail_outgoing_list->busmail_msg_buffer[i] = buffer2[i];
    }

    if (option_debug > 10) {
      char debug_buf[1024];
      char *debug_buf_pos;

      memset(debug_buf, 0, 1024);
      debug_buf_pos = debug_buf;

      for (i = 0; i < len; i++) {
        debug_buf_pos += sprintf(debug_buf_pos, "[%.2X] ", buffer2[i]);
        if (debug_buf_pos > (char *) (&debug_buf + 1000))
          break;
      }

      if (option_debug > 1)
        DEBUGA_CVM("INFO: %s was prepared to send\n", CELLIAX_P_LOG, debug_buf);
    }

    if (option_debug > 1) {
      DEBUGA_CVM("OUTGOING INFO Frame TxSeq is %X, RxSeq is %X\n", CELLIAX_P_LOG,
                 (buffer2[BUSMAIL_OFFSET_HEADER] & BUSMAIL_HEADER_TXSEQ_MASK) >> 4,
                 buffer2[BUSMAIL_OFFSET_HEADER] & BUSMAIL_HEADER_RXSEQ_MASK);
/*
      DEBUGA_CVM("OUTGOING list:\n", CELLIAX_P_LOG);
      celliax_serial_list_print_CVM_BUSMAIL(p, p->cvm_busmail_outgoing_list);
      DEBUGA_CVM("OUTGOING list END\n", CELLIAX_P_LOG); */
    }
    p->cvm_busmail_outgoing_list->txseqno =
      (unsigned char) (buffer2[BUSMAIL_OFFSET_HEADER] & BUSMAIL_HEADER_TXSEQ_MASK) >> 4;
    p->cvm_busmail_outgoing_list->valid = 1;    /* ready to send (?) */

  } else {
    /* if it is CTRL frame, send it straight to the wire */
    if (BUSMAIL_HEADER_SABM !=
        (buffer2[BUSMAIL_OFFSET_HEADER] & BUSMAIL_HEADER_SABM_MASK)) {
      /*SABM ctrl frames have no RxSeq bits */

      buffer2[BUSMAIL_OFFSET_HEADER] &= ~BUSMAIL_HEADER_RXSEQ_MASK;

      if (BUSMAIL_HEADER_REJ ==
          (buffer2[BUSMAIL_OFFSET_HEADER] & BUSMAIL_HEADER_REJ_MASK)) {

        if (option_debug > 1)
          DEBUGA_CVM("CTRL REJ frame...\n", CELLIAX_P_LOG);

        if (0xFF != p->busmail_rxseq_cvm_last) {
          buffer2[BUSMAIL_OFFSET_HEADER] |=
            (p->busmail_rxseq_cvm_last + 1) & BUSMAIL_HEADER_RXSEQ_MASK;
        } else {
          if (option_debug > 1)
            DEBUGA_CVM
              ("Skipping sending REJ, because we just cleared RxSeq counter, and probably it was a packet that is invalid now...\n",
               CELLIAX_P_LOG);
          return 0;
        }

      } else {
        buffer2[BUSMAIL_OFFSET_HEADER] |=
          (p->busmail_rxseq_cvm_last + 1) & BUSMAIL_HEADER_RXSEQ_MASK;
      }
    }

    /* update CRC */
    busmail_frame->busmail_crc += buffer2[BUSMAIL_OFFSET_HEADER];
    buffer2[len - 1] = busmail_frame->busmail_crc;

    if (option_debug > 10) {
      char debug_buf[1024];
      char *debug_buf_pos;

      memset(debug_buf, 0, 1024);
      debug_buf_pos = debug_buf;

      for (i = 0; i < len; i++) {
        debug_buf_pos += sprintf(debug_buf_pos, "[%.2X] ", buffer2[i]);
        if (debug_buf_pos > (char *) (&debug_buf + 1000))
          break;
      }

      if (option_debug > 1)
        DEBUGA_CVM("CTRL: %s was prepared to send\n", CELLIAX_P_LOG, debug_buf);
    }
//    usleep(100);
    celliax_serial_send_CVM_BUSMAIL(p, len, buffer2);
  }

  return 0;
}

int celliax_serial_send_ctrl_frame_CVM_BUSMAIL(struct celliax_pvt *p,
                                               unsigned char FrameType)
{
  /*FrameType parameter is really a busmail header with info neeeded to tell the frame type to send */
  struct cvm_busmail_frame busmail_frame;

  switch (FrameType & 0xF0) {
    /* only higher nibble is important for us, do not take PF bit into considration */

  case BUSMAIL_HEADER_CTRL_FRAME | BUSMAIL_HEADER_CTRL_SU_FRAME | BUSMAIL_HEADER_SUID_RR:
  case BUSMAIL_HEADER_CTRL_FRAME | BUSMAIL_HEADER_CTRL_SU_FRAME | BUSMAIL_HEADER_SUID_REJ:
  case BUSMAIL_HEADER_CTRL_FRAME | BUSMAIL_HEADER_CTRL_SU_FRAME | BUSMAIL_HEADER_SUID_RNR:
  case BUSMAIL_HEADER_CTRL_FRAME | BUSMAIL_HEADER_CTRL_UN_FRAME | BUSMAIL_HEADER_UNID_SABM:

    busmail_frame.busmail_header =
      (FrameType & 0xF8);

    break;

  default:
    WARNINGA("UNKNOWN CTRL TYPE specified, sending nothing!!!\n", CELLIAX_P_LOG);
    return -1;
    break;
  }

  busmail_frame.busmail_mail_params_buffer_len = 0;

  /* Sending to CVM */
  return celliax_serial_write_CVM_BUSMAIL(p, &busmail_frame);
}

int celliax_serial_send_info_frame_CVM_BUSMAIL(struct celliax_pvt *p, int FrameType,
                                               unsigned char ParamsLen,
                                               unsigned char *Params)
{
  /*FrameType parameter is really a Primitive ID */
  struct cvm_busmail_frame busmail_frame;
  int i = 0;
  busmail_frame.busmail_header =
    (BUSMAIL_HEADER_PF_BIT_MASK & 0xFF) | BUSMAIL_HEADER_INFO_FRAME;

  busmail_frame.busmail_mail_primitive[BUSMAIL_MAIL_PRIMITIVE_LSB] = FrameType & 0xFF;
  busmail_frame.busmail_mail_primitive[BUSMAIL_MAIL_PRIMITIVE_MSB] =
    (FrameType >> 8) & 0xFF;

  busmail_frame.busmail_mail_program_id = BUSMAIL_MAIL_PROGRAM_ID;
  busmail_frame.busmail_mail_task_id = BUSMAIL_MAIL_TASK_ID;

  for (i = 0; i < ParamsLen; i++) {
    busmail_frame.busmail_mail_params_buffer[i] = Params[i];
  }

  busmail_frame.busmail_mail_params_buffer_len = ParamsLen;

  /* Sending to CVM */
  return celliax_serial_write_CVM_BUSMAIL(p, &busmail_frame);
}

int celliax_serial_lists_free_CVM_BUSMAIL(struct celliax_pvt *p)
{
  struct cvm_busmail_msg *ptr, *prev;
/*
  if (option_debug > 1) {
    DEBUGA_CVM("START FREEING OUTGOING\n", CELLIAX_P_LOG);
    DEBUGA_CVM("OUTGOING list:\n", CELLIAX_P_LOG);
    celliax_serial_list_print_CVM_BUSMAIL(p, p->cvm_busmail_outgoing_list);
    DEBUGA_CVM("OUTGOING list END\n", CELLIAX_P_LOG);
  }
*/
  ptr = p->cvm_busmail_outgoing_list;

  if (ptr) {
    while (ptr->next != NULL)
      ptr = ptr->next;

    while (ptr->previous != NULL) {

      if (option_debug > 1)
        DEBUGA_CVM("FREED \n", CELLIAX_P_LOG);

      prev = ptr->previous;
      free(ptr);
      ptr = prev;
    }

    free(ptr);
  }

  if (option_debug > 1)
    DEBUGA_CVM("LAST FREED \n", CELLIAX_P_LOG);

  p->cvm_busmail_outgoing_list = NULL;
  p->cvm_busmail_outgoing_list = celliax_serial_list_init_CVM_BUSMAIL(p);

  if (option_debug > 1) {
    DEBUGA_CVM("OUTGOING list:\n", CELLIAX_P_LOG);
    celliax_serial_list_print_CVM_BUSMAIL(p, p->cvm_busmail_outgoing_list);
    DEBUGA_CVM("OUTGOING list END\n", CELLIAX_P_LOG);
    DEBUGA_CVM("STARTING FREE INGOING\n", CELLIAX_P_LOG);
  }

  return 0;
}

struct cvm_busmail_msg *celliax_serial_list_init_CVM_BUSMAIL(struct celliax_pvt *p)
{
  struct cvm_busmail_msg *list;
  list = p->cvm_busmail_outgoing_list;

  PUSHA_UNLOCKA(&p->cvm_busmail_outgoing_list_lock);
  CVM_LOKKA(&p->cvm_busmail_outgoing_list_lock);

  if (list == NULL) {
    list = malloc(sizeof(*(list)));
    list->valid = 0;
    list->busmail_msg_len = 0;
    list->acknowledged = 0;
    list->how_many_sent = 0;
    list->sent = 0;
    list->tv_sec = 0;
    list->tv_usec = 0;
    list->next = NULL;
    list->previous = NULL;
  }

  if (list->valid != 0) {
    struct cvm_busmail_msg *new;
    new = malloc(sizeof(*new));
    new->valid = 0;
    new->busmail_msg_len = 0;
    new->acknowledged = 0;
    new->how_many_sent = 0;
    new->sent = 0;
    new->tv_sec = 0;
    new->tv_usec = 0;
    new->next = NULL;
    new->previous = list;
    list->next = new;
    list = new;
  }

  CVM_UNLOCKA(&p->cvm_busmail_outgoing_list_lock);
  POPPA_UNLOCKA(&p->cvm_busmail_outgoing_list_lock);

  return list;
}

int celliax_serial_list_print_CVM_BUSMAIL(struct celliax_pvt *p,
                                          struct cvm_busmail_msg *list)
{
  struct cvm_busmail_msg *ptr;
  ptr = list;

  if (ptr) {
    while (ptr->next != NULL)
      ptr = ptr->next;

    while (ptr) {

      if (option_debug > 3)
        DEBUGA_CVM
          ("PTR msg is: %d, seqnum is %.2X, tv_sec is %d, tv_usec is %d, acknowledged is: %d,"
           " sent is:%d, how_many_sent is: %d\n", CELLIAX_P_LOG, ptr->valid,
           /*ptr->seqnum */ 44,
           ptr->tv_sec, ptr->tv_usec, ptr->acknowledged, ptr->sent, ptr->how_many_sent);

      ptr = ptr->previous;
    }
  }

  return 0;
}

#endif /* CELLIAX_CVM */
