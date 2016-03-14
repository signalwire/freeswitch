##
##  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
##
##  Use of this source code is governed by a BSD-style license
##  that can be found in the LICENSE file in the root of the source
##  tree. An additional intellectual property rights grant can be found
##  in the file PATENTS.  All contributing project authors may
##  be found in the AUTHORS file in the root of the source tree.
##

VP10_COMMON_SRCS-yes += vp10_common.mk
VP10_COMMON_SRCS-yes += vp10_iface_common.h
VP10_COMMON_SRCS-yes += common/ppflags.h
VP10_COMMON_SRCS-yes += common/alloccommon.c
VP10_COMMON_SRCS-yes += common/blockd.c
VP10_COMMON_SRCS-yes += common/debugmodes.c
VP10_COMMON_SRCS-yes += common/entropy.c
VP10_COMMON_SRCS-yes += common/entropymode.c
VP10_COMMON_SRCS-yes += common/entropymv.c
VP10_COMMON_SRCS-yes += common/frame_buffers.c
VP10_COMMON_SRCS-yes += common/frame_buffers.h
VP10_COMMON_SRCS-yes += common/alloccommon.h
VP10_COMMON_SRCS-yes += common/blockd.h
VP10_COMMON_SRCS-yes += common/common.h
VP10_COMMON_SRCS-yes += common/entropy.h
VP10_COMMON_SRCS-yes += common/entropymode.h
VP10_COMMON_SRCS-yes += common/entropymv.h
VP10_COMMON_SRCS-yes += common/enums.h
VP10_COMMON_SRCS-yes += common/filter.h
VP10_COMMON_SRCS-yes += common/filter.c
VP10_COMMON_SRCS-yes += common/idct.h
VP10_COMMON_SRCS-yes += common/idct.c
VP10_COMMON_SRCS-yes += common/vp10_inv_txfm.h
VP10_COMMON_SRCS-yes += common/vp10_inv_txfm.c
VP10_COMMON_SRCS-yes += common/loopfilter.h
VP10_COMMON_SRCS-yes += common/thread_common.h
VP10_COMMON_SRCS-yes += common/mv.h
VP10_COMMON_SRCS-yes += common/onyxc_int.h
VP10_COMMON_SRCS-yes += common/pred_common.h
VP10_COMMON_SRCS-yes += common/pred_common.c
VP10_COMMON_SRCS-yes += common/quant_common.h
VP10_COMMON_SRCS-yes += common/reconinter.h
VP10_COMMON_SRCS-yes += common/reconintra.h
VP10_COMMON_SRCS-yes += common/vp10_rtcd.c
VP10_COMMON_SRCS-yes += common/vp10_rtcd_defs.pl
VP10_COMMON_SRCS-yes += common/scale.h
VP10_COMMON_SRCS-yes += common/scale.c
VP10_COMMON_SRCS-yes += common/seg_common.h
VP10_COMMON_SRCS-yes += common/seg_common.c
VP10_COMMON_SRCS-yes += common/textblit.h
VP10_COMMON_SRCS-yes += common/tile_common.h
VP10_COMMON_SRCS-yes += common/tile_common.c
VP10_COMMON_SRCS-yes += common/loopfilter.c
VP10_COMMON_SRCS-yes += common/thread_common.c
VP10_COMMON_SRCS-yes += common/mvref_common.c
VP10_COMMON_SRCS-yes += common/mvref_common.h
VP10_COMMON_SRCS-yes += common/quant_common.c
VP10_COMMON_SRCS-yes += common/reconinter.c
VP10_COMMON_SRCS-yes += common/reconintra.c
VP10_COMMON_SRCS-$(CONFIG_POSTPROC_VISUALIZER) += common/textblit.c
VP10_COMMON_SRCS-yes += common/common_data.h
VP10_COMMON_SRCS-yes += common/scan.c
VP10_COMMON_SRCS-yes += common/scan.h
VP10_COMMON_SRCS-yes += common/vp10_fwd_txfm.h
VP10_COMMON_SRCS-yes += common/vp10_fwd_txfm.c

VP10_COMMON_SRCS-$(CONFIG_VP9_POSTPROC) += common/postproc.h
VP10_COMMON_SRCS-$(CONFIG_VP9_POSTPROC) += common/postproc.c
VP10_COMMON_SRCS-$(CONFIG_VP9_POSTPROC) += common/mfqe.h
VP10_COMMON_SRCS-$(CONFIG_VP9_POSTPROC) += common/mfqe.c
ifeq ($(CONFIG_VP9_POSTPROC),yes)
VP10_COMMON_SRCS-$(HAVE_SSE2) += common/x86/mfqe_sse2.asm
VP10_COMMON_SRCS-$(HAVE_SSE2) += common/x86/postproc_sse2.asm
endif

ifneq ($(CONFIG_VP9_HIGHBITDEPTH),yes)
VP10_COMMON_SRCS-$(HAVE_DSPR2)  += common/mips/dspr2/itrans4_dspr2.c
VP10_COMMON_SRCS-$(HAVE_DSPR2)  += common/mips/dspr2/itrans8_dspr2.c
VP10_COMMON_SRCS-$(HAVE_DSPR2)  += common/mips/dspr2/itrans16_dspr2.c
endif

# common (msa)
VP10_COMMON_SRCS-$(HAVE_MSA) += common/mips/msa/idct4x4_msa.c
VP10_COMMON_SRCS-$(HAVE_MSA) += common/mips/msa/idct8x8_msa.c
VP10_COMMON_SRCS-$(HAVE_MSA) += common/mips/msa/idct16x16_msa.c

ifeq ($(CONFIG_VP9_POSTPROC),yes)
VP10_COMMON_SRCS-$(HAVE_MSA) += common/mips/msa/mfqe_msa.c
endif

VP10_COMMON_SRCS-$(HAVE_SSE2) += common/x86/idct_intrin_sse2.c
VP10_COMMON_SRCS-$(HAVE_SSE2) += common/x86/vp10_fwd_txfm_sse2.c
VP10_COMMON_SRCS-$(HAVE_SSE2) += common/x86/vp10_fwd_dct32x32_impl_sse2.h
VP10_COMMON_SRCS-$(HAVE_SSE2) += common/x86/vp10_fwd_txfm_impl_sse2.h

ifneq ($(CONFIG_VP9_HIGHBITDEPTH),yes)
VP10_COMMON_SRCS-$(HAVE_NEON) += common/arm/neon/iht4x4_add_neon.c
VP10_COMMON_SRCS-$(HAVE_NEON) += common/arm/neon/iht8x8_add_neon.c
endif

VP10_COMMON_SRCS-$(HAVE_SSE2) += common/x86/vp10_inv_txfm_sse2.c
VP10_COMMON_SRCS-$(HAVE_SSE2) += common/x86/vp10_inv_txfm_sse2.h

$(eval $(call rtcd_h_template,vp10_rtcd,vp10/common/vp10_rtcd_defs.pl))
