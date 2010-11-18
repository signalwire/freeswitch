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

#include <string>

#include <k3l.h>

/* if using full k3l.h (for softpbx), version already defined. */
#ifndef k3lApiMajorVersion
# include <k3lVersion.h>
#endif

#ifdef __GNUC_PREREQ
#if __GNUC_PREREQ(4,3)
#include <cstring>
#endif
#endif

#include <types.hpp>

#ifndef _K3LAPI_HPP_
#define _K3LAPI_HPP_

struct K3LAPI
{
    /* exceptions */

    struct start_failed
    {
        start_failed(const char * _msg) : msg(_msg) {};
        start_failed(std::string  _msg) : msg(_msg) {};
        std::string msg;
    };

    struct failed_command
    {
        failed_command(int32 _code, unsigned short _dev, unsigned short _obj, int32 _rc)
        : code(_code), dev(_dev), obj(_obj), rc(_rc) {};

        int32           code;
        unsigned short  dev;
        unsigned short  obj;
        int32           rc;
    };

    struct failed_raw_command
    {
        failed_raw_command(unsigned short _dev, unsigned short _dsp, int32 _rc)
        : dev(_dev), dsp(_dsp), rc(_rc) {};

        unsigned short  dev;
        unsigned short  dsp;
        int32           rc;
    };

    struct invalid_device
    {
        invalid_device(int32 _device)
        : device(_device) {};

        int32 device;
    };

    struct invalid_channel
    {
        invalid_channel(int32 _device, int32 _channel)
        : device(_device), channel(_channel) {};

        int32 device, channel;
    };

    struct invalid_link
    {
        invalid_link(unsigned int _device, unsigned int _link)
        : device(_device), link(_link) {};

        int32 device, link;
    };

    struct get_param_failed
    {
        get_param_failed(std::string _name, int32 _rc)
        : name(_name), rc((KLibraryStatus)_rc) {};

        std::string name;
        KLibraryStatus rc;
    };

    typedef K3L_DEVICE_CONFIG          device_conf_type;
    typedef K3L_CHANNEL_CONFIG        channel_conf_type;
    typedef K3L_CHANNEL_CONFIG *  channel_ptr_conf_type;
    typedef K3L_LINK_CONFIG              link_conf_type;
    typedef K3L_LINK_CONFIG *        link_ptr_conf_type;

    /* constructors/destructors */

             K3LAPI(bool has_exceptions = false);
    virtual ~K3LAPI() {};

    /* (init|final)ialize the whole thing! */

    void start(void);
    void stop(void);

    /* verificacao de intervalos */

    inline bool valid_device(int32 dev)
    {
        return (dev >= 0 && dev < ((int32)_device_count));
    }

    inline bool valid_channel(int32 dev, int32 obj)
    {
        return (valid_device(dev) && obj >= 0 && obj < ((int32)_channel_count[dev]));
    }

    inline bool valid_link(int32 dev, int32 obj)
    {
        return (valid_device(dev) && obj >= 0 && obj < ((int32)_link_count[dev]));
    }

    /*!
      \brief High level object identifier
      Since Khomp works with an object concept, this is used to map the
      object id with its proper type.
     */
    struct target
    {
        /*! The types a target can have */
        typedef enum { DEVICE, CHANNEL, MIXER, LINK } target_type;

        target(K3LAPI & k3lapi, target_type type_init, int32 device_value, int32 object_value)
        : type(type_init),
          device((unsigned short)device_value),
          object((unsigned short)object_value)
        {
            switch (type_init)
            {
                case DEVICE:
                    if (!k3lapi.valid_device(device_value))
                        throw invalid_device(device_value);
                    break;

                case CHANNEL:
                case MIXER:
                    if (!k3lapi.valid_channel(device_value, object_value))
                        throw invalid_channel(device_value, object_value);
                    break;

                case LINK:
                    if (!k3lapi.valid_link(device_value, object_value))
                        throw invalid_link(device_value, object_value);
                    break;
            }

        };

        const target_type type;

        const unsigned short device;
        const unsigned short object;
    };

    /* envio de comandos para placa (geral) */

    void raw_command(int32 dev, int32 dsp, std::string & str);
    void raw_command(int32 dev, int32 dsp, const char * cmds, int32 size);

    /* obter dados 'cacheados' (geral) */

    inline unsigned int device_count(void)
    {
        return _device_count;
    }

    /* envio de comandos para placa (sem identificadores) */

    void mixer(int32 dev, int32 obj, byte track, KMixerSource src, int32 index);
    void mixerRecord(int32 dev, int32 obj, byte track, KMixerSource src, int32 index);
    void mixerCTbus(int32 dev, int32 obj, byte track, KMixerSource src, int32 index);

