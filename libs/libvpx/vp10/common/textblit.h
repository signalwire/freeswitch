/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP10_COMMON_TEXTBLIT_H_
#define VP10_COMMON_TEXTBLIT_H_

#ifdef __cplusplus
extern "C" {
#endif

void vp10_blit_text(const char *msg, unsigned char *address, int pitch);

void vp10_blit_line(int x0, int x1, int y0, int y1, unsigned char *image,
                   int pitch);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP10_COMMON_TEXTBLIT_H_
