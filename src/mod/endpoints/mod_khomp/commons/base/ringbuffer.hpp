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

/* WARNING: This is a generic ringbuffer abstraction, which works for single-sized elements,
            partial elements, single/multi-elements read/writes. It is not wise to mix some
            functions (partial element write / full element write), since it was not designed
            with this use in mind.

            Also, it works only for single-reader + single-writer, since it does not depends
            on external mutex functions.

    NOTE: for single element provide/consume, this abstraction has standard C++ semantics.

          for multiple and partial element provide/consume, memcpy is used - thus complex C++
          objects which need correct copy constructor semantics should not copied this way.

 */

#include <string.h>

#include <cmath>
#include <iostream>

#include <noncopyable.hpp>
#include <atomic.hpp>

#ifndef _RINGBUFFER_HPP_
#define _RINGBUFFER_HPP_

struct Buffer_pointer
{
    Buffer_pointer(unsigned int _complete, unsigned short _partial)
    : complete(_complete), partial(_partial)
    {};

    Buffer_pointer(const Buffer_pointer & o)
    : complete(o.complete), partial(o.partial)
    {};

    Buffer_pointer(const volatile Buffer_pointer & o)
    : complete(o.complete), partial(o.partial)
    {};

    void operator=(const volatile Buffer_pointer o)
    {
        complete = o.complete;
        partial = o.partial;
    }

    void operator=(const Buffer_pointer o) volatile
    {
        complete = o.complete;
        partial = o.partial;
    }

    bool operator==(const Buffer_pointer & o)
    {
        return (complete == o.complete && partial == o.partial);
    }

    unsigned int  complete:20;
    unsigned short partial:12;
}
__attribute__((packed));

struct Buffer_table
{
    Buffer_table()
    : reader(0,0),
      writer(1,1)
    {};

    Buffer_table(const Buffer_table & o)
    : reader(o.reader), writer(o.writer)
    {};

    Buffer_table(const volatile Buffer_table & o)
    : reader(o.reader), writer(o.writer)
    {};

    void operator=(const volatile Buffer_table o)
    {
        reader = o.reader;
        writer = o.writer;
    }

    void operator=(const Buffer_table o) volatile
    {
        reader = o.reader;
        writer = o.writer;
    }

    bool operator==(const Buffer_table & o)
    {
        return (reader == o.reader && writer == o.writer);
    }

    Buffer_pointer reader;
    Buffer_pointer writer;
}
__attribute__((packed));

struct Ringbuffer_traits
{
    struct BufferFull  {};
    struct BufferEmpty {};

  protected:
    Ringbuffer_traits(unsigned int block, unsigned int size)
    : _block(block), _size(size)
    {};

    bool         traits_provide(      char *, const char *, unsigned int, bool);
    unsigned int traits_consume(const char *,       char *, unsigned int, bool);

    unsigned int traits_consume_begins(const char *, char *, unsigned int, bool);
    bool         traits_consume_commit(unsigned int);

    bool         traits_provide_partial(      char *, const char *, unsigned int);
    unsigned int traits_consume_partial(const char *,       char *, unsigned int);

    unsigned int traits_get(      char *, std::istream &, unsigned int);
    unsigned int traits_put(const char *, std::ostream &, unsigned int);

    bool update(Buffer_pointer & cache, Buffer_pointer & update)
    {
        return Atomic::doCAS(&(_pointers.reader), &cache, update);
    }

    inline unsigned int free_blocks(const Buffer_table & cache) const
    {
        const unsigned int r = cache.reader.complete;
        const unsigned int w = cache.writer.complete;

        if (r >= w)
            return (r - w);

        return _size - (w - r);
    }

    inline unsigned int used_blocks(const Buffer_table & cache) const
    {
        const unsigned int r = cache.reader.complete;
        const unsigned int w = cache.writer.complete;

        if (r >= w)
            return (_size - (r - w)) - 1;

        return (w - r) - 1;
    }

 protected:
    const unsigned int      _block;
    const unsigned int      _size;

    volatile Buffer_table _pointers;
};

template <typename T>
struct Ringbuffer: public Ringbuffer_traits, public NonCopyable
{
    Ringbuffer(unsigned int size)
    : Ringbuffer_traits(sizeof(T), size)
    {
        _buffer = new T[_size];
        _malloc = true;
    };

    Ringbuffer(unsigned int size, T * buffer)
    : Ringbuffer_traits(sizeof(T), size)
    {
        _buffer = buffer;
        _malloc = false;
    };

    ~Ringbuffer()
    {
        if (_malloc)
          delete[] _buffer;
    }

    /***** GENERIC RANGE/INDEX CALCULATION FUNCTIONS *****/

  protected:
    inline bool may_write(const Buffer_table & cache) const
    {
        const unsigned int r = cache.reader.complete;
        const unsigned int w = cache.writer.complete;

        return (((r - w) != 0) && (!(r == 0 && w == _size)));
    }

    inline bool may_read(const Buffer_table & cache) const
    {
        if ((cache.writer.complete - cache.reader.complete) == 1)
            return false;

        return true;
    }

    inline unsigned int writer_next(const Buffer_pointer & cache, Buffer_pointer & index) const
    {
        unsigned int dest = cache.complete - 1,
                     temp = cache.complete + 1;

        if (temp > _size) index.complete = 1;
        else              index.complete = temp;

        index.partial = 1;

        return dest;
    };

    inline void reader_next(const Buffer_pointer & cache, Buffer_pointer & index) const
    {
        unsigned int temp = cache.complete + 1;

        if (temp == _size) index.complete = 0;
        else               index.complete = temp;

        index.partial = 0;
    }

