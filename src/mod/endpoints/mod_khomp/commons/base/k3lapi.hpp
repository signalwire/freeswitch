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
#include <stdexcept>

#include <format.hpp>

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

struct K3LAPITraits
{
    struct invalid_device;
    struct invalid_channel;
    struct invalid_link;

    struct invalid_target: public std::runtime_error
    {
        friend class invalid_device;
        friend class invalid_channel;
        friend class invalid_link;

        const int32 device, object;

      protected:
        invalid_target(int32 _device, int32 _object, const std::string & msg)
        : std::runtime_error(msg), device(_device), object(_object) {};
    };

    struct invalid_device: public invalid_target
    {
        invalid_device(int32 _device)
        : invalid_target(_device, -1, STG(FMT("invalid device number '%d'") % _device)) {};
    };

    struct invalid_channel: public invalid_target
    {
        invalid_channel(int32 _device, int32 _channel)
        : invalid_target(_device, _channel, STG(FMT("invalid channel number '%d' on device '%d'") % _channel % _device)) {};
    };

    struct invalid_link: public invalid_target
    {
        invalid_link(int32 _device, int32 _link)
        : invalid_target(_device, _link, STG(FMT("invalid link number '%d' on device '%d'") % _link % _device)) {};
    };
};

struct K3LAPIBase
{
    /* High level checked object identifier. */

    struct GenericTarget
    {
        typedef enum { DEVICE, CHANNEL, MIXER, LINK } Type;

        GenericTarget(const K3LAPIBase & k3lapi, Type _type, int32 _device, int32 _object)
        : type(_type), device((unsigned int)_device), object((unsigned int)_object)
        {
            switch (_type)
            {
                case DEVICE:
                    if (!k3lapi.valid_device(_device))
                        throw K3LAPITraits::invalid_device(_device);
                    break;

                case CHANNEL:
                case MIXER:
                    if (!k3lapi.valid_channel(_device, _object))
                        throw K3LAPITraits::invalid_channel(_device, _object);
                    break;

                case LINK:
                    if (!k3lapi.valid_link(_device, _object))
                        throw K3LAPITraits::invalid_link(_device, _object);
                    break;
            }
        };

        const Type type;

        const unsigned int device;
        const unsigned int object;
    };

/*
    struct LinkTarget    : public GenericTarget
    {
        LinkTarget(const K3LAPIBase & k3lapi, int32 _device, int32 _object)
        : GenericTarget(k3lapi, GenericTarget::LINK, _device, _object) {};
    };

    struct ChannelTarget : public GenericTarget
    {
        ChannelTarget(const K3LAPIBase & k3lapi, int32 _device, int32 _object)
        : GenericTarget(k3lapi, GenericTarget::CHANNEL, _device, _object) {};
    };

*/
    template < GenericTarget::Type T >
    struct Target: public GenericTarget
    {
        Target(const K3LAPIBase & k3lapi, int32 _device, int32 _object)
        : GenericTarget(k3lapi, T, _device, _object) {};

//        operator const GenericTarget&() const { return static_cast<const GenericTarget &>(*this); };
    };

    /* exceptions */

    struct start_failed: public std::runtime_error
    {
        start_failed(const char * msg)
        : std::runtime_error(msg) {};
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

    struct get_param_failed
    {
        get_param_failed(std::string _name, int32 _rc)
        : name(_name), rc((KLibraryStatus)_rc) {};

        std::string name;
        KLibraryStatus rc;
    };

    /* typedefs essenciais */

    typedef K3L_DEVICE_CONFIG          device_conf_type;
    typedef K3L_CHANNEL_CONFIG        channel_conf_type;
    typedef K3L_CHANNEL_CONFIG *  channel_ptr_conf_type;
    typedef K3L_LINK_CONFIG              link_conf_type;
    typedef K3L_LINK_CONFIG *        link_ptr_conf_type;

    /* constructors/destructors */

