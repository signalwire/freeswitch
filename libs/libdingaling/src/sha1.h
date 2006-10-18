/* 
 * libDingaLing XMPP Jingle Library
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is libDingaLing XMPP Jingle Library
 *
 * The Initial Developer of the Original Code is
 * Steve Reid <steve@edmweb.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Steve Reid <steve@edmweb.com>
 *
 * sha1.h
 *
 */
/*! \file sha1.h
    \brief SHA1
*/

#ifndef __SHA1_H
#define __SHA1_H
#ifdef __cplusplus
extern "C" {
#endif
#ifdef __STUPIDFORMATBUG__
}
#endif

typedef struct {
    unsigned long state[5];
    unsigned long count[2];
    unsigned char buffer[64];
} sha_context_t;

void SHA1Transform(unsigned long state[5], unsigned char buffer[64]);
void SHA1Init(sha_context_t* context);
void SHA1Update(sha_context_t* context, unsigned char* data, unsigned int len);
void SHA1Final(unsigned char digest[20], sha_context_t* context);

#ifdef __cplusplus
}
#endif
#endif
