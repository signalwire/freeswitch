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

/********** BUFFER FUNCTIONS **********/

/* writes everything or nothing */
bool Ringbuffer_traits::traits_provide(char * buffer, const char * value, unsigned int amount, bool skip_overwrite)
{
    /* avoid using different values */
    Buffer_table cache = _pointers;

    const unsigned int reader = cache.reader.complete;
    const unsigned int writer = cache.writer.complete;

    const unsigned int dest = cache.writer.complete - 1;

    const bool reader_less = cache.reader.complete < cache.writer.complete;

    if (amount >= _size)
        return false;

    bool ret = true;

    /* should we go around the buffer for writing? */
    if (((writer + amount) > _size) && (reader_less || !skip_overwrite))
    {
        /* Documentation of the formula used in the 'if' below.
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

        if ((_size - (writer - reader + 1)) <= amount)
        {
            if (skip_overwrite)
                return false;

            do
            {
                Buffer_pointer extra(cache.reader);
                extra.complete = (extra.complete + amount) % _size;

                if (update(cache.reader, extra))
                    break;
            }
            while (true);

            ret = false;
        }

        unsigned int wr1 = _size - writer + 1; /* writer is already 1 position after */
        unsigned int wr2 = amount - wr1;

//        fprintf(stderr, "%p> partial write: (%d/%d) %d/%d [%d/%d]\n", this, wr1, wr2, amount, _size, reader, writer);

        /* two partial writes (one at the end, another at the beginning) */
        memcpy((void *) &(buffer[dest]), (const void *)  (value),      _block * wr1);
        memcpy((void *)  (buffer),       (const void *) &(value[wr1]), _block * wr2);
    }
    else
    {
        if (!reader_less && ((reader - writer) <= amount))
        {
            if (skip_overwrite)
                return false;

            do
            {
                Buffer_pointer extra(cache.reader);
                extra.complete = (extra.complete + amount) % _size;

                if (update(cache.reader, extra))
                    break;
            }
            while (true);

            ret = false;
        }

//        fprintf(stderr, "%p> full write: a=%d/s=%d [r=%d/w=%d]\n", this, amount, _size, reader, writer);

        /* we are talking about buffers here, man! */
        memcpy((void *) &(buffer[dest]), (const void *) value, _block * amount);
    }

    _pointers.writer.complete = ((dest + amount) % _size) + 1;
    _pointers.writer.partial  = 1;

//    fprintf(stderr, "%p> write end: %d [block=%d]\n", this, writer, _block);

    return ret;
}