    void command (int32 dev, int32 obj, int32 code, std::string & str);
    void command (int32 dev, int32 obj, int32 code, const char * parms = NULL);

    /* obter dados 'cacheados' (sem identificadores) */

    inline unsigned int channel_count(int32 dev)
    {
        if (!valid_device(dev))
        {
            if (_has_exceptions)
                throw invalid_device(dev);
            else
                return 0;
        }

        return _channel_count[dev];
    }

    inline unsigned int link_count(int32 dev)
    {
        if (!valid_device(dev))
        {
            if (_has_exceptions)
                throw invalid_device(dev);
            else
                return 0;
        }

        return _link_count[dev];
    }

    inline uint32 channel_stats(int32 dev, int32 obj, uint32 index)
    {
        if (!valid_channel(dev, obj))
        {
            if (_has_exceptions)
                throw invalid_channel(dev, obj);
            else
                return 0;
        }

        uint32 res_value = (uint32)-1;
        stt_code stt_res = ksFail;

#if K3L_AT_LEAST(2,1,0)
        stt_res = k3lGetChannelStats(dev, obj, index, &res_value);
#endif

        if(stt_res != ksSuccess)
        {
            return (uint32)-1;
        }

        return res_value;
    }

    KDeviceType device_type(int32 dev)
    {
        if (!valid_device(dev))
        {
            if (_has_exceptions)
                throw invalid_device(dev);
            else
                return kdtDevTypeCount;
        }

        return _device_type[dev];
    }


    K3L_DEVICE_CONFIG & device_config(int32 dev)
    {
        if (!valid_device(dev))
            throw invalid_device(dev);

        return _device_config[dev];
    }

    K3L_CHANNEL_CONFIG & channel_config(int32 dev, int32 obj)
    {
        if (!valid_channel(dev, obj))
            throw invalid_channel(dev, obj);

        return _channel_config[dev][obj];
    }

    K3L_LINK_CONFIG & link_config(int32 dev, int32 obj)
    {
        if (!valid_link(dev, obj))
            throw invalid_channel(dev, obj);

        return _link_config[dev][obj];
    }

    /* envio de comandos para placa (com identificadores) */

    void mixer(target & tgt, byte track, KMixerSource src, int32 index)
    {
        mixer((int32)tgt.device, (int32)tgt.object, track, src, index);
    }

    void mixerRecord(target & tgt, byte track, KMixerSource src, int32 index)
    {
        mixerRecord((int32)tgt.device, (int32)tgt.object, track, src, index);
    }

    void mixerCTbus(target & tgt, byte track, KMixerSource src, int32 index)
    {
        mixerCTbus((int32)tgt.device, (int32)tgt.object, track, src, index);
    }

    void command (target & tgt, int32 code, std::string & str)
    {
        command((int32)tgt.device, (int32)tgt.object, code, str);
    };

    void command (target & tgt, int32 code, const char * parms = NULL)
    {
        command((int32)tgt.device, (int32)tgt.object, code, parms);
    };

    /* obter dados 'cacheados' (com indentificadores) */

    inline unsigned int channel_count(target & tgt)
    {
        return _channel_count[tgt.device];
    }

    inline unsigned int link_count(target & tgt)
    {
        return _link_count[tgt.device];
    }

    KDeviceType device_type(target & tgt)
    {
        return _device_type[tgt.device];
    }


    K3L_DEVICE_CONFIG & device_config(target & tgt)
    {
        return _device_config[tgt.device];
    }

    K3L_CHANNEL_CONFIG & channel_config(target & tgt)
    {
        return _channel_config[tgt.device][tgt.object];
    }

    K3L_LINK_CONFIG & link_config(target & tgt)
    {
        return _link_config[tgt.device][tgt.object];
    }

    /* pega valores em strings de eventos */

    KLibraryStatus get_param(K3L_EVENT *ev, const char *name, std::string &res);
    std::string get_param(K3L_EVENT *ev, const char *name);

    /* inicializa valores em cache */

    void init(void);
    void fini(void);

    /* utilidades diversas e informacoes */

    enum DspType
    {
        DSP_AUDIO,
        DSP_SIGNALING,
    };

    int32 get_dsp(int32, DspType);

 protected:

    const bool           _has_exceptions;

    unsigned int           _device_count;
    unsigned int *        _channel_count;
    unsigned int *           _link_count;

         device_conf_type *   _device_config;
    channel_ptr_conf_type *  _channel_config;
       link_ptr_conf_type *     _link_config;
              KDeviceType *     _device_type;
};

#endif /* _K3LAPI_HPP_ */
