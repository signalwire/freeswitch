##
##  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
##
##  Use of this source code is governed by a BSD-style license
##  that can be found in the LICENSE file in the root of the source
##  tree. An additional intellectual property rights grant can be found
##  in the file PATENTS.  All contributing project authors may
##  be found in the AUTHORS file in the root of the source tree.
##

VP10_CX_EXPORTS += exports_enc

VP10_CX_SRCS-yes += $(VP10_COMMON_SRCS-yes)
VP10_CX_SRCS-no  += $(VP10_COMMON_SRCS-no)
VP10_CX_SRCS_REMOVE-yes += $(VP10_COMMON_SRCS_REMOVE-yes)
VP10_CX_SRCS_REMOVE-no  += $(VP10_COMMON_SRCS_REMOVE-no)

VP10_CX_SRCS-yes += vp10_cx_iface.c

VP10_CX_SRCS-yes += encoder/bitstream.c
VP10_CX_SRCS-yes += encoder/context_tree.c
VP10_CX_SRCS-yes += encoder/context_tree.h
VP10_CX_SRCS-yes += encoder/cost.h
VP10_CX_SRCS-yes += encoder/cost.c
VP10_CX_SRCS-yes += encoder/dct.c
VP10_CX_SRCS-$(CONFIG_VP9_TEMPORAL_DENOISING) += encoder/denoiser.c
VP10_CX_SRCS-$(CONFIG_VP9_TEMPORAL_DENOISING) += encoder/denoiser.h
VP10_CX_SRCS-yes += encoder/encodeframe.c
VP10_CX_SRCS-yes += encoder/encodeframe.h
VP10_CX_SRCS-yes += encoder/encodemb.c
VP10_CX_SRCS-yes += encoder/encodemv.c
VP10_CX_SRCS-yes += encoder/ethread.h
VP10_CX_SRCS-yes += encoder/ethread.c
VP10_CX_SRCS-yes += encoder/extend.c
VP10_CX_SRCS-yes += encoder/firstpass.c
VP10_CX_SRCS-yes += encoder/block.h
VP10_CX_SRCS-yes += encoder/bitstream.h
VP10_CX_SRCS-yes += encoder/encodemb.h
VP10_CX_SRCS-yes += encoder/encodemv.h
VP10_CX_SRCS-yes += encoder/extend.h
VP10_CX_SRCS-yes += encoder/firstpass.h
VP10_CX_SRCS-yes += encoder/lookahead.c
VP10_CX_SRCS-yes += encoder/lookahead.h
VP10_CX_SRCS-yes += encoder/mcomp.h
VP10_CX_SRCS-yes += encoder/encoder.h
VP10_CX_SRCS-yes += encoder/quantize.h
VP10_CX_SRCS-yes += encoder/ratectrl.h
VP10_CX_SRCS-yes += encoder/rd.h
VP10_CX_SRCS-yes += encoder/rdopt.h
VP10_CX_SRCS-yes += encoder/tokenize.h
VP10_CX_SRCS-yes += encoder/treewriter.h
VP10_CX_SRCS-yes += encoder/mcomp.c
VP10_CX_SRCS-yes += encoder/encoder.c
VP10_CX_SRCS-yes += encoder/picklpf.c
VP10_CX_SRCS-yes += encoder/picklpf.h
VP10_CX_SRCS-yes += encoder/quantize.c
VP10_CX_SRCS-yes += encoder/ratectrl.c
VP10_CX_SRCS-yes += encoder/rd.c
VP10_CX_SRCS-yes += encoder/rdopt.c
VP10_CX_SRCS-yes += encoder/segmentation.c
VP10_CX_SRCS-yes += encoder/segmentation.h
VP10_CX_SRCS-yes += encoder/speed_features.c
VP10_CX_SRCS-yes += encoder/speed_features.h
VP10_CX_SRCS-yes += encoder/subexp.c
VP10_CX_SRCS-yes += encoder/subexp.h
VP10_CX_SRCS-yes += encoder/resize.c
VP10_CX_SRCS-yes += encoder/resize.h
VP10_CX_SRCS-$(CONFIG_INTERNAL_STATS) += encoder/blockiness.c