/* returns the number of itens that have been read */
unsigned int Ringbuffer_traits::traits_consume(const char * buffer, char * value, unsigned int amount, bool atomic_mode)
{
    /* avoid using different values */
    Buffer_table cache = _pointers;

    const unsigned int writer = _pointers.writer.complete;
    const unsigned int reader = _pointers.reader.complete;

    const bool writer_less = writer < reader;

    unsigned int total = 0;

    /* should we go around the buffer for reading? */
    if (writer_less && (reader + amount >= _size))
    {
        total = std::min(_size - (reader - writer + 1), amount);

        if ((total == 0) || (atomic_mode && (total < amount)))
            return 0;

        unsigned int rd1 = _size - reader;
        unsigned int rd2 = total - rd1;

//        fprintf(stderr, "%p> partial read: (%d/%d) %d/%d [%d/%d]\n", this, rd1, rd2, total, _size, reader, writer);

        /* two partial consumes (one at the end, another at the beginning) */
        memcpy((void *)  (value),      (const void *) &(buffer[reader]), _block * rd1);
        memcpy((void *) &(value[rd1]), (const void *)  (buffer),         _block * rd2);
    }
    else
    {
        total = std::min((!writer_less ? writer - (reader + 1) : amount), amount);

        if ((total == 0) || (atomic_mode && (total < amount)))
            return 0;

//        fprintf(stderr, "%p> full read: %d/%d [%d/%d]\n", this, total, _size, reader, writer);

        /* we are talking about buffers here, man! */
        memcpy((void *) value, (const void *) &(buffer[reader]), _block * total);
    }

    do
    {
        /* jump the reader forward */
        Buffer_pointer index((cache.reader.complete + total) % _size);

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
    Buffer_table cache = _pointers;

    /* avoid using different values */
    const unsigned int reader = cache.reader.complete;
    const unsigned int writer = cache.writer.complete;

    const bool writer_less = writer < reader;

    unsigned int total = 0;

    /* should we go around the buffer for reading? */
    if (writer_less && (reader + amount >= _size))
    {
        total = std::min(_size - (reader - writer + 1), amount);

        if ((total == 0) || (atomic_mode && (total < amount)))
            return 0;

        unsigned int rd1 = _size - reader;
        unsigned int rd2 = total - rd1;

//        fprintf(stderr, "%p> partial read: (%d/%d) %d/%d [%d/%d]\n", this, rd1, rd2, total, _size, reader, writer);

        /* two partial consumes (one at the end, another at the beginning) */
        memcpy((void *)  (value),      (const void *) &(buffer[reader]), _block * rd1);
        memcpy((void *) &(value[rd1]), (const void *)  (buffer),         _block * rd2);
    }
    else
    {
        total = std::min((!writer_less ? writer - (reader + 1) : amount), amount);

        if ((total == 0) || (atomic_mode && (total < amount)))
            return 0;

//        fprintf(stderr, "%p> full read: %d/%d [%d/%d]\n", this, total, _size, reader, writer);

        /* we are talking about buffers here, man! */
        memcpy((void *) value, (const void *) &(buffer[reader]), _block * total);
    }

//    fprintf(stderr, "%p> read end: %d [%d]\n", this, _reader, _reader_partial);

    return total;
}

bool Ringbuffer_traits::traits_consume_commit(unsigned int amount)
{
   	/* avoid using different values */
    Buffer_table cache = _pointers;

   	const unsigned int writer = cache.writer.complete;
    const unsigned int reader = cache.reader.complete;

    const bool writer_less = writer < reader;

    unsigned int total = 0;

    /* should we go around the buffer for reading? */
    if (writer_less && (reader + amount >= _size))
    {
        total = std::min(_size - (reader - writer + 1), amount);

        if (total < amount)
            return false;

//        fprintf(stderr, "%p> partial read: (%d/%d) %d/%d [%d/%d]\n", this, rd1, rd2, total, _size, reader, writer);
    }
    else
    {
        total = std::min((!writer_less ? writer - (reader + 1) : amount), amount);

        if (total < amount)
            return false;

//        fprintf(stderr, "%p> full read: %d/%d [%d/%d]\n", this, total, _size, reader, writer);
    }

    do
    {
        /* jump the reader forward */
        Buffer_pointer index(cache.reader);
        index.complete = (index.complete + total) % _size;

        if (update(cache.reader, index))
            break;
    }
    while (true);

//    fprintf(stderr, "%p> read end: %d [%d]\n", this, _reader, _reader_partial);

    return true;
}

/********** PARTIAL BUFFER FUNCTIONS (bytes) ***********/

/* writes everything or nothing */
bool Ringbuffer_traits::traits_provide_partial(char * buffer, const char * value, unsigned int amount)
{
    /* avoid using different values */
    Buffer_table cache = _pointers;

    const unsigned int reader = (cache.reader.complete * _block) + cache.reader.partial;
    const unsigned int writer = ((cache.writer.complete - 1) * _block) + cache.writer.partial;

    const unsigned int size = _size * _block;
    const unsigned int dest = writer - 1;

//    fprintf(stderr, "%p> provide partial: %d/%d [%d/%d]\n", this, reader, writer, amount, size);

    const bool reader_less = reader < writer;

    /* should we go around the buffer for writing? */
    if (reader_less && ((writer + amount) > size))
    {
        /* Documentation of the formula used in the 'if' below.
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

        if ((size - (writer - reader + 1)) <= amount)
            return false;

        unsigned int wr1 = size - writer + 1; /* writer is already 1 position after */
        unsigned int wr2 = amount - wr1;

//        fprintf(stderr, "%p> p partial write: (%d/%d) %d/%d [%d/%d]\n", this, wr1, wr2, amount, size, reader, writer);

        /* two partial writes (one at the end, another at the beginning) */
        memcpy((void *) &(buffer[dest]), (const void *)  (value),      wr1);
        memcpy((void *)  (buffer),       (const void *) &(value[wr1]), wr2);
    }
    else
    {
        if (!reader_less && ((reader - writer) <= amount))
            return false;

//        fprintf(stderr, "%p> p full write: %d/%d [r=%d/w=%d]\n", this, amount, size, reader, writer);

        /* we are talking about buffers here, man! */
        memcpy((void *) &(buffer[dest]), (const void *) value, amount);
    }

    unsigned int new_writer = ((dest + amount) % size) + 1;

    /* update "full length position" */
    _pointers.writer.complete = (unsigned int)floor((double) new_writer / (double)_block)+1;
    _pointers.writer.partial  = (unsigned short)(new_writer % _block);

//    fprintf(stderr, "%p> p write end: %d [block=%d]\n", this, new_writer, _block);

    return true;
}

/* returns the number of bytes that have been read */
unsigned int Ringbuffer_traits::traits_consume_partial(const char * buffer, char * value, unsigned int amount)
{
    /* avoid using different values */
    Buffer_table cache = _pointers;

    const unsigned int writer = ((cache.writer.complete - 1) * _block) + cache.writer.partial;
    const unsigned int reader = (cache.reader.complete * _block) + cache.reader.partial;

    const unsigned int size = _size * _block;

//    fprintf(stderr, "%p> consume partial: %d/%d [%d/%d]\n", this, reader, writer, amount, size);

    const bool writer_less = writer < reader;

    unsigned int total = 0;

    /* should we go around the buffer for reading? */
    if (writer_less && (reader + amount >= size))
    {
        total = std::min(size - (reader - writer + 1), amount);

        if (total == 0)
            return 0;

        unsigned int rd1 = size - reader;
        unsigned int rd2 = total - rd1;

//        fprintf(stderr, "%p> p partial read: (%d/%d) %d/%d [%d/%d]\n", this, rd1, rd2, total, size, reader, writer);

        /* two partial consumes (one at the end, another at the beginning) */
        memcpy((void *)  (value),      (const void *) &(buffer[reader]), rd1);
        memcpy((void *) &(value[rd1]), (const void *)  (buffer),         rd2);
    }
    else
    {
        total = std::min((writer_less ? amount : writer - (reader + 1)), amount);

        if (total == 0)
            return 0;

//        fprintf(stderr, "%p> p full read: %d/%d [r=%d/w=%d]\n", this, total, size, reader, writer);

        /* we are talking about buffers here, man! */
        memcpy((void *) value, (const void *) &(buffer[reader]), total);
    }

    do
    {
        unsigned int new_reader = (((cache.reader.complete * _block) + cache.reader.partial) + total) % size;

        /* jump the reader forward */
        Buffer_pointer index((unsigned int)floor((double)new_reader / (double)_block),
            (unsigned short)(new_reader % _block));

        if (update(cache.reader, index))
        {
//            fprintf(stderr, "%p> p read end: %d [block=%d]\n", this, new_reader, _block);
            break;
        }
    }
    while (true);

    return total;
}



/********** IO FUNCTIONS **********/

/* returns the number of items written to from buffer to stream */
unsigned int Ringbuffer_traits::traits_put(const char * buffer, std::ostream &fd, unsigned int amount)
{
    /* avoid using different values */
    Buffer_table cache = _pointers;

    const unsigned int reader = cache.reader.complete;
    const unsigned int writer = cache.writer.complete;

    const bool writer_less = writer < reader;

    unsigned int total = 0;

    /* should we go around the buffer for reading? */
    if (writer_less && (reader + amount >= _size))
    {
        total = std::min(_size - (reader - writer + 1), amount);

        if (total == 0)
            return 0;

        unsigned int rd1 = _size - reader;
        unsigned int rd2 = total - rd1;

//        fprintf(stderr, "%p> partial read: (%d/%d) %d/%d [%d/%d]\n", this, rd1, rd2, total, _size, reader, writer);

        /* two partial consumes (one at the end, another at the beginning) */
        fd.write((const char *) &(buffer[reader]), _block * rd1);
        fd.write((const char *)  (buffer),         _block * rd2);
    }
    else
    {
        total = std::min((!writer_less ? writer - (reader + 1) : amount), amount);

        if (total == 0)
            return 0;

//        fprintf(stderr, "%p> full read: %d/%d [%d/%d]\n", this, total, _size, reader, writer);

        /* we are talking about buffers here, man! */
        fd.write((const char *) &(buffer[reader]), _block * total);
    }

    do
    {
        /* jump the reader forward */
        Buffer_pointer index(cache.reader);
        index.complete = (index.complete + total) % _size;

        if (update(cache.reader, index))
            break;
    }
    while (true);

    return total;
}

/* returns number of items read from stream to buffer */
unsigned int Ringbuffer_traits::traits_get(char * buffer, std::istream &fd, unsigned int amount)
{
    /* avoid using different values */
    Buffer_table cache = _pointers;

    const unsigned int reader = cache.reader.complete;
    const unsigned int writer = cache.writer.complete;

    const unsigned int dest = writer - 1;

    const bool reader_less = reader < writer;

    unsigned int real_amount = 0;

    /* should we go around the buffer for writing? */
    if (reader_less && ((writer + amount) > _size))
    {
        /* Documentation of the formula used in the 'if' below.
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

        if ((_size - (writer - reader + 1)) <= amount)
            return false;

        unsigned int wr1 = _size - writer + 1; /* writer is already 1 position after */
        unsigned int wr2 = amount - wr1;

//        fprintf(stderr, "%p> partial write: (%d/%d) %d/%d [%d/%d]\n", this, wr1, wr2, amount, _size, reader, writer);

        unsigned int char_amount = 0;

        /* one partial write on the buffer (at the end) */
        fd.read((char *) &(buffer[dest]), _block * wr1);
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
        if (!reader_less && ((reader - writer) <= amount))
            return false;

//        fprintf(stderr, "%p> full write: %d/%d [%d/%d]\n", this, amount, _size, reader, writer);

        /* we are talking about buffers here, man! */
        fd.read((char *) &(buffer[dest]), _block * amount);

        real_amount = fd.gcount() / _block;
    }

    _pointers.writer.complete = ((dest + real_amount) % _size) + 1;
    _pointers.writer.partial  = 1;

    return real_amount;
}
