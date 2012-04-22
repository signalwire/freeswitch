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

#include <ringbuffer.hpp>

// #include <stdio.h>


  /* Documentation of the formula used in the buffer arithmetic.
   *
   * [0|1|2|3|4|5|6|7] => size=8
   *      |   |
   *  reader  |
   *         writer
   *
   * => writer has places [5,6,7,0,1] to write (5 places).
   *
   * =>  8 - (4-2+1) = 8 - (2+1) = 8 - 3 = 5
   *
   * > writer goes 1 up, amount goes 1 down.
   * > reader goes 1 up, amount goes 1 up.
   * > size goes 1 down, amount goes 1 down.
   *
   */

/********** BUFFER FUNCTIONS **********/

/* writes everything or nothing */
bool Ringbuffer_traits::traits_provide(char * buffer, const char * value, unsigned int amount, bool do_not_overwrite)
{
    /* avoid using different values */
    Buffer_table cache = _pointers;

    bool need_overwrite = false;

    if (amount > free_blocks(cache))
    {
        if (do_not_overwrite)
            return false;

        /* if we are allowed to overwrite, just the buffer size matters for us */
        if (amount >= _size)
            return false;

        /* we need to change reader pointer below... */
        need_overwrite = true;
    }

    const unsigned int wr = cache.writer.complete;
    const unsigned int wp = cache.writer.complete - 1;

    /* should we go around the buffer for writing? */
    if ((wr + amount) > _size)
    {
//        fprintf(stderr, "%p> first if matched\n", this);

        if (need_overwrite)
        {
            do
            {
                Buffer_pointer extra(cache.reader);
                extra.complete = ((wr + amount) % _size); // (extra.complete + amount) % _size;
//                extra.complete = (extra.complete + amount) % _size;

                if (update(cache.reader, extra))
                    break;
            }
            while (true);
        }

        unsigned int wr1 = _size - wr + 1; /* writer is already 1 position after */
        unsigned int wr2 = amount - wr1;

//        fprintf(stderr, "%p> partial write: (%d/%d) %d/%d [%d/%d]\n", this, wr1, wr2, amount, _size, reader, writer);

        /* two partial writes (one at the end, another at the beginning) */
        memcpy((void *) &(buffer[wp]), (const void *)  (value),      _block * wr1);
        memcpy((void *)  (buffer),     (const void *) &(value[wr1]), _block * wr2);
    }
    else
    {
//        fprintf(stderr, "%p> second if matched\n", this);

        if (need_overwrite)
        {
            do
            {
                Buffer_pointer extra(cache.reader);
                extra.complete = ((wr + amount) % _size); // (extra.complete + amount) % _size;

                if (update(cache.reader, extra))
                    break;
            }
            while (true);
        }

//        fprintf(stderr, "%p> full write: a=%d/s=%d [r=%d/w=%d]\n", this, amount, _size, reader, writer);

        /* we are talking about buffers here, man! */
        memcpy((void *) &(buffer[wp]), (const void *) value, _block * amount);
    }

    _pointers.writer.complete = ((wp + amount) % _size) + 1;
    _pointers.writer.partial  = 1;

//    if (need_overwrite)
//        fprintf(stdout, "%p> write end: w=%d/r=%d\n", this, _pointers.writer.complete, _pointers.reader.complete);

    return true;
}

/* returns the number of itens that have been read */
unsigned int Ringbuffer_traits::traits_consume(const char * buffer, char * value, unsigned int amount, bool atomic_mode)
{
    /* avoid using different values */
    Buffer_table cache = _pointers;

    const unsigned int available = used_blocks(cache);

    if (atomic_mode && amount > available)
        return false;

    const unsigned int rd = _pointers.reader.complete;

    unsigned int total = std::min(amount, available);

    /* should we go around the buffer for reading? */
    if ((rd + total) >= _size)
    {
        unsigned int rd1 = _size - rd;
        unsigned int rd2 = total - rd1;

//        fprintf(stderr, "%p> partial read: (%d/%d) %d/%d [%d/%d]\n", this, rd1, rd2, total, _size, reader, writer);

        /* two partial consumes (one at the end, another at the beginning) */
        memcpy((void *)  (value),      (const void *) &(buffer[rd]), _block * rd1);
        memcpy((void *) &(value[rd1]), (const void *)  (buffer),     _block * rd2);
    }
    else
    {
//        fprintf(stderr, "%p> full read: %d/%d [%d/%d]\n", this, total, _size, reader, writer);

        /* we are talking about buffers here, man! */
        memcpy((void *) value, (const void *) &(buffer[rd]), _block * total);
    }

    do
    {
        /* jump the reader forward */
        Buffer_pointer index((cache.reader.complete + total) % _size, 0);

        if (update(cache.reader, index))
            break;
    }
    while (true);

//    fprintf(stderr, "%p> read end: %d [block=%d]\n", this, reader, _block);

    return total;
}