    /***** BUFFER FUNCTIONS *****/

  public:
    bool provide(const T & value)
    {
        Buffer_table   cache = _pointers;
        Buffer_pointer index = _pointers.writer;

        if (!may_write(cache))
            return false;

//        fprintf(stderr, "%p> provide %d/%d!\n", this, reader, writer);

        unsigned int dest = writer_next(cache.writer, index);

        _buffer[dest] = value;

        _pointers.writer = index;

//        fprintf(stderr, "%p> write: %d/%d [%d/%d]\n", this, _pointers.reader, _pointers.writer, _pointers.reader_partial, _pointers.writer_partial);

        return true;
    }

    bool consume(T & value)
    {
        Buffer_table   cache = _pointers;
        Buffer_pointer index = _pointers.reader;

        if (!may_read(cache))
            return false;

//        fprintf(stderr, "%p> consume %d/%d!\n", this, reader, writer);

        value = _buffer[index.complete];

        do
        {
            reader_next(cache.reader, index);

            if (update(cache.reader, index))
                break;

            cache.reader = index;
        }
        while (true);

//      fprintf(stderr, "%p> read: %d/%d [%d/%d]\n", this, _pointers.reader, _pointers.writer, _pointers.reader_partial, _pointers.writer_partial);

        return true;
    }

    /* writes everything or nothing */
    inline bool provide(const T * value, unsigned int amount, bool do_not_overwrite = true)
    {
        return traits_provide((char *)_buffer, (const char *) value, amount, do_not_overwrite);
    }

    /* returns the number of items that have been read (atomic_mode == true means 'all or nothing') */
    inline unsigned int consume(T * value, unsigned int amount, bool atomic_mode = false)
    {
        return traits_consume((const char *)_buffer, (char *) value, amount, atomic_mode);
    }

    /***** TWO-PHASE BUFFER FUNCTIONS *****/

    /* returns the number of items that have been read (atomic_mode == true means 'all or nothing') */
    inline unsigned int consume_begins(T * value, unsigned int amount, bool atomic_mode = false)
    {
        return traits_consume_begins((const char *)_buffer, (char *) value, amount, atomic_mode);
    }

    /* returns true if we could commit that much of buffer (use only after consume_begins).    *
     * note: you may commit less bytes that have been read to keep some data inside the buffer */
    inline bool consume_commit(unsigned int amount)
    {
        return traits_consume_commit(amount);
    }

    /***** TWO-PHASE SINGLE-ELEMENT BUFFER FUNCTIONS *****/

    T & provider_start(void)
    {
        Buffer_table  cache = _pointers;

        if (!may_write(cache))
            throw BufferFull();

        unsigned writer = _pointers.writer.complete - 1;

//        fprintf(stderr, "%p> provider start %d/%d!\n", this, reader, writer);

        return _buffer[writer];
    }

    void provider_commit(void)
    {
        unsigned int temp = _pointers.writer.complete + 1;

//        fprintf(stderr, "%p> provider commit %d!\n", this, temp);

        if (temp > _size)
            temp = 1;

        _pointers.writer.complete = temp;
        _pointers.writer.partial  = 1;

//        fprintf(stderr, "%p> write: %d/%d [%d/%d]\n", this, _pointers.reader, _pointers.writer, _pointers.reader_partial, _pointers.writer_partial);
    }

    const T & consumer_start(void)
    {
        Buffer_table  cache = _pointers;

        if (!may_read(cache))
            throw BufferEmpty();

        unsigned int reader = _pointers.reader.complete;

//        fprintf(stderr, "%p> consumer start %d/%d!\n", this, reader, writer);

        return _buffer[reader];
    }

    void consumer_commit(void)
    {
        Buffer_pointer cache = _pointers.reader;
        Buffer_pointer index(cache);

        do
        {
            reader_next(cache, index);

            if (update(cache, index))
                break;

            cache = index;
        }
        while (true);

//        fprintf(stderr, "%p> consumer commit %d!\n", this, temp);

//        fprintf(stderr, "%p> read: %d/%d [%d/%d]\n", this, _pointers.reader, _pointers.writer, _pointers.reader_partial, _pointers.writer_partial);
    }

    /* writes everything or nothing, but works on bytes (may write incomplete elements) */
    /* WARNING: do not mix this with full element provider */
    inline bool provider_partial(const char *buffer, unsigned int amount)
    {
        return traits_provide_partial((char *)_buffer, buffer, amount);
    }

    /* returns the number of bytes that have been read (may read incomplete elements) */
    /* WARNING: do not mix this with full element consumer */
    inline unsigned int consumer_partial(char *buffer, unsigned int amount)
    {
        return traits_consume_partial((const char *)_buffer, buffer, amount);
    }

    /***** IO FUNCTIONS *****/

    /* returns the number of items written to from buffer to stream */
    inline unsigned int put(std::ostream &fd, unsigned int amount)
    {
        return traits_put((const char *)_buffer, fd, amount);
    }

    /* returns number of items read from stream to buffer */
    inline unsigned int get(std::istream &fd, unsigned int amount)
    {
        return traits_get((char *)_buffer, fd, amount);
    }

    void clear()
    {
        _pointers.reader.complete = 0;
        _pointers.reader.partial  = 0;
        _pointers.writer.complete = 1;
        _pointers.writer.partial  = 1;
    }

 protected:
    T *  _buffer;
    bool _malloc;
};

#endif /* _RINGBUFFER_HPP_ */