             K3LAPIBase();
    virtual ~K3LAPIBase() {};

    /* (init|final)ialize the whole thing! */

    void start(void);
    void stop(void);

    /* verificacao de intervalos */

    inline bool valid_device(int32 dev) const
    {
        return (dev >= 0 && dev < ((int32)_device_count));
    }

    inline bool valid_channel(int32 dev, int32 obj) const
    {
        return (valid_device(dev) && obj >= 0 && obj < ((int32)_channel_count[dev]));
    }

    inline bool valid_link(int32 dev, int32 obj) const
    {
        return (valid_device(dev) && obj >= 0 && obj < ((int32)_link_count[dev]));
    }

    /* envio de comandos para placa (geral) */

    void raw_command(int32 dev, int32 dsp, std::string & str) const;
    void raw_command(int32 dev, int32 dsp, const char * cmds, int32 size) const;

    /* obter dados 'cacheados' (geral) */

    inline unsigned int device_count(void) const
    {
        return _device_count;
    }

    /* envio de comandos para placa (sem identificadores) */

    void mixer(int32 dev, int32 obj, byte track, KMixerSource src, int32 index) const;
    void mixerRecord(int32 dev, KDeviceType type, int32 obj, byte track, KMixerSource src, int32 index) const;
    void mixerCTbus(int32 dev, int32 obj, byte track, KMixerSource src, int32 index) const;

    void command (int32 dev, int32 obj, int32 code, std::string & str) const;
    void command (int32 dev, int32 obj, int32 code, const char * parms = NULL) const;


    /* envio de comandos para placa (com identificadores) */

    void mixer(const GenericTarget & tgt, byte track, KMixerSource src, int32 index) const
    {
        mixer(tgt.device, tgt.object, track, src, index);
    }

    void mixerRecord(const GenericTarget & tgt, byte track, KMixerSource src, int32 index) const
    {
        mixerRecord((int32)tgt.device, _device_type[tgt.device], (int32)tgt.object, track, src, index);
    }

    void mixerCTbus(const GenericTarget & tgt, byte track, KMixerSource src, int32 index) const
    {
        mixerCTbus((int32)tgt.device, (int32)tgt.object, track, src, index);
    }

    void command(const GenericTarget & tgt, int32 code, std::string & str) const
    {
        command((int32)tgt.device, (int32)tgt.object, code, str);
    };

    void command(const GenericTarget & tgt, int32 code, const char * parms = NULL) const
    {
        command((int32)tgt.device, (int32)tgt.object, code, parms);
    };

    /* obter dados 'cacheados' (com indentificadores) */

    inline unsigned int channel_count(const GenericTarget & tgt) const
    {
        return _channel_count[tgt.device];
    }

    inline unsigned int link_count(const GenericTarget & tgt) const
    {
        return _link_count[tgt.device];
    }

    KDeviceType device_type(const GenericTarget & tgt) const
    {
        return _device_type[tgt.device];
    }

    const K3L_DEVICE_CONFIG & device_config(const GenericTarget & tgt) const
    {
        return _device_config[tgt.device];
    }

    const K3L_CHANNEL_CONFIG & channel_config(const Target<GenericTarget::CHANNEL> & tgt) const
    {
        return _channel_config[tgt.device][tgt.object];
    }

    const K3L_LINK_CONFIG & link_config(const Target<GenericTarget::LINK> & tgt) const
    {
        return _link_config[tgt.device][tgt.object];
    }

    /* pega valores em strings de eventos */

    KLibraryStatus get_param(K3L_EVENT *ev, const char *name, std::string &res) const;

    std::string get_param(K3L_EVENT *ev, const char *name) const;
    std::string get_param_optional(K3L_EVENT *ev, const char *name) const;

    /* inicializa valores em cache */

    void init(void);
    void fini(void);

    /* utilidades diversas e informacoes */

    enum DspType
    {
        DSP_AUDIO,
        DSP_SIGNALING,
    };