VP10_CX_SRCS-yes += encoder/tokenize.c
VP10_CX_SRCS-yes += encoder/treewriter.c
VP10_CX_SRCS-yes += encoder/aq_variance.c
VP10_CX_SRCS-yes += encoder/aq_variance.h
VP10_CX_SRCS-yes += encoder/aq_cyclicrefresh.c
VP10_CX_SRCS-yes += encoder/aq_cyclicrefresh.h
VP10_CX_SRCS-yes += encoder/aq_complexity.c
VP10_CX_SRCS-yes += encoder/aq_complexity.h
VP10_CX_SRCS-yes += encoder/skin_detection.c
VP10_CX_SRCS-yes += encoder/skin_detection.h
ifeq ($(CONFIG_VP9_POSTPROC),yes)
VP10_CX_SRCS-$(CONFIG_INTERNAL_STATS) += common/postproc.h
VP10_CX_SRCS-$(CONFIG_INTERNAL_STATS) += common/postproc.c
endif
VP10_CX_SRCS-yes += encoder/temporal_filter.c
VP10_CX_SRCS-yes += encoder/temporal_filter.h
VP10_CX_SRCS-yes += encoder/mbgraph.c
VP10_CX_SRCS-yes += encoder/mbgraph.h

VP10_CX_SRCS-$(HAVE_SSE2) += encoder/x86/temporal_filter_apply_sse2.asm
VP10_CX_SRCS-$(HAVE_SSE2) += encoder/x86/quantize_sse2.c
ifeq ($(CONFIG_VP9_HIGHBITDEPTH),yes)
VP10_CX_SRCS-$(HAVE_SSE2) += encoder/x86/highbd_block_error_intrin_sse2.c
endif

ifeq ($(CONFIG_USE_X86INC),yes)
VP10_CX_SRCS-$(HAVE_MMX) += encoder/x86/dct_mmx.asm
VP10_CX_SRCS-$(HAVE_SSE2) += encoder/x86/error_sse2.asm
endif

ifeq ($(ARCH_X86_64),yes)
ifeq ($(CONFIG_USE_X86INC),yes)
VP10_CX_SRCS-$(HAVE_SSSE3) += encoder/x86/quantize_ssse3_x86_64.asm
endif
endif

VP10_CX_SRCS-$(HAVE_SSE2) += encoder/x86/dct_sse2.c
VP10_CX_SRCS-$(HAVE_SSSE3) += encoder/x86/dct_ssse3.c

ifeq ($(CONFIG_VP9_TEMPORAL_DENOISING),yes)
VP10_CX_SRCS-$(HAVE_SSE2) += encoder/x86/denoiser_sse2.c
endif

VP10_CX_SRCS-$(HAVE_AVX2) += encoder/x86/error_intrin_avx2.c

ifneq ($(CONFIG_VP9_HIGHBITDEPTH),yes)
VP10_CX_SRCS-$(HAVE_NEON) += encoder/arm/neon/dct_neon.c
VP10_CX_SRCS-$(HAVE_NEON) += encoder/arm/neon/error_neon.c
endif
VP10_CX_SRCS-$(HAVE_NEON) += encoder/arm/neon/quantize_neon.c

VP10_CX_SRCS-$(HAVE_MSA) += encoder/mips/msa/error_msa.c
VP10_CX_SRCS-$(HAVE_MSA) += encoder/mips/msa/fdct4x4_msa.c
VP10_CX_SRCS-$(HAVE_MSA) += encoder/mips/msa/fdct8x8_msa.c
VP10_CX_SRCS-$(HAVE_MSA) += encoder/mips/msa/fdct16x16_msa.c
VP10_CX_SRCS-$(HAVE_MSA) += encoder/mips/msa/fdct_msa.h
VP10_CX_SRCS-$(HAVE_MSA) += encoder/mips/msa/temporal_filter_msa.c

VP10_CX_SRCS-yes := $(filter-out $(VP10_CX_SRCS_REMOVE-yes),$(VP10_CX_SRCS-yes))
