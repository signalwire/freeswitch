/*
    KHOMP generic endpoint/channel library.
    Copyright (C) 2007-2009 Khomp Ind. & Com.

  The contents of this file are subject to the Mozilla Public License Version 1.1
  (the "License"); you may not use this file except in compliance with the
  License. You may obtain a copy of the License at http://www.mozilla.org/MPL/

  Software distributed under the License is distributed on an "AS IS" basis,
  WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License for
  the specific language governing rights and limitations under the License.

  Alternatively, the contents of this file may be used under the terms of the
  "GNU Lesser General Public License 2.1" license (the “LGPL" License), in which
  case the provisions of "LGPL License" are applicable instead of those above.

  If you wish to allow use of your version of this file only under the terms of
  the LGPL License and not to allow others to use your version of this file under
  the MPL, indicate your decision by deleting the provisions above and replace them
  with the notice and other provisions required by the LGPL License. If you do not
  delete the provisions above, a recipient may use your version of this file under
  either the MPL or the LGPL License.

  The LGPL header follows below:

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/

#include <k3lapi.hpp>

#include <format.hpp>
#include <verbose.hpp>

K3LAPI::K3LAPI(bool has_exceptions)
: _has_exceptions(has_exceptions),
  _device_count(0),  _channel_count(0),  _link_count(0),
  _device_config(0), _channel_config(0), _link_config(0)
{};

/* initialize the whole thing! */

void K3LAPI::start(void)
{
    /* tie the used k3l to the compiled k3l version */
    char *ret = k3lStart(k3lApiMajorVersion, k3lApiMinorVersion, 0); //k3lApiBuildVersion);

    if (ret && *ret)
        throw start_failed(ret);

    /* call init automagically */
    init();
}

void K3LAPI::stop(void)
{
    k3lStop();
    fini();
}

/* envio de comandos para placa */

void K3LAPI::mixer(int32 dev, int32 obj, byte track, KMixerSource src, int32 index)
{
    KMixerCommand mix;

    mix.Track = track;
    mix.Source = src;
    mix.SourceIndex = index;

    command(dev, obj, CM_MIXER, (const char *) &mix);
}

void K3LAPI::mixerRecord(int32 dev, int32 obj, byte track, KMixerSource src, int32 index)
{
    /* estes buffers *NAO PODEM SER ESTATICOS*! */
    char cmd[] = { 0x3f, 0x03, (char)obj, (char)track, 0xff, 0xff };

    switch (src)
    {
        case kmsChannel:
            cmd[4] = 0x05;
            cmd[5] = (char)index;
            break;

        case kmsNoDelayChannel:
            cmd[4] = 0x0a;
            cmd[5] = (char)index;
            break;

        case kmsGenerator:
            cmd[4] = 0x09;

            switch ((KMixerTone)index)
            {
                case kmtSilence:
                    cmd[5] = 0x0F;
                    break;
                case kmtDial:
                    cmd[5] = 0x08;
                    break;
                case kmtBusy:
                    cmd[5] = 0x0D;
                    break;

                case kmtFax:
                case kmtVoice:
                case kmtEndOf425:
                case kmtCollect:
                case kmtEndOfDtmf:
                    /* TODO: exception, unable to generate */
                    break;
            }
            break;

        case kmsCTbus:
        case kmsPlay:
            /* TODO: exception, not implemented! */
            break;
    }

    int32 dsp = get_dsp(dev, DSP_AUDIO);

    raw_command(dev, dsp, cmd, sizeof(cmd));
}

void K3LAPI::mixerCTbus(int32 dev, int32 obj, byte track, KMixerSource src, int32 index)
{
    KMixerCommand mix;

    mix.Track = track;
    mix.Source = src;
    mix.SourceIndex = index;

    command(dev, obj, CM_MIXER_CTBUS, (const char *) &mix);
}

void K3LAPI::command(int32 dev, int32 obj, int32 code, std::string & str)
{
    command(dev, obj, code, str.c_str());
}

void K3LAPI::command (int32 dev, int32 obj, int32 code, const char * parms)
{
    K3L_COMMAND cmd;

    cmd.Cmd = code;
    cmd.Object = obj;
    cmd.Params = (byte *)parms;

    int32 rc = k3lSendCommand(dev, &cmd);

    if (rc != ksSuccess)
        throw failed_command(code, dev, obj, rc);
}

void K3LAPI::raw_command(int32 dev, int32 dsp, std::string & str)
{
    raw_command(dev, dsp, str.data(), str.size());
}

void K3LAPI::raw_command(int32 dev, int32 dsp, const char * cmds, int32 size)
{
    std::string str(cmds, size);

    int32 rc = k3lSendRawCommand(dev, dsp, (void *)cmds, size);

    if (rc != ksSuccess)
        throw failed_raw_command(dev, dsp, rc);
}

