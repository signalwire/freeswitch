##
##  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
##
##  Use of this source code is governed by a BSD-style license
##  that can be found in the LICENSE file in the root of the source
##  tree. An additional intellectual property rights grant can be found
##  in the file PATENTS.  All contributing project authors may
##  be found in the AUTHORS file in the root of the source tree.
##

VP10_DX_EXPORTS += exports_dec

VP10_DX_SRCS-yes += $(VP10_COMMON_SRCS-yes)
VP10_DX_SRCS-no  += $(VP10_COMMON_SRCS-no)
VP10_DX_SRCS_REMOVE-yes += $(VP10_COMMON_SRCS_REMOVE-yes)
VP10_DX_SRCS_REMOVE-no  += $(VP10_COMMON_SRCS_REMOVE-no)

VP10_DX_SRCS-yes += vp10_dx_iface.c

VP10_DX_SRCS-yes += decoder/decodemv.c
VP10_DX_SRCS-yes += decoder/decodeframe.c
VP10_DX_SRCS-yes += decoder/decodeframe.h
VP10_DX_SRCS-yes += decoder/detokenize.c
VP10_DX_SRCS-yes += decoder/decodemv.h
VP10_DX_SRCS-yes += decoder/detokenize.h
VP10_DX_SRCS-yes += decoder/dthread.c
VP10_DX_SRCS-yes += decoder/dthread.h
VP10_DX_SRCS-yes += decoder/decoder.c
VP10_DX_SRCS-yes += decoder/decoder.h
VP10_DX_SRCS-yes += decoder/dsubexp.c
VP10_DX_SRCS-yes += decoder/dsubexp.h

VP10_DX_SRCS-yes := $(filter-out $(VP10_DX_SRCS_REMOVE-yes),$(VP10_DX_SRCS-yes))