    int32 get_dsp(KDeviceType, DspType) const;

    int32 get_dsp(const GenericTarget &, DspType) const;

 protected:

    unsigned int           _device_count;
    unsigned int *        _channel_count;
    unsigned int *           _link_count;

         device_conf_type *   _device_config;
    channel_ptr_conf_type *  _channel_config;
       link_ptr_conf_type *     _link_config;
              KDeviceType *     _device_type;
};

/* exceptions */
template < bool E = false >
struct K3LAPIException
{
    void invalid_device(const int32 device) const
    {
        /* NOTHING */
    }

    void invalid_channel(const int32 device, const int32 channel) const
    {
        /* NOTHING */
    }

    void invalid_link(const int32 device, const int32 link) const
    {
        /* NOTHING */
    }
};

template < >
struct K3LAPIException < true >
{
    void invalid_device(const int32 device) const
    {
        throw K3LAPITraits::invalid_device(device);
    }

    void invalid_channel(const int32 device, const int32 channel) const
    {
        throw K3LAPITraits::invalid_channel(device, channel);
    }

    void invalid_link(const int32 device, const int32 link) const
    {
        throw K3LAPITraits::invalid_link(device, link);
    }
};

template < bool E = false >
struct K3LAPITemplate: public K3LAPIBase, protected K3LAPIException < E >
{
    using K3LAPIBase::device_config;
    using K3LAPIBase::channel_config;
    using K3LAPIBase::link_config;

    using K3LAPIBase::device_type;
    using K3LAPIBase::get_dsp;
    
    using K3LAPIBase::mixerRecord;

    /* obter dados 'cacheados' (sem identificadores) */

    inline unsigned int channel_count(int32 dev) const
    {
        if (!valid_device(dev))
        {
            K3LAPIException< E >::invalid_device(dev);
            return 0;
        }

        return _channel_count[dev];
    }

    inline unsigned int link_count(int32 dev) const
    {
        if (!valid_device(dev))
        {
            K3LAPIException< E >::invalid_device(dev);
            return 0;
        }

        return _link_count[dev];
    }

    inline uint32 channel_stats(int32 dev, int32 obj, uint32 index) const
    {
        if (!valid_channel(dev, obj))
        {
            K3LAPIException< E >::invalid_channel(dev, obj);
            return 0u;
        }

        uint32 res_value = 0u;

#if K3L_AT_LEAST(2,1,0)
        if (k3lGetChannelStats(dev, obj, index, &res_value) != ksSuccess)
            return 0u;

        return res_value;
#endif
    }

    KDeviceType device_type(int32 dev) const
    {
        if (!valid_device(dev))
        {
            K3LAPIException< E >::invalid_device(dev);
            return kdtDevTypeCount;
        }

        return _device_type[dev];
    }

    const K3L_DEVICE_CONFIG & device_config(int32 dev) const
    {
        if (!valid_device(dev))
            throw K3LAPITraits::invalid_device(dev);

        return _device_config[dev];
    }

    const K3L_CHANNEL_CONFIG & channel_config(int32 dev, int32 obj) const
    {
        if (!valid_channel(dev, obj))
            throw K3LAPITraits::invalid_channel(dev, obj);

        return _channel_config[dev][obj];
    }

    const K3L_LINK_CONFIG & link_config(int32 dev, int32 obj) const
    {
        if (!valid_link(dev, obj))
            throw K3LAPITraits::invalid_link(dev, obj);

        return _link_config[dev][obj];
    }

    int32 get_dsp(int32 dev, DspType type) const
    {
        return get_dsp(device_type(dev), type);
    }

    void mixerRecord(int32 dev, int32 obj, byte track, KMixerSource src, int32 index) const
    {
        mixerRecord(dev, device_type(dev), obj, track, src, index);
    }
};

typedef K3LAPITemplate<> K3LAPI;

#endif /* _K3LAPI_HPP_ */