KLibraryStatus K3LAPI::get_param(K3L_EVENT *ev, const char *name, std::string &res)
{
    char tmp_param[256];
    memset((void*)tmp_param, 0, sizeof(tmp_param));

    int32 rc = k3lGetEventParam (ev, (sbyte *)name, (sbyte *)tmp_param, sizeof(tmp_param)-1);

    if (rc != ksSuccess)
        return (KLibraryStatus)rc;

    res.append(tmp_param, strlen(tmp_param));
    return ksSuccess;
}

std::string K3LAPI::get_param(K3L_EVENT *ev, const char *name)
{
    std::string res;

    KLibraryStatus rc = get_param(ev, name, res);

    if (rc != ksSuccess)
        throw get_param_failed(name, rc);

    return res;
}

void K3LAPI::init(void)
{
    if (_device_count != 0) return;

    _device_count = k3lGetDeviceCount();

    _device_type    = new KDeviceType[_device_count];
    _device_config  = new device_conf_type[_device_count];
    _channel_config = new channel_ptr_conf_type[_device_count];
    _link_config    = new link_ptr_conf_type[_device_count];
    _channel_count  = new unsigned int[_device_count];
    _link_count     = new unsigned int[_device_count];

    for (unsigned int dev = 0; dev < _device_count; dev++)
    {
        KLibraryStatus ret = ksSuccess;

        _device_type[dev] = (KDeviceType) k3lGetDeviceType(dev);

        /* caches each device config */
        ret = (KLibraryStatus)k3lGetDeviceConfig(dev, ksoDevice + dev, &(_device_config[dev]), sizeof(_device_config[dev]));

        if (ret != ksSuccess)
            throw start_failed(STG(FMT("k3lGetDeviceConfig(dev=%d): %s") % dev % Verbose::status(ret)));

        /* adjust channel/link count for device */
        _channel_count[dev] = _device_config[dev].ChannelCount;
        _link_count[dev] = _device_config[dev].LinkCount;

        /* caches each channel config */
        _channel_config[dev] = new channel_conf_type[_channel_count[dev]];

        for (unsigned int obj = 0; obj < _channel_count[dev]; obj++)
        {
            ret = (KLibraryStatus)k3lGetDeviceConfig(dev, ksoChannel + obj, &(_channel_config[dev][obj]), sizeof(_channel_config[dev][obj]));

            if (ret != ksSuccess)
                throw start_failed(STG(FMT("k3lGetDeviceConfig(dev=%d,chan=%d): %s") % dev % obj % Verbose::status(ret)));
        }

        /* adjust link count for device */
        _link_count[dev] = _device_config[dev].LinkCount;

        /* caches each link config */
        _link_config[dev] = new link_conf_type[_link_count[dev]];

        for (unsigned int obj = 0; obj < _link_count[dev]; obj++)
        {
            ret = (KLibraryStatus)k3lGetDeviceConfig(dev, ksoLink + obj, &(_link_config[dev][obj]), sizeof(_link_config[dev][obj]));

            if (ret != ksSuccess)
                throw start_failed(STG(FMT("k3lGetDeviceConfig(dev=%d,link=%d): %s") % dev % obj % Verbose::status(ret)));
        }
    }
}

void K3LAPI::fini(void)
{
    for (unsigned int dev = 0; dev < _device_count; dev++)
    {
        if (_channel_config[dev])
        {
            delete[] _channel_config[dev];
            _channel_config[dev] = NULL;
        }

        if (_link_config[dev])
        {
            delete[] _link_config[dev];
            _link_config[dev] = NULL;
        }
    }

    _device_count = 0;

    if (_device_type)    { delete[] _device_type;       _device_type = NULL; }
    if (_device_config)  { delete[] _device_config;   _device_config = NULL; }
    if (_channel_config) { delete[] _channel_config; _channel_config = NULL; }
    if (_link_config)    { delete[] _link_config;       _link_config = NULL; }
    if (_channel_count)  { delete[] _channel_count;   _channel_count = NULL; }
    if (_link_count)     { delete[] _link_count;         _link_count = NULL; }
}

int32 K3LAPI::get_dsp(int32 dev, K3LAPI::DspType type)
{
    switch (device_type(dev))
    {
        case kdtFXO:
        case kdtFXOVoIP:
#if K3L_AT_LEAST(1,6,0)
        case kdtGSM:
        case kdtGSMSpx:
#endif
#if K3L_AT_LEAST(2,1,0)
        case kdtGSMUSB:
        case kdtGSMUSBSpx:
#endif
            return 0;

        default:
            return (type == DSP_AUDIO ? 1 : 0);
    }
}