/********** TWO-PHASE BUFFER FUNCTIONS ***********/

/* returns the number of itens that have been read */
unsigned int Ringbuffer_traits::traits_consume_begins(const char * buffer, char * value, unsigned int amount, bool atomic_mode)
{
    /* avoid using different values */
    Buffer_table cache = _pointers;

    const unsigned int available = used_blocks(cache);

    if (amount > available)
    {
        if (atomic_mode)
            return false;
    }

    const unsigned int rd = _pointers.reader.complete;

    unsigned int total = std::min(amount, available);

    /* should we go around the buffer for reading? */
    if ((rd + total) >= _size)
    {
        unsigned int rd1 = _size - rd;
        unsigned int rd2 = total - rd1;

//        fprintf(stderr, "%p> partial read: (%d/%d) %d/%d [%d/%d]\n", this, rd1, rd2, total, _size, reader, writer);

        /* two partial consumes (one at the end, another at the beginning) */
        memcpy((void *)  (value),      (const void *) &(buffer[rd]), _block * rd1);
        memcpy((void *) &(value[rd1]), (const void *)  (buffer),     _block * rd2);
    }
    else
    {
//        fprintf(stderr, "%p> full read: %d/%d [%d/%d]\n", this, total, _size, reader, writer);

        /* we are talking about buffers here, man! */
        memcpy((void *) value, (const void *) &(buffer[rd]), _block * total);
    }

    return total;
}

bool Ringbuffer_traits::traits_consume_commit(unsigned int amount)
{
    if (amount == 0)
        return true;

    /* avoid using different values */
    Buffer_table cache = _pointers;

    const unsigned int available = used_blocks(cache);

    /* cannot commit more than available! */
    if (amount > available)
        return false;

    unsigned int total = std::min(amount, available);

    do
    {
        /* jump the reader forward */
        Buffer_pointer index((cache.reader.complete + total) % _size, 0);

        if (update(cache.reader, index))
            break;
    }
    while (true);

//    fprintf(stderr, "%p> read end: %d [block=%d]\n", this, reader, _block);

    return true;
}

/********** PARTIAL BUFFER FUNCTIONS (bytes) ***********/

/* writes everything or nothing */
bool Ringbuffer_traits::traits_provide_partial(char * buffer, const char * value, unsigned int amount)
{
    /* avoid using different values */
    Buffer_table cache = _pointers;

    const unsigned int memsize = (_size * _block);

    if (amount > (free_blocks(cache) * _block))
        return false;

    const unsigned int wr = ((cache.writer.complete - 1) * _block) + cache.writer.partial;
    const unsigned int wp = wr - 1;

    /* should we go around the buffer for writing? */
    if ((wr + amount) > memsize)
    {
//        fprintf(stderr, "%p> first if matched\n", this);

        unsigned int wr1 = memsize - wr + 1; /* writer is already 1 position after */
        unsigned int wr2 = amount - wr1;

//        fprintf(stderr, "%p> partial write: (%d/%d) %d/%d [%d/%d]\n", this, wr1, wr2, amount, _size, reader, writer);

        /* two partial writes (one at the end, another at the beginning) */
        memcpy((void *) &(buffer[wp]), (const void *)  (value),      wr1);
        memcpy((void *)  (buffer),     (const void *) &(value[wr1]), wr2);
    }
    else
    {
//        fprintf(stderr, "%p> second if matched\n", this);

//        fprintf(stderr, "%p> full write: a=%d/s=%d [r=%d/w=%d]\n", this, amount, _size, reader, writer);

        /* we are talking about buffers here, man! */
        memcpy((void *) &(buffer[wp]), (const void *) value, amount);
    }

    const unsigned int new_wp = (wp + amount) % memsize;

    _pointers.writer.complete = (unsigned int)(floor((double)new_wp / (double)_block) + 1);
    _pointers.writer.partial  = (new_wp % _block) + 1;

//    if (need_overwrite)
//        fprintf(stdout, "%p> write end: w=%d/r=%d\n", this, _pointers.writer.complete, _pointers.reader.complete);

    return true;
}

