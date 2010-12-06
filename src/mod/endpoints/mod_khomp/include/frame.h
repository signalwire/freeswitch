/*******************************************************************************

    KHOMP generic endpoint/channel library.
    Copyright (C) 2007-2010 Khomp Ind. & Com.

  The contents of this file are subject to the Mozilla Public License 
  Version 1.1 (the "License"); you may not use this file except in compliance 
  with the License. You may obtain a copy of the License at 
  http://www.mozilla.org/MPL/ 

  Software distributed under the License is distributed on an "AS IS" basis,
  WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License for
  the specific language governing rights and limitations under the License.

  Alternatively, the contents of this file may be used under the terms of the
  "GNU Lesser General Public License 2.1" license (the “LGPL" License), in which
  case the provisions of "LGPL License" are applicable instead of those above.

  If you wish to allow use of your version of this file only under the terms of
  the LGPL License and not to allow others to use your version of this file 
  under the MPL, indicate your decision by deleting the provisions above and 
  replace them with the notice and other provisions required by the LGPL 
  License. If you do not delete the provisions above, a recipient may use your 
  version of this file under either the MPL or the LGPL License.

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
    along with this library; if not, write to the Free Software Foundation, 
    Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*******************************************************************************/

#ifndef _FRAME_HPP_
#define _FRAME_HPP_

#include <stdlib.h>
#include <sys/mman.h>

#include <ringbuffer.hpp>

#include "globals.h"

struct FrameStorage
{
    static const unsigned int frame_count = 24;//6;
    static const unsigned int audio_count = 24;//4;

    FrameStorage(switch_codec_t * codec, int packet_size);
    virtual ~FrameStorage();

    inline switch_frame_t * next_frame(void)
    {
        return &(_frames[next_index()]);
    }

    inline unsigned int next_index()
    {
        unsigned int tmp = _index;

        if (++_index >= frame_count)
            _index = 0;

        return tmp;
    }

    inline switch_frame_t * cng_frame(void)
    {
        return &_cng_frame;
    }

    char * audio_buffer()
    {
        return _buffer;
    };

 private:
    switch_frame_t   _cng_frame;

    switch_frame_t * _frames;
    char           * _buffer;

    unsigned int     _index;
};

/* Internal frame array structure. */
template < int S >
struct FrameManager: protected FrameStorage
{
    typedef const char Packet[ S ];

    typedef Ringbuffer < Packet >  AudioBuffer;

    FrameManager(switch_codec_t * codec)
    : FrameStorage(codec, S),
      _audio(audio_count, (Packet*)audio_buffer())
    {};

//    ~FrameManager();

    // may throw Ringbuffer::BufferEmpty
    switch_frame_t * pick(void)
    {
        try
        {
            /* try to consume from buffer.. */
            Packet & a = _audio.consumer_start();

            switch_frame * f = next_frame();

            /* adjust pointer */
            f->data = (char *)(&a);

            /* advance now */
            _audio.consumer_commit();
 
            return f;
        }
        catch (...) // AudioBuffer::BufferEmpty & e)
        {
            return NULL;
        }
    }

    // may throw Ringbuffer::BufferFull
    bool give(const char * buf, unsigned int size)
    {
        return _audio.provider_partial(buf, size);
    }

    switch_frame_t * cng(void)
    {
        return cng_frame();
    }

    void clear()
    {
        _audio.clear();
    }

 protected:
    AudioBuffer      _audio;

    unsigned int     _index;
};

typedef FrameManager < Globals::switch_packet_size > FrameSwitchManager;
typedef FrameManager < Globals::boards_packet_size > FrameBoardsManager;

#endif /* _FRAME_HPP_ */
