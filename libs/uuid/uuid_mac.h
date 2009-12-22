/*
**  OSSP uuid - Universally Unique Identifier
**  Copyright (c) 2004-2008 Ralf S. Engelschall <rse@engelschall.com>
**  Copyright (c) 2004-2008 The OSSP Project <http://www.ossp.org/>
**
**  This file is part of OSSP uuid, a library for the generation
**  of UUIDs which can found at http://www.ossp.org/pkg/lib/uuid/
**
**  Permission to use, copy, modify, and distribute this software for
**  any purpose with or without fee is hereby granted, provided that
**  the above copyright notice and this permission notice appear in all
**  copies.
**
**  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
**  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
**  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
**  IN NO EVENT SHALL THE AUTHORS AND COPYRIGHT HOLDERS AND THEIR
**  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
**  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
**  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
**  USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
**  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
**  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
**  OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
**  SUCH DAMAGE.
**
**  uuid_mac.h: Media Access Control (MAC) resolver API definition
*/

#ifndef __UUID_MAC_H__
#define __UUID_MAC_H__

#include <string.h> /* size_t */

#define MAC_PREFIX uuid_

/* embedding support */
#ifdef MAC_PREFIX
#if defined(__STDC__) || defined(__cplusplus)
#define __MAC_CONCAT(x,y) x ## y
#define MAC_CONCAT(x,y) __MAC_CONCAT(x,y)
#else
#define __MAC_CONCAT(x) x
#define MAC_CONCAT(x,y) __MAC_CONCAT(x)y
#endif
#define mac_address MAC_CONCAT(MAC_PREFIX,mac_address)
#endif

#define MAC_LEN 6

extern int mac_address(unsigned char *_data_ptr, size_t _data_len);

#endif /* __UUID_MAC_H__ */