/* returns the number of bytes that have been read */
unsigned int Ringbuffer_traits::traits_consume_partial(const char * buffer, char * value, unsigned int amount)
{
    /* avoid using different values */
    Buffer_table cache = _pointers;

    const unsigned int available = used_blocks(cache) * _block;

    const unsigned int rd = (_pointers.reader.complete * _block) + _pointers.reader.partial;

    const unsigned int memsize = _size * _block;

    unsigned int total = std::min(amount, available);

    /* should we go around the buffer for reading? */
    if ((rd + total) >= _size)
    {
        unsigned int rd1 = memsize - rd;
        unsigned int rd2 = total - rd1;

//        fprintf(stderr, "%p> partial read: (%d/%d) %d/%d [%d/%d]\n", this, rd1, rd2, total, _size, reader, writer);

        /* two partial consumes (one at the end, another at the beginning) */
        memcpy((void *)  (value),      (const void *) &(buffer[rd]), rd1);
        memcpy((void *) &(value[rd1]), (const void *)  (buffer),     rd2);
    }
    else
    {
//        fprintf(stderr, "%p> full read: %d/%d [%d/%d]\n", this, total, _size, reader, writer);

        /* we are talking about buffers here, man! */
        memcpy((void *) value, (const void *) &(buffer[rd]), total);
    }

    do
    {
        const unsigned int new_rd = (((cache.reader.complete * _block) + cache.reader.partial) + total) % memsize;

        /* jump the reader forward */
        Buffer_pointer index((unsigned int)floor((double)new_rd / (double)_block), (unsigned short)(new_rd % _block));

        if (update(cache.reader, index))
            break;
    }
    while (true);

//    fprintf(stderr, "%p> read end: %d [block=%d]\n", this, reader, _block);

    return total;
}

/********** IO FUNCTIONS **********/

/* returns the number of items written to from buffer to stream */
unsigned int Ringbuffer_traits::traits_put(const char * buffer, std::ostream &fd, unsigned int amount)
{
    /* avoid using different values */
    Buffer_table cache = _pointers;

    const unsigned int available = used_blocks(cache);

    if (amount > available)
        return false;

    const unsigned int wr = _pointers.writer.complete;
    const unsigned int rd = _pointers.reader.complete;

    unsigned int total = std::min(amount, available);

    /* should we go around the buffer for reading? */
    if ((rd + total) >= _size)
    {
        unsigned int rd1 = _size - rd;
        unsigned int rd2 = total - rd1;

//        fprintf(stderr, "%p> partial read: (%d/%d) %d/%d [%d/%d]\n", this, rd1, rd2, total, _size, reader, writer);

        /* two partial consumes (one at the end, another at the beginning) */
        fd.write((const char *) &(buffer[rd]), _block * rd1);
        fd.write((const char *)  (buffer),     _block * rd2);
    }
    else
    {
//      fprintf(stderr, "%p> full read: %d/%d [%d/%d]\n", this, total, _size, reader, writer);
        fd.write((const char *) &(buffer[rd]), _block * total);
    }

    do
    {
        /* jump the reader forward */
        Buffer_pointer index((cache.reader.complete + total) % _size, 0);

        if (update(cache.reader, index))
            break;
    }
    while (true);

//    fprintf(stderr, "%p> read end: %d [block=%d]\n", this, reader, _block);

    return total;
}

/* returns number of items read from stream to buffer */
unsigned int Ringbuffer_traits::traits_get(char * buffer, std::istream &fd, unsigned int amount)
{
    /* avoid using different values */
    Buffer_table cache = _pointers;

    if (amount > free_blocks(cache))
        return false;

    const unsigned int wr = cache.writer.complete;
    const unsigned int wp = cache.writer.complete - 1;

    unsigned int real_amount = 0;

    /* should we go around the buffer for writing? */
    if ((wr + amount) > _size)
    {
//        fprintf(stderr, "%p> first if matched\n", this);

        unsigned int wr1 = _size - wr + 1; /* writer is already 1 position after */
        unsigned int wr2 = amount - wr1;

//        fprintf(stderr, "%p> partial write: (%d/%d) %d/%d [%d/%d]\n", this, wr1, wr2, amount, _size, reader, writer);

        /* two partial writes (one at the end, another at the beginning) */
        unsigned int char_amount = 0;

        /* one partial write on the buffer (at the end) */
        fd.read((char *) &(buffer[wp]), _block * wr1);
        char_amount += fd.gcount();

        if (fd.gcount() == (int)(_block * wr1))
        {
            /* another partial write on the buffer (at the beginning) */
            fd.read((char *) (buffer), _block * wr2);
            char_amount += fd.gcount();
        }

        real_amount = char_amount / _block;
    }
    else
    {
//        fprintf(stderr, "%p> second if matched\n", this);

//        fprintf(stderr, "%p> full write: a=%d/s=%d [r=%d/w=%d]\n", this, amount, _size, reader, writer);

        /* we are talking about buffers here, man! */
        fd.read((char *) &(buffer[wp]), _block * amount);

        real_amount = fd.gcount() / _block;
    }

    _pointers.writer.complete = ((wp + amount) % _size) + 1;
    _pointers.writer.partial  = 1;

//    fprintf(stdout, "%p> write end: %d\n", this, _pointers.writer.complete);

    return real_amount;
}
