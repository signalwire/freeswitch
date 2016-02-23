/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <stdio.h>
#include <limits.h>

#include "vpx/vpx_encoder.h"
#include "vpx_dsp/bitwriter_buffer.h"
#include "vpx_dsp/vpx_dsp_common.h"
#include "vpx_mem/vpx_mem.h"
#include "vpx_ports/mem_ops.h"
#include "vpx_ports/system_state.h"

#include "vp10/common/entropy.h"
#include "vp10/common/entropymode.h"
#include "vp10/common/entropymv.h"
#include "vp10/common/mvref_common.h"
#include "vp10/common/pred_common.h"
#include "vp10/common/seg_common.h"
#include "vp10/common/tile_common.h"

#include "vp10/encoder/cost.h"
#include "vp10/encoder/bitstream.h"
#include "vp10/encoder/encodemv.h"
#include "vp10/encoder/mcomp.h"
#include "vp10/encoder/segmentation.h"
#include "vp10/encoder/subexp.h"
#include "vp10/encoder/tokenize.h"

static const struct vp10_token intra_mode_encodings[INTRA_MODES] = {
  {0, 1}, {6, 3}, {28, 5}, {30, 5}, {58, 6}, {59, 6}, {126, 7}, {127, 7},
  {62, 6}, {2, 2}};
static const struct vp10_token switchable_interp_encodings[SWITCHABLE_FILTERS] =
  {{0, 1}, {2, 2}, {3, 2}};
static const struct vp10_token partition_encodings[PARTITION_TYPES] =
  {{0, 1}, {2, 2}, {6, 3}, {7, 3}};
static const struct vp10_token inter_mode_encodings[INTER_MODES] =
  {{2, 2}, {6, 3}, {0, 1}, {7, 3}};

static struct vp10_token ext_tx_encodings[TX_TYPES];

void vp10_encode_token_init() {
  vp10_tokens_from_tree(ext_tx_encodings, vp10_ext_tx_tree);
}

static void write_intra_mode(vpx_writer *w, PREDICTION_MODE mode,
                             const vpx_prob *probs) {
  vp10_write_token(w, vp10_intra_mode_tree, probs, &intra_mode_encodings[mode]);
}

static void write_inter_mode(vpx_writer *w, PREDICTION_MODE mode,
                             const vpx_prob *probs) {
  assert(is_inter_mode(mode));
  vp10_write_token(w, vp10_inter_mode_tree, probs,
                  &inter_mode_encodings[INTER_OFFSET(mode)]);
}

static void encode_unsigned_max(struct vpx_write_bit_buffer *wb,
                                int data, int max) {
  vpx_wb_write_literal(wb, data, get_unsigned_bits(max));
}

static void prob_diff_update(const vpx_tree_index *tree,
                             vpx_prob probs[/*n - 1*/],
                             const unsigned int counts[/*n - 1*/],
                             int n, vpx_writer *w) {
  int i;
  unsigned int branch_ct[32][2];

  // Assuming max number of probabilities <= 32
  assert(n <= 32);

  vp10_tree_probs_from_distribution(tree, branch_ct, counts);
  for (i = 0; i < n - 1; ++i)
    vp10_cond_prob_diff_update(w, &probs[i], branch_ct[i]);
}

static int prob_diff_update_savings(const vpx_tree_index *tree,
                                    vpx_prob probs[/*n - 1*/],
                                    const unsigned int counts[/*n - 1*/],
                                    int n) {
  int i;
  unsigned int branch_ct[32][2];
  int savings = 0;

  // Assuming max number of probabilities <= 32
  assert(n <= 32);
  vp10_tree_probs_from_distribution(tree, branch_ct, counts);
  for (i = 0; i < n - 1; ++i) {
    savings += vp10_cond_prob_diff_update_savings(&probs[i],
                                                  branch_ct[i]);
  }
  return savings;
}

static void write_selected_tx_size(const VP10_COMMON *cm,
                                   const MACROBLOCKD *xd, vpx_writer *w) {
  TX_SIZE tx_size = xd->mi[0]->mbmi.tx_size;
  BLOCK_SIZE bsize = xd->mi[0]->mbmi.sb_type;
  const TX_SIZE max_tx_size = max_txsize_lookup[bsize];
  const vpx_prob *const tx_probs = get_tx_probs2(max_tx_size, xd,
                                                 &cm->fc->tx_probs);
  vpx_write(w, tx_size != TX_4X4, tx_probs[0]);
  if (tx_size != TX_4X4 && max_tx_size >= TX_16X16) {
    vpx_write(w, tx_size != TX_8X8, tx_probs[1]);
    if (tx_size != TX_8X8 && max_tx_size >= TX_32X32)
      vpx_write(w, tx_size != TX_16X16, tx_probs[2]);
  }
}

static int write_skip(const VP10_COMMON *cm, const MACROBLOCKD *xd,
                      int segment_id, const MODE_INFO *mi, vpx_writer *w) {
  if (segfeature_active(&cm->seg, segment_id, SEG_LVL_SKIP)) {
    return 1;
  } else {
    const int skip = mi->mbmi.skip;
    vpx_write(w, skip, vp10_get_skip_prob(cm, xd));
    return skip;
  }
}

static void update_skip_probs(VP10_COMMON *cm, vpx_writer *w,
                              FRAME_COUNTS *counts) {
  int k;

  for (k = 0; k < SKIP_CONTEXTS; ++k)
    vp10_cond_prob_diff_update(w, &cm->fc->skip_probs[k], counts->skip[k]);
}

static void update_switchable_interp_probs(VP10_COMMON *cm, vpx_writer *w,
                                           FRAME_COUNTS *counts) {
  int j;
  for (j = 0; j < SWITCHABLE_FILTER_CONTEXTS; ++j)
    prob_diff_update(vp10_switchable_interp_tree,
                     cm->fc->switchable_interp_prob[j],
                     counts->switchable_interp[j], SWITCHABLE_FILTERS, w);
}

static void update_ext_tx_probs(VP10_COMMON *cm, vpx_writer *w) {
  const int savings_thresh = vp10_cost_one(GROUP_DIFF_UPDATE_PROB) -
                             vp10_cost_zero(GROUP_DIFF_UPDATE_PROB);
  int i, j;

  int savings = 0;
  int do_update = 0;
  for (i = TX_4X4; i < EXT_TX_SIZES; ++i) {
    for (j = 0; j < TX_TYPES; ++j)
      savings += prob_diff_update_savings(
          vp10_ext_tx_tree, cm->fc->intra_ext_tx_prob[i][j],
          cm->counts.intra_ext_tx[i][j], TX_TYPES);
  }
  do_update = savings > savings_thresh;
  vpx_write(w, do_update, GROUP_DIFF_UPDATE_PROB);
  if (do_update) {
    for (i = TX_4X4; i < EXT_TX_SIZES; ++i) {
      for (j = 0; j < TX_TYPES; ++j)
        prob_diff_update(vp10_ext_tx_tree,
                         cm->fc->intra_ext_tx_prob[i][j],
                         cm->counts.intra_ext_tx[i][j],
                         TX_TYPES, w);
    }
  }
  savings = 0;
  do_update = 0;
  for (i = TX_4X4; i < EXT_TX_SIZES; ++i) {
    savings += prob_diff_update_savings(
        vp10_ext_tx_tree, cm->fc->inter_ext_tx_prob[i],
        cm->counts.inter_ext_tx[i], TX_TYPES);
  }
  do_update = savings > savings_thresh;
  vpx_write(w, do_update, GROUP_DIFF_UPDATE_PROB);
  if (do_update) {
    for (i = TX_4X4; i < EXT_TX_SIZES; ++i) {
      prob_diff_update(vp10_ext_tx_tree,
                       cm->fc->inter_ext_tx_prob[i],
                       cm->counts.inter_ext_tx[i],
                       TX_TYPES, w);
    }
  }
}

static void pack_mb_tokens(vpx_writer *w,
                           TOKENEXTRA **tp, const TOKENEXTRA *const stop,
                           vpx_bit_depth_t bit_depth, const TX_SIZE tx) {
  TOKENEXTRA *p = *tp;
#if !CONFIG_MISC_FIXES
  (void) tx;
#endif

  while (p < stop && p->token != EOSB_TOKEN) {
    const int t = p->token;
    const struct vp10_token *const a = &vp10_coef_encodings[t];
    int i = 0;
    int v = a->value;
    int n = a->len;
#if CONFIG_VP9_HIGHBITDEPTH
    const vp10_extra_bit *b;
    if (bit_depth == VPX_BITS_12)
      b = &vp10_extra_bits_high12[t];
    else if (bit_depth == VPX_BITS_10)
      b = &vp10_extra_bits_high10[t];
    else
      b = &vp10_extra_bits[t];
#else
    const vp10_extra_bit *const b = &vp10_extra_bits[t];
    (void) bit_depth;
#endif  // CONFIG_VP9_HIGHBITDEPTH

    /* skip one or two nodes */
    if (p->skip_eob_node) {
      n -= p->skip_eob_node;
      i = 2 * p->skip_eob_node;
    }

    // TODO(jbb): expanding this can lead to big gains.  It allows
    // much better branch prediction and would enable us to avoid numerous
    // lookups and compares.

    // If we have a token that's in the constrained set, the coefficient tree
    // is split into two treed writes.  The first treed write takes care of the
    // unconstrained nodes.  The second treed write takes care of the
    // constrained nodes.
    if (t >= TWO_TOKEN && t < EOB_TOKEN) {
      int len = UNCONSTRAINED_NODES - p->skip_eob_node;
      int bits = v >> (n - len);
      vp10_write_tree(w, vp10_coef_tree, p->context_tree, bits, len, i);
      vp10_write_tree(w, vp10_coef_con_tree,
                     vp10_pareto8_full[p->context_tree[PIVOT_NODE] - 1],
                     v, n - len, 0);
    } else {
      vp10_write_tree(w, vp10_coef_tree, p->context_tree, v, n, i);
    }

    if (b->base_val) {
      const int e = p->extra, l = b->len;
#if CONFIG_MISC_FIXES
      int skip_bits =
          (b->base_val == CAT6_MIN_VAL) ? TX_SIZES - 1 - tx : 0;
#else
      int skip_bits = 0;
#endif

      if (l) {
        const unsigned char *pb = b->prob;
        int v = e >> 1;
        int n = l;              /* number of bits in v, assumed nonzero */
        int i = 0;

        do {
          const int bb = (v >> --n) & 1;
          if (skip_bits) {
            skip_bits--;
            assert(!bb);
          } else {
            vpx_write(w, bb, pb[i >> 1]);
          }
          i = b->tree[i + bb];
        } while (n);
      }

      vpx_write_bit(w, e & 1);
    }
    ++p;
  }

  *tp = p;
}

static void write_segment_id(vpx_writer *w, const struct segmentation *seg,
                             const struct segmentation_probs *segp,
                             int segment_id) {
  if (seg->enabled && seg->update_map)
    vp10_write_tree(w, vp10_segment_tree, segp->tree_probs, segment_id, 3, 0);
}

// This function encodes the reference frame
static void write_ref_frames(const VP10_COMMON *cm, const MACROBLOCKD *xd,
                             vpx_writer *w) {
  const MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const int is_compound = has_second_ref(mbmi);
  const int segment_id = mbmi->segment_id;

  // If segment level coding of this signal is disabled...
  // or the segment allows multiple reference frame options
  if (segfeature_active(&cm->seg, segment_id, SEG_LVL_REF_FRAME)) {
    assert(!is_compound);
    assert(mbmi->ref_frame[0] ==
               get_segdata(&cm->seg, segment_id, SEG_LVL_REF_FRAME));
  } else {
    // does the feature use compound prediction or not
    // (if not specified at the frame/segment level)
    if (cm->reference_mode == REFERENCE_MODE_SELECT) {
      vpx_write(w, is_compound, vp10_get_reference_mode_prob(cm, xd));
    } else {
      assert(!is_compound == (cm->reference_mode == SINGLE_REFERENCE));
    }

    if (is_compound) {
      vpx_write(w, mbmi->ref_frame[0] == GOLDEN_FRAME,
                vp10_get_pred_prob_comp_ref_p(cm, xd));
    } else {
      const int bit0 = mbmi->ref_frame[0] != LAST_FRAME;
      vpx_write(w, bit0, vp10_get_pred_prob_single_ref_p1(cm, xd));
      if (bit0) {
        const int bit1 = mbmi->ref_frame[0] != GOLDEN_FRAME;
        vpx_write(w, bit1, vp10_get_pred_prob_single_ref_p2(cm, xd));
      }
    }
  }
}

static void pack_inter_mode_mvs(VP10_COMP *cpi, const MODE_INFO *mi,
                                vpx_writer *w) {
  VP10_COMMON *const cm = &cpi->common;
  const nmv_context *nmvc = &cm->fc->nmvc;
  const MACROBLOCK *const x = &cpi->td.mb;
  const MACROBLOCKD *const xd = &x->e_mbd;
  const struct segmentation *const seg = &cm->seg;
#if CONFIG_MISC_FIXES
  const struct segmentation_probs *const segp = &cm->fc->seg;
#else
  const struct segmentation_probs *const segp = &cm->segp;
#endif
  const MB_MODE_INFO *const mbmi = &mi->mbmi;
  const MB_MODE_INFO_EXT *const mbmi_ext = x->mbmi_ext;
  const PREDICTION_MODE mode = mbmi->mode;
  const int segment_id = mbmi->segment_id;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  const int allow_hp = cm->allow_high_precision_mv;
  const int is_inter = is_inter_block(mbmi);
  const int is_compound = has_second_ref(mbmi);
  int skip, ref;

  if (seg->update_map) {
    if (seg->temporal_update) {
      const int pred_flag = mbmi->seg_id_predicted;
      vpx_prob pred_prob = vp10_get_pred_prob_seg_id(segp, xd);
      vpx_write(w, pred_flag, pred_prob);
      if (!pred_flag)
        write_segment_id(w, seg, segp, segment_id);
    } else {
      write_segment_id(w, seg, segp, segment_id);
    }
  }

  skip = write_skip(cm, xd, segment_id, mi, w);

  if (!segfeature_active(seg, segment_id, SEG_LVL_REF_FRAME))
    vpx_write(w, is_inter, vp10_get_intra_inter_prob(cm, xd));

  if (bsize >= BLOCK_8X8 && cm->tx_mode == TX_MODE_SELECT &&
      !(is_inter && skip) && !xd->lossless[segment_id]) {
    write_selected_tx_size(cm, xd, w);
  }

  if (!is_inter) {
    if (bsize >= BLOCK_8X8) {
      write_intra_mode(w, mode, cm->fc->y_mode_prob[size_group_lookup[bsize]]);
    } else {
      int idx, idy;
      const int num_4x4_w = num_4x4_blocks_wide_lookup[bsize];
      const int num_4x4_h = num_4x4_blocks_high_lookup[bsize];
      for (idy = 0; idy < 2; idy += num_4x4_h) {
        for (idx = 0; idx < 2; idx += num_4x4_w) {
          const PREDICTION_MODE b_mode = mi->bmi[idy * 2 + idx].as_mode;
          write_intra_mode(w, b_mode, cm->fc->y_mode_prob[0]);
        }
      }
    }
    write_intra_mode(w, mbmi->uv_mode, cm->fc->uv_mode_prob[mode]);
  } else {
    const int mode_ctx = mbmi_ext->mode_context[mbmi->ref_frame[0]];
    const vpx_prob *const inter_probs = cm->fc->inter_mode_probs[mode_ctx];
    write_ref_frames(cm, xd, w);

    // If segment skip is not enabled code the mode.
    if (!segfeature_active(seg, segment_id, SEG_LVL_SKIP)) {
      if (bsize >= BLOCK_8X8) {
        write_inter_mode(w, mode, inter_probs);
      }
    }

    if (cm->interp_filter == SWITCHABLE) {
      const int ctx = vp10_get_pred_context_switchable_interp(xd);
      vp10_write_token(w, vp10_switchable_interp_tree,
                      cm->fc->switchable_interp_prob[ctx],
                      &switchable_interp_encodings[mbmi->interp_filter]);
      ++cpi->interp_filter_selected[0][mbmi->interp_filter];
    } else {
      assert(mbmi->interp_filter == cm->interp_filter);
    }

    if (bsize < BLOCK_8X8) {
      const int num_4x4_w = num_4x4_blocks_wide_lookup[bsize];
      const int num_4x4_h = num_4x4_blocks_high_lookup[bsize];
      int idx, idy;
      for (idy = 0; idy < 2; idy += num_4x4_h) {
        for (idx = 0; idx < 2; idx += num_4x4_w) {
          const int j = idy * 2 + idx;
          const PREDICTION_MODE b_mode = mi->bmi[j].as_mode;
          write_inter_mode(w, b_mode, inter_probs);
          if (b_mode == NEWMV) {
            for (ref = 0; ref < 1 + is_compound; ++ref)
              vp10_encode_mv(cpi, w, &mi->bmi[j].as_mv[ref].as_mv,
                            &mbmi_ext->ref_mvs[mbmi->ref_frame[ref]][0].as_mv,
                            nmvc, allow_hp);
          }
        }
      }
    } else {
      if (mode == NEWMV) {
        for (ref = 0; ref < 1 + is_compound; ++ref)
          vp10_encode_mv(cpi, w, &mbmi->mv[ref].as_mv,
                        &mbmi_ext->ref_mvs[mbmi->ref_frame[ref]][0].as_mv, nmvc,
                        allow_hp);
      }
    }
  }
  if (mbmi->tx_size < TX_32X32 &&
      cm->base_qindex > 0 && !mbmi->skip &&
      !segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP)) {
    if (is_inter) {
      vp10_write_token(
          w, vp10_ext_tx_tree,
          cm->fc->inter_ext_tx_prob[mbmi->tx_size],
          &ext_tx_encodings[mbmi->tx_type]);
    } else {
      vp10_write_token(
          w, vp10_ext_tx_tree,
          cm->fc->intra_ext_tx_prob[mbmi->tx_size]
                                   [intra_mode_to_tx_type_context[mbmi->mode]],
          &ext_tx_encodings[mbmi->tx_type]);
    }
  } else {
    if (!mbmi->skip)
      assert(mbmi->tx_type == DCT_DCT);
  }
}

static void write_mb_modes_kf(const VP10_COMMON *cm, const MACROBLOCKD *xd,
                              MODE_INFO **mi_8x8, vpx_writer *w) {
  const struct segmentation *const seg = &cm->seg;
#if CONFIG_MISC_FIXES
  const struct segmentation_probs *const segp = &cm->fc->seg;
#else
  const struct segmentation_probs *const segp = &cm->segp;
#endif
  const MODE_INFO *const mi = mi_8x8[0];
  const MODE_INFO *const above_mi = xd->above_mi;
  const MODE_INFO *const left_mi = xd->left_mi;
  const MB_MODE_INFO *const mbmi = &mi->mbmi;
  const BLOCK_SIZE bsize = mbmi->sb_type;

  if (seg->update_map)
    write_segment_id(w, seg, segp, mbmi->segment_id);

  write_skip(cm, xd, mbmi->segment_id, mi, w);

  if (bsize >= BLOCK_8X8 && cm->tx_mode == TX_MODE_SELECT &&
      !xd->lossless[mbmi->segment_id])
    write_selected_tx_size(cm, xd, w);

  if (bsize >= BLOCK_8X8) {
    write_intra_mode(w, mbmi->mode,
                     get_y_mode_probs(cm, mi, above_mi, left_mi, 0));
  } else {
    const int num_4x4_w = num_4x4_blocks_wide_lookup[bsize];
    const int num_4x4_h = num_4x4_blocks_high_lookup[bsize];
    int idx, idy;

    for (idy = 0; idy < 2; idy += num_4x4_h) {
      for (idx = 0; idx < 2; idx += num_4x4_w) {
        const int block = idy * 2 + idx;
        write_intra_mode(w, mi->bmi[block].as_mode,
                         get_y_mode_probs(cm, mi, above_mi, left_mi, block));
      }
    }
  }

  write_intra_mode(w, mbmi->uv_mode, cm->fc->uv_mode_prob[mbmi->mode]);

  if (mbmi->tx_size < TX_32X32 &&
      cm->base_qindex > 0 && !mbmi->skip &&
      !segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP)) {
    vp10_write_token(
        w, vp10_ext_tx_tree,
        cm->fc->intra_ext_tx_prob[mbmi->tx_size]
                                 [intra_mode_to_tx_type_context[mbmi->mode]],
        &ext_tx_encodings[mbmi->tx_type]);
  }
}

static void write_modes_b(VP10_COMP *cpi, const TileInfo *const tile,
                          vpx_writer *w, TOKENEXTRA **tok,
                          const TOKENEXTRA *const tok_end,
                          int mi_row, int mi_col) {
  const VP10_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &cpi->td.mb.e_mbd;
  MODE_INFO *m;
  int plane;

  xd->mi = cm->mi_grid_visible + (mi_row * cm->mi_stride + mi_col);
  m = xd->mi[0];

  cpi->td.mb.mbmi_ext = cpi->mbmi_ext_base + (mi_row * cm->mi_cols + mi_col);

  set_mi_row_col(xd, tile,
                 mi_row, num_8x8_blocks_high_lookup[m->mbmi.sb_type],
                 mi_col, num_8x8_blocks_wide_lookup[m->mbmi.sb_type],
                 cm->mi_rows, cm->mi_cols);
  if (frame_is_intra_only(cm)) {
    write_mb_modes_kf(cm, xd, xd->mi, w);
  } else {
    pack_inter_mode_mvs(cpi, m, w);
  }

  if (!m->mbmi.skip) {
    assert(*tok < tok_end);
    for (plane = 0; plane < MAX_MB_PLANE; ++plane) {
      TX_SIZE tx = plane ? get_uv_tx_size(&m->mbmi, &xd->plane[plane])
                         : m->mbmi.tx_size;
      pack_mb_tokens(w, tok, tok_end, cm->bit_depth, tx);
      assert(*tok < tok_end && (*tok)->token == EOSB_TOKEN);
      (*tok)++;
    }
  }
}

static void write_partition(const VP10_COMMON *const cm,
                            const MACROBLOCKD *const xd,
                            int hbs, int mi_row, int mi_col,
                            PARTITION_TYPE p, BLOCK_SIZE bsize, vpx_writer *w) {
  const int ctx = partition_plane_context(xd, mi_row, mi_col, bsize);
  const vpx_prob *const probs = cm->fc->partition_prob[ctx];
  const int has_rows = (mi_row + hbs) < cm->mi_rows;
  const int has_cols = (mi_col + hbs) < cm->mi_cols;

  if (has_rows && has_cols) {
    vp10_write_token(w, vp10_partition_tree, probs, &partition_encodings[p]);
  } else if (!has_rows && has_cols) {
    assert(p == PARTITION_SPLIT || p == PARTITION_HORZ);
    vpx_write(w, p == PARTITION_SPLIT, probs[1]);
  } else if (has_rows && !has_cols) {
    assert(p == PARTITION_SPLIT || p == PARTITION_VERT);
    vpx_write(w, p == PARTITION_SPLIT, probs[2]);
  } else {
    assert(p == PARTITION_SPLIT);
  }
}

static void write_modes_sb(VP10_COMP *cpi,
                           const TileInfo *const tile, vpx_writer *w,
                           TOKENEXTRA **tok, const TOKENEXTRA *const tok_end,
                           int mi_row, int mi_col, BLOCK_SIZE bsize) {
  const VP10_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &cpi->td.mb.e_mbd;

  const int bsl = b_width_log2_lookup[bsize];
  const int bs = (1 << bsl) / 4;
  PARTITION_TYPE partition;
  BLOCK_SIZE subsize;
  const MODE_INFO *m = NULL;

  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols)
    return;

  m = cm->mi_grid_visible[mi_row * cm->mi_stride + mi_col];

  partition = partition_lookup[bsl][m->mbmi.sb_type];
  write_partition(cm, xd, bs, mi_row, mi_col, partition, bsize, w);
  subsize = get_subsize(bsize, partition);
  if (subsize < BLOCK_8X8) {
    write_modes_b(cpi, tile, w, tok, tok_end, mi_row, mi_col);
  } else {
    switch (partition) {
      case PARTITION_NONE:
        write_modes_b(cpi, tile, w, tok, tok_end, mi_row, mi_col);
        break;
      case PARTITION_HORZ:
        write_modes_b(cpi, tile, w, tok, tok_end, mi_row, mi_col);
        if (mi_row + bs < cm->mi_rows)
          write_modes_b(cpi, tile, w, tok, tok_end, mi_row + bs, mi_col);
        break;
      case PARTITION_VERT:
        write_modes_b(cpi, tile, w, tok, tok_end, mi_row, mi_col);
        if (mi_col + bs < cm->mi_cols)
          write_modes_b(cpi, tile, w, tok, tok_end, mi_row, mi_col + bs);
        break;
      case PARTITION_SPLIT:
        write_modes_sb(cpi, tile, w, tok, tok_end, mi_row, mi_col, subsize);
        write_modes_sb(cpi, tile, w, tok, tok_end, mi_row, mi_col + bs,
                       subsize);
        write_modes_sb(cpi, tile, w, tok, tok_end, mi_row + bs, mi_col,
                       subsize);
        write_modes_sb(cpi, tile, w, tok, tok_end, mi_row + bs, mi_col + bs,
                       subsize);
        break;
      default:
        assert(0);
    }
  }

  // update partition context
  if (bsize >= BLOCK_8X8 &&
      (bsize == BLOCK_8X8 || partition != PARTITION_SPLIT))
    update_partition_context(xd, mi_row, mi_col, subsize, bsize);
}

static void write_modes(VP10_COMP *cpi,
                        const TileInfo *const tile, vpx_writer *w,
                        TOKENEXTRA **tok, const TOKENEXTRA *const tok_end) {
  MACROBLOCKD *const xd = &cpi->td.mb.e_mbd;
  int mi_row, mi_col;

  for (mi_row = tile->mi_row_start; mi_row < tile->mi_row_end;
       mi_row += MI_BLOCK_SIZE) {
    vp10_zero(xd->left_seg_context);
    for (mi_col = tile->mi_col_start; mi_col < tile->mi_col_end;
         mi_col += MI_BLOCK_SIZE)
      write_modes_sb(cpi, tile, w, tok, tok_end, mi_row, mi_col,
                     BLOCK_64X64);
  }
}

static void build_tree_distribution(VP10_COMP *cpi, TX_SIZE tx_size,
                                    vp10_coeff_stats *coef_branch_ct,
                                    vp10_coeff_probs_model *coef_probs) {
  vp10_coeff_count *coef_counts = cpi->td.rd_counts.coef_counts[tx_size];
  unsigned int (*eob_branch_ct)[REF_TYPES][COEF_BANDS][COEFF_CONTEXTS] =
      cpi->common.counts.eob_branch[tx_size];
  int i, j, k, l, m;

  for (i = 0; i < PLANE_TYPES; ++i) {
    for (j = 0; j < REF_TYPES; ++j) {
      for (k = 0; k < COEF_BANDS; ++k) {
        for (l = 0; l < BAND_COEFF_CONTEXTS(k); ++l) {
          vp10_tree_probs_from_distribution(vp10_coef_tree,
                                           coef_branch_ct[i][j][k][l],
                                           coef_counts[i][j][k][l]);
          coef_branch_ct[i][j][k][l][0][1] = eob_branch_ct[i][j][k][l] -
                                             coef_branch_ct[i][j][k][l][0][0];
          for (m = 0; m < UNCONSTRAINED_NODES; ++m)
            coef_probs[i][j][k][l][m] = get_binary_prob(
                                            coef_branch_ct[i][j][k][l][m][0],
                                            coef_branch_ct[i][j][k][l][m][1]);
        }
      }
    }
  }
}

static void update_coef_probs_common(vpx_writer* const bc, VP10_COMP *cpi,
                                     TX_SIZE tx_size,
                                     vp10_coeff_stats *frame_branch_ct,
                                     vp10_coeff_probs_model *new_coef_probs) {
  vp10_coeff_probs_model *old_coef_probs = cpi->common.fc->coef_probs[tx_size];
  const vpx_prob upd = DIFF_UPDATE_PROB;
  const int entropy_nodes_update = UNCONSTRAINED_NODES;
  int i, j, k, l, t;
  int stepsize = cpi->sf.coeff_prob_appx_step;

  switch (cpi->sf.use_fast_coef_updates) {
    case TWO_LOOP: {
      /* dry run to see if there is any update at all needed */
      int savings = 0;
      int update[2] = {0, 0};
      for (i = 0; i < PLANE_TYPES; ++i) {
        for (j = 0; j < REF_TYPES; ++j) {
          for (k = 0; k < COEF_BANDS; ++k) {
            for (l = 0; l < BAND_COEFF_CONTEXTS(k); ++l) {
              for (t = 0; t < entropy_nodes_update; ++t) {
                vpx_prob newp = new_coef_probs[i][j][k][l][t];
                const vpx_prob oldp = old_coef_probs[i][j][k][l][t];
                int s;
                int u = 0;
                if (t == PIVOT_NODE)
                  s = vp10_prob_diff_update_savings_search_model(
                      frame_branch_ct[i][j][k][l][0],
                      old_coef_probs[i][j][k][l], &newp, upd, stepsize);
                else
                  s = vp10_prob_diff_update_savings_search(
                      frame_branch_ct[i][j][k][l][t], oldp, &newp, upd);
                if (s > 0 && newp != oldp)
                  u = 1;
                if (u)
                  savings += s - (int)(vp10_cost_zero(upd));
                else
                  savings -= (int)(vp10_cost_zero(upd));
                update[u]++;
              }
            }
          }
        }
      }

      // printf("Update %d %d, savings %d\n", update[0], update[1], savings);
      /* Is coef updated at all */
      if (update[1] == 0 || savings < 0) {
        vpx_write_bit(bc, 0);
        return;
      }
      vpx_write_bit(bc, 1);
      for (i = 0; i < PLANE_TYPES; ++i) {
        for (j = 0; j < REF_TYPES; ++j) {
          for (k = 0; k < COEF_BANDS; ++k) {
            for (l = 0; l < BAND_COEFF_CONTEXTS(k); ++l) {
              // calc probs and branch cts for this frame only
              for (t = 0; t < entropy_nodes_update; ++t) {
                vpx_prob newp = new_coef_probs[i][j][k][l][t];
                vpx_prob *oldp = old_coef_probs[i][j][k][l] + t;
                const vpx_prob upd = DIFF_UPDATE_PROB;
                int s;
                int u = 0;
                if (t == PIVOT_NODE)
                  s = vp10_prob_diff_update_savings_search_model(
                      frame_branch_ct[i][j][k][l][0],
                      old_coef_probs[i][j][k][l], &newp, upd, stepsize);
                else
                  s = vp10_prob_diff_update_savings_search(
                      frame_branch_ct[i][j][k][l][t],
                      *oldp, &newp, upd);
                if (s > 0 && newp != *oldp)
                  u = 1;
                vpx_write(bc, u, upd);
                if (u) {
                  /* send/use new probability */
                  vp10_write_prob_diff_update(bc, newp, *oldp);
                  *oldp = newp;
                }
              }
            }
          }
        }
      }
      return;
    }

    case ONE_LOOP_REDUCED: {
      int updates = 0;
      int noupdates_before_first = 0;
      for (i = 0; i < PLANE_TYPES; ++i) {
        for (j = 0; j < REF_TYPES; ++j) {
          for (k = 0; k < COEF_BANDS; ++k) {
            for (l = 0; l < BAND_COEFF_CONTEXTS(k); ++l) {
              // calc probs and branch cts for this frame only
              for (t = 0; t < entropy_nodes_update; ++t) {
                vpx_prob newp = new_coef_probs[i][j][k][l][t];
                vpx_prob *oldp = old_coef_probs[i][j][k][l] + t;
                int s;
                int u = 0;

                if (t == PIVOT_NODE) {
                  s = vp10_prob_diff_update_savings_search_model(
                      frame_branch_ct[i][j][k][l][0],
                      old_coef_probs[i][j][k][l], &newp, upd, stepsize);
                } else {
                  s = vp10_prob_diff_update_savings_search(
                      frame_branch_ct[i][j][k][l][t],
                      *oldp, &newp, upd);
                }

                if (s > 0 && newp != *oldp)
                  u = 1;
                updates += u;
                if (u == 0 && updates == 0) {
                  noupdates_before_first++;
                  continue;
                }
                if (u == 1 && updates == 1) {
                  int v;
                  // first update
                  vpx_write_bit(bc, 1);
                  for (v = 0; v < noupdates_before_first; ++v)
                    vpx_write(bc, 0, upd);
                }
                vpx_write(bc, u, upd);
                if (u) {
                  /* send/use new probability */
                  vp10_write_prob_diff_update(bc, newp, *oldp);
                  *oldp = newp;
                }
              }
            }
          }
        }
      }
      if (updates == 0) {
        vpx_write_bit(bc, 0);  // no updates
      }
      return;
    }
    default:
      assert(0);
  }
}

static void update_coef_probs(VP10_COMP *cpi, vpx_writer* w) {
  const TX_MODE tx_mode = cpi->common.tx_mode;
  const TX_SIZE max_tx_size = tx_mode_to_biggest_tx_size[tx_mode];
  TX_SIZE tx_size;
  for (tx_size = TX_4X4; tx_size <= max_tx_size; ++tx_size) {
    vp10_coeff_stats frame_branch_ct[PLANE_TYPES];
    vp10_coeff_probs_model frame_coef_probs[PLANE_TYPES];
    if (cpi->td.counts->tx.tx_totals[tx_size] <= 20 ||
        (tx_size >= TX_16X16 && cpi->sf.tx_size_search_method == USE_TX_8X8)) {
      vpx_write_bit(w, 0);
    } else {
      build_tree_distribution(cpi, tx_size, frame_branch_ct,
                              frame_coef_probs);
      update_coef_probs_common(w, cpi, tx_size, frame_branch_ct,
                               frame_coef_probs);
    }
  }
}

static void encode_loopfilter(struct loopfilter *lf,
                              struct vpx_write_bit_buffer *wb) {
  int i;

  // Encode the loop filter level and type
  vpx_wb_write_literal(wb, lf->filter_level, 6);
  vpx_wb_write_literal(wb, lf->sharpness_level, 3);

  // Write out loop filter deltas applied at the MB level based on mode or
  // ref frame (if they are enabled).
  vpx_wb_write_bit(wb, lf->mode_ref_delta_enabled);

  if (lf->mode_ref_delta_enabled) {
    vpx_wb_write_bit(wb, lf->mode_ref_delta_update);
    if (lf->mode_ref_delta_update) {
      for (i = 0; i < MAX_REF_FRAMES; i++) {
        const int delta = lf->ref_deltas[i];
        const int changed = delta != lf->last_ref_deltas[i];
        vpx_wb_write_bit(wb, changed);
        if (changed) {
          lf->last_ref_deltas[i] = delta;
          vpx_wb_write_inv_signed_literal(wb, delta, 6);
        }
      }

      for (i = 0; i < MAX_MODE_LF_DELTAS; i++) {
        const int delta = lf->mode_deltas[i];
        const int changed = delta != lf->last_mode_deltas[i];
        vpx_wb_write_bit(wb, changed);
        if (changed) {
          lf->last_mode_deltas[i] = delta;
          vpx_wb_write_inv_signed_literal(wb, delta, 6);
        }
      }
    }
  }
}

static void write_delta_q(struct vpx_write_bit_buffer *wb, int delta_q) {
  if (delta_q != 0) {
    vpx_wb_write_bit(wb, 1);
    vpx_wb_write_inv_signed_literal(wb, delta_q, CONFIG_MISC_FIXES ? 6 : 4);
  } else {
    vpx_wb_write_bit(wb, 0);
  }
}

static void encode_quantization(const VP10_COMMON *const cm,
                                struct vpx_write_bit_buffer *wb) {
  vpx_wb_write_literal(wb, cm->base_qindex, QINDEX_BITS);
  write_delta_q(wb, cm->y_dc_delta_q);
  write_delta_q(wb, cm->uv_dc_delta_q);
  write_delta_q(wb, cm->uv_ac_delta_q);
}

static void encode_segmentation(VP10_COMMON *cm, MACROBLOCKD *xd,
                                struct vpx_write_bit_buffer *wb) {
  int i, j;

  const struct segmentation *seg = &cm->seg;
#if !CONFIG_MISC_FIXES
  const struct segmentation_probs *segp = &cm->segp;
#endif

  vpx_wb_write_bit(wb, seg->enabled);
  if (!seg->enabled)
    return;

  // Segmentation map
  if (!frame_is_intra_only(cm) && !cm->error_resilient_mode) {
    vpx_wb_write_bit(wb, seg->update_map);
  } else {
    assert(seg->update_map == 1);
  }
  if (seg->update_map) {
    // Select the coding strategy (temporal or spatial)
    vp10_choose_segmap_coding_method(cm, xd);
#if !CONFIG_MISC_FIXES
    // Write out probabilities used to decode unpredicted  macro-block segments
    for (i = 0; i < SEG_TREE_PROBS; i++) {
      const int prob = segp->tree_probs[i];
      const int update = prob != MAX_PROB;
      vpx_wb_write_bit(wb, update);
      if (update)
        vpx_wb_write_literal(wb, prob, 8);
    }
#endif

    // Write out the chosen coding method.
    if (!frame_is_intra_only(cm) && !cm->error_resilient_mode) {
      vpx_wb_write_bit(wb, seg->temporal_update);
    } else {
      assert(seg->temporal_update == 0);
    }

#if !CONFIG_MISC_FIXES
    if (seg->temporal_update) {
      for (i = 0; i < PREDICTION_PROBS; i++) {
        const int prob = segp->pred_probs[i];
        const int update = prob != MAX_PROB;
        vpx_wb_write_bit(wb, update);
        if (update)
          vpx_wb_write_literal(wb, prob, 8);
      }
    }
#endif
  }

  // Segmentation data
  vpx_wb_write_bit(wb, seg->update_data);
  if (seg->update_data) {
    vpx_wb_write_bit(wb, seg->abs_delta);

    for (i = 0; i < MAX_SEGMENTS; i++) {
      for (j = 0; j < SEG_LVL_MAX; j++) {
        const int active = segfeature_active(seg, i, j);
        vpx_wb_write_bit(wb, active);
        if (active) {
          const int data = get_segdata(seg, i, j);
          const int data_max = vp10_seg_feature_data_max(j);

          if (vp10_is_segfeature_signed(j)) {
            encode_unsigned_max(wb, abs(data), data_max);
            vpx_wb_write_bit(wb, data < 0);
          } else {
            encode_unsigned_max(wb, data, data_max);
          }
        }
      }
    }
  }
}

#if CONFIG_MISC_FIXES
static void update_seg_probs(VP10_COMP *cpi, vpx_writer *w) {
  VP10_COMMON *cm = &cpi->common;

  if (!cpi->common.seg.enabled)
    return;

  if (cpi->common.seg.temporal_update) {
    int i;

    for (i = 0; i < PREDICTION_PROBS; i++)
      vp10_cond_prob_diff_update(w, &cm->fc->seg.pred_probs[i],
          cm->counts.seg.pred[i]);

    prob_diff_update(vp10_segment_tree, cm->fc->seg.tree_probs,
        cm->counts.seg.tree_mispred, MAX_SEGMENTS, w);
  } else {
    prob_diff_update(vp10_segment_tree, cm->fc->seg.tree_probs,
        cm->counts.seg.tree_total, MAX_SEGMENTS, w);
  }
}

static void write_txfm_mode(TX_MODE mode, struct vpx_write_bit_buffer *wb) {
  vpx_wb_write_bit(wb, mode == TX_MODE_SELECT);
  if (mode != TX_MODE_SELECT)
    vpx_wb_write_literal(wb, mode, 2);
}
#else
static void write_txfm_mode(TX_MODE mode, struct vpx_writer *wb) {
  vpx_write_literal(wb, VPXMIN(mode, ALLOW_32X32), 2);
  if (mode >= ALLOW_32X32)
    vpx_write_bit(wb, mode == TX_MODE_SELECT);
}
#endif


static void update_txfm_probs(VP10_COMMON *cm, vpx_writer *w,
                              FRAME_COUNTS *counts) {

  if (cm->tx_mode == TX_MODE_SELECT) {
    int i, j;
    unsigned int ct_8x8p[TX_SIZES - 3][2];
    unsigned int ct_16x16p[TX_SIZES - 2][2];
    unsigned int ct_32x32p[TX_SIZES - 1][2];


    for (i = 0; i < TX_SIZE_CONTEXTS; i++) {
      vp10_tx_counts_to_branch_counts_8x8(counts->tx.p8x8[i], ct_8x8p);
      for (j = 0; j < TX_SIZES - 3; j++)
        vp10_cond_prob_diff_update(w, &cm->fc->tx_probs.p8x8[i][j], ct_8x8p[j]);
    }

    for (i = 0; i < TX_SIZE_CONTEXTS; i++) {
      vp10_tx_counts_to_branch_counts_16x16(counts->tx.p16x16[i], ct_16x16p);
      for (j = 0; j < TX_SIZES - 2; j++)
        vp10_cond_prob_diff_update(w, &cm->fc->tx_probs.p16x16[i][j],
                                  ct_16x16p[j]);
    }

    for (i = 0; i < TX_SIZE_CONTEXTS; i++) {
      vp10_tx_counts_to_branch_counts_32x32(counts->tx.p32x32[i], ct_32x32p);
      for (j = 0; j < TX_SIZES - 1; j++)
        vp10_cond_prob_diff_update(w, &cm->fc->tx_probs.p32x32[i][j],
                                  ct_32x32p[j]);
    }
  }
}

static void write_interp_filter(INTERP_FILTER filter,
                                struct vpx_write_bit_buffer *wb) {
  vpx_wb_write_bit(wb, filter == SWITCHABLE);
  if (filter != SWITCHABLE)
    vpx_wb_write_literal(wb, filter, 2);
}

static void fix_interp_filter(VP10_COMMON *cm, FRAME_COUNTS *counts) {
  if (cm->interp_filter == SWITCHABLE) {
    // Check to see if only one of the filters is actually used
    int count[SWITCHABLE_FILTERS];
    int i, j, c = 0;
    for (i = 0; i < SWITCHABLE_FILTERS; ++i) {
      count[i] = 0;
      for (j = 0; j < SWITCHABLE_FILTER_CONTEXTS; ++j)
        count[i] += counts->switchable_interp[j][i];
      c += (count[i] > 0);
    }
    if (c == 1) {
      // Only one filter is used. So set the filter at frame level
      for (i = 0; i < SWITCHABLE_FILTERS; ++i) {
        if (count[i]) {
          cm->interp_filter = i;
          break;
        }
      }
    }
  }
}

static void write_tile_info(const VP10_COMMON *const cm,
                            struct vpx_write_bit_buffer *wb) {
  int min_log2_tile_cols, max_log2_tile_cols, ones;
  vp10_get_tile_n_bits(cm->mi_cols, &min_log2_tile_cols, &max_log2_tile_cols);

  // columns
  ones = cm->log2_tile_cols - min_log2_tile_cols;
  while (ones--)
    vpx_wb_write_bit(wb, 1);

  if (cm->log2_tile_cols < max_log2_tile_cols)
    vpx_wb_write_bit(wb, 0);

  // rows
  vpx_wb_write_bit(wb, cm->log2_tile_rows != 0);
  if (cm->log2_tile_rows != 0)
    vpx_wb_write_bit(wb, cm->log2_tile_rows != 1);
}

static int get_refresh_mask(VP10_COMP *cpi) {
  if (vp10_preserve_existing_gf(cpi)) {
    // We have decided to preserve the previously existing golden frame as our
    // new ARF frame. However, in the short term we leave it in the GF slot and,
    // if we're updating the GF with the current decoded frame, we save it
    // instead to the ARF slot.
    // Later, in the function vp10_encoder.c:vp10_update_reference_frames() we
    // will swap gld_fb_idx and alt_fb_idx to achieve our objective. We do it
    // there so that it can be done outside of the recode loop.
    // Note: This is highly specific to the use of ARF as a forward reference,
    // and this needs to be generalized as other uses are implemented
    // (like RTC/temporal scalability).
    return (cpi->refresh_last_frame << cpi->lst_fb_idx) |
           (cpi->refresh_golden_frame << cpi->alt_fb_idx);
  } else {
    int arf_idx = cpi->alt_fb_idx;
    if ((cpi->oxcf.pass == 2) && cpi->multi_arf_allowed) {
      const GF_GROUP *const gf_group = &cpi->twopass.gf_group;
      arf_idx = gf_group->arf_update_idx[gf_group->index];
    }
    return (cpi->refresh_last_frame << cpi->lst_fb_idx) |
           (cpi->refresh_golden_frame << cpi->gld_fb_idx) |
           (cpi->refresh_alt_ref_frame << arf_idx);
  }
}

static size_t encode_tiles(VP10_COMP *cpi, uint8_t *data_ptr,
                           unsigned int *max_tile_sz) {
  VP10_COMMON *const cm = &cpi->common;
  vpx_writer residual_bc;
  int tile_row, tile_col;
  TOKENEXTRA *tok_end;
  size_t total_size = 0;
  const int tile_cols = 1 << cm->log2_tile_cols;
  const int tile_rows = 1 << cm->log2_tile_rows;
  unsigned int max_tile = 0;

  memset(cm->above_seg_context, 0,
         sizeof(*cm->above_seg_context) * mi_cols_aligned_to_sb(cm->mi_cols));

  for (tile_row = 0; tile_row < tile_rows; tile_row++) {
    for (tile_col = 0; tile_col < tile_cols; tile_col++) {
      int tile_idx = tile_row * tile_cols + tile_col;
      TOKENEXTRA *tok = cpi->tile_tok[tile_row][tile_col];

      tok_end = cpi->tile_tok[tile_row][tile_col] +
          cpi->tok_count[tile_row][tile_col];

      if (tile_col < tile_cols - 1 || tile_row < tile_rows - 1)
        vpx_start_encode(&residual_bc, data_ptr + total_size + 4);
      else
        vpx_start_encode(&residual_bc, data_ptr + total_size);

      write_modes(cpi, &cpi->tile_data[tile_idx].tile_info,
                  &residual_bc, &tok, tok_end);
      assert(tok == tok_end);
      vpx_stop_encode(&residual_bc);
      if (tile_col < tile_cols - 1 || tile_row < tile_rows - 1) {
        unsigned int tile_sz;

        // size of this tile
        assert(residual_bc.pos > 0);
        tile_sz = residual_bc.pos - CONFIG_MISC_FIXES;
        mem_put_le32(data_ptr + total_size, tile_sz);
        max_tile = max_tile > tile_sz ? max_tile : tile_sz;
        total_size += 4;
      }

      total_size += residual_bc.pos;
    }
  }
  *max_tile_sz = max_tile;

  return total_size;
}

static void write_render_size(const VP10_COMMON *cm,
                              struct vpx_write_bit_buffer *wb) {
  const int scaling_active = cm->width != cm->render_width ||
                             cm->height != cm->render_height;
  vpx_wb_write_bit(wb, scaling_active);
  if (scaling_active) {
    vpx_wb_write_literal(wb, cm->render_width - 1, 16);
    vpx_wb_write_literal(wb, cm->render_height - 1, 16);
  }
}

static void write_frame_size(const VP10_COMMON *cm,
                             struct vpx_write_bit_buffer *wb) {
  vpx_wb_write_literal(wb, cm->width - 1, 16);
  vpx_wb_write_literal(wb, cm->height - 1, 16);

  write_render_size(cm, wb);
}

static void write_frame_size_with_refs(VP10_COMP *cpi,
                                       struct vpx_write_bit_buffer *wb) {
  VP10_COMMON *const cm = &cpi->common;
  int found = 0;

  MV_REFERENCE_FRAME ref_frame;
  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    YV12_BUFFER_CONFIG *cfg = get_ref_frame_buffer(cpi, ref_frame);

    if (cfg != NULL) {
      found = cm->width == cfg->y_crop_width &&
              cm->height == cfg->y_crop_height;
#if CONFIG_MISC_FIXES
      found &= cm->render_width == cfg->render_width &&
               cm->render_height == cfg->render_height;
#endif
    }
    vpx_wb_write_bit(wb, found);
    if (found) {
      break;
    }
  }

  if (!found) {
    vpx_wb_write_literal(wb, cm->width - 1, 16);
    vpx_wb_write_literal(wb, cm->height - 1, 16);

#if CONFIG_MISC_FIXES
    write_render_size(cm, wb);
#endif
  }

#if !CONFIG_MISC_FIXES
  write_render_size(cm, wb);
#endif
}

static void write_sync_code(struct vpx_write_bit_buffer *wb) {
  vpx_wb_write_literal(wb, VP10_SYNC_CODE_0, 8);
  vpx_wb_write_literal(wb, VP10_SYNC_CODE_1, 8);
  vpx_wb_write_literal(wb, VP10_SYNC_CODE_2, 8);
}

static void write_profile(BITSTREAM_PROFILE profile,
                          struct vpx_write_bit_buffer *wb) {
  switch (profile) {
    case PROFILE_0:
      vpx_wb_write_literal(wb, 0, 2);
      break;
    case PROFILE_1:
      vpx_wb_write_literal(wb, 2, 2);
      break;
    case PROFILE_2:
      vpx_wb_write_literal(wb, 1, 2);
      break;
    case PROFILE_3:
      vpx_wb_write_literal(wb, 6, 3);
      break;
    default:
      assert(0);
  }
}

static void write_bitdepth_colorspace_sampling(
    VP10_COMMON *const cm, struct vpx_write_bit_buffer *wb) {
  if (cm->profile >= PROFILE_2) {
    assert(cm->bit_depth > VPX_BITS_8);
    vpx_wb_write_bit(wb, cm->bit_depth == VPX_BITS_10 ? 0 : 1);
  }
  vpx_wb_write_literal(wb, cm->color_space, 3);
  if (cm->color_space != VPX_CS_SRGB) {
    // 0: [16, 235] (i.e. xvYCC), 1: [0, 255]
    vpx_wb_write_bit(wb, cm->color_range);
    if (cm->profile == PROFILE_1 || cm->profile == PROFILE_3) {
      assert(cm->subsampling_x != 1 || cm->subsampling_y != 1);
      vpx_wb_write_bit(wb, cm->subsampling_x);
      vpx_wb_write_bit(wb, cm->subsampling_y);
      vpx_wb_write_bit(wb, 0);  // unused
    } else {
      assert(cm->subsampling_x == 1 && cm->subsampling_y == 1);
    }
  } else {
    assert(cm->profile == PROFILE_1 || cm->profile == PROFILE_3);
    vpx_wb_write_bit(wb, 0);  // unused
  }
}

static void write_uncompressed_header(VP10_COMP *cpi,
                                      struct vpx_write_bit_buffer *wb) {
  VP10_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &cpi->td.mb.e_mbd;

  vpx_wb_write_literal(wb, VP9_FRAME_MARKER, 2);

  write_profile(cm->profile, wb);

  vpx_wb_write_bit(wb, 0);  // show_existing_frame
  vpx_wb_write_bit(wb, cm->frame_type);
  vpx_wb_write_bit(wb, cm->show_frame);
  vpx_wb_write_bit(wb, cm->error_resilient_mode);

  if (cm->frame_type == KEY_FRAME) {
    write_sync_code(wb);
    write_bitdepth_colorspace_sampling(cm, wb);
    write_frame_size(cm, wb);
  } else {
    if (!cm->show_frame)
      vpx_wb_write_bit(wb, cm->intra_only);

    if (!cm->error_resilient_mode) {
#if CONFIG_MISC_FIXES
      if (cm->intra_only) {
        vpx_wb_write_bit(wb,
                         cm->reset_frame_context == RESET_FRAME_CONTEXT_ALL);
      } else {
        vpx_wb_write_bit(wb,
                         cm->reset_frame_context != RESET_FRAME_CONTEXT_NONE);
        if (cm->reset_frame_context != RESET_FRAME_CONTEXT_NONE)
          vpx_wb_write_bit(wb,
                           cm->reset_frame_context == RESET_FRAME_CONTEXT_ALL);
      }
#else
      static const int reset_frame_context_conv_tbl[3] = { 0, 2, 3 };

      vpx_wb_write_literal(wb,
          reset_frame_context_conv_tbl[cm->reset_frame_context], 2);
#endif
    }

    if (cm->intra_only) {
      write_sync_code(wb);

#if CONFIG_MISC_FIXES
      write_bitdepth_colorspace_sampling(cm, wb);
#else
      // Note for profile 0, 420 8bpp is assumed.
      if (cm->profile > PROFILE_0) {
        write_bitdepth_colorspace_sampling(cm, wb);
      }
#endif

      vpx_wb_write_literal(wb, get_refresh_mask(cpi), REF_FRAMES);
      write_frame_size(cm, wb);
    } else {
      MV_REFERENCE_FRAME ref_frame;
      vpx_wb_write_literal(wb, get_refresh_mask(cpi), REF_FRAMES);
      for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
        assert(get_ref_frame_map_idx(cpi, ref_frame) != INVALID_IDX);
        vpx_wb_write_literal(wb, get_ref_frame_map_idx(cpi, ref_frame),
                             REF_FRAMES_LOG2);
        vpx_wb_write_bit(wb, cm->ref_frame_sign_bias[ref_frame]);
      }

      write_frame_size_with_refs(cpi, wb);

      vpx_wb_write_bit(wb, cm->allow_high_precision_mv);

      fix_interp_filter(cm, cpi->td.counts);
      write_interp_filter(cm->interp_filter, wb);
    }
  }

  if (!cm->error_resilient_mode) {
    vpx_wb_write_bit(wb,
                     cm->refresh_frame_context != REFRESH_FRAME_CONTEXT_OFF);
#if CONFIG_MISC_FIXES
    if (cm->refresh_frame_context != REFRESH_FRAME_CONTEXT_OFF)
#endif
      vpx_wb_write_bit(wb, cm->refresh_frame_context !=
                               REFRESH_FRAME_CONTEXT_BACKWARD);
  }

  vpx_wb_write_literal(wb, cm->frame_context_idx, FRAME_CONTEXTS_LOG2);

  encode_loopfilter(&cm->lf, wb);
  encode_quantization(cm, wb);
  encode_segmentation(cm, xd, wb);
#if CONFIG_MISC_FIXES
  if (!cm->seg.enabled && xd->lossless[0])
    cm->tx_mode = TX_4X4;
  else
    write_txfm_mode(cm->tx_mode, wb);
  if (cpi->allow_comp_inter_inter) {
    const int use_hybrid_pred = cm->reference_mode == REFERENCE_MODE_SELECT;
    const int use_compound_pred = cm->reference_mode != SINGLE_REFERENCE;

    vpx_wb_write_bit(wb, use_hybrid_pred);
    if (!use_hybrid_pred)
      vpx_wb_write_bit(wb, use_compound_pred);
  }
#endif

  write_tile_info(cm, wb);
}

static size_t write_compressed_header(VP10_COMP *cpi, uint8_t *data) {
  VP10_COMMON *const cm = &cpi->common;
  FRAME_CONTEXT *const fc = cm->fc;
  FRAME_COUNTS *counts = cpi->td.counts;
  vpx_writer header_bc;
  int i;
#if CONFIG_MISC_FIXES
  int j;
#endif

  vpx_start_encode(&header_bc, data);

#if !CONFIG_MISC_FIXES
  if (cpi->td.mb.e_mbd.lossless[0]) {
    cm->tx_mode = TX_4X4;
  } else {
    write_txfm_mode(cm->tx_mode, &header_bc);
    update_txfm_probs(cm, &header_bc, counts);
  }
#else
  update_txfm_probs(cm, &header_bc, counts);
#endif
  update_coef_probs(cpi, &header_bc);
  update_skip_probs(cm, &header_bc, counts);
#if CONFIG_MISC_FIXES
  update_seg_probs(cpi, &header_bc);

  for (i = 0; i < INTRA_MODES; ++i)
    prob_diff_update(vp10_intra_mode_tree, fc->uv_mode_prob[i],
                     counts->uv_mode[i], INTRA_MODES, &header_bc);

  for (i = 0; i < PARTITION_CONTEXTS; ++i)
    prob_diff_update(vp10_partition_tree, fc->partition_prob[i],
                     counts->partition[i], PARTITION_TYPES, &header_bc);
#endif

  if (frame_is_intra_only(cm)) {
    vp10_copy(cm->kf_y_prob, vp10_kf_y_mode_prob);
#if CONFIG_MISC_FIXES
    for (i = 0; i < INTRA_MODES; ++i)
      for (j = 0; j < INTRA_MODES; ++j)
        prob_diff_update(vp10_intra_mode_tree, cm->kf_y_prob[i][j],
                         counts->kf_y_mode[i][j], INTRA_MODES, &header_bc);
#endif
  } else {
    for (i = 0; i < INTER_MODE_CONTEXTS; ++i)
      prob_diff_update(vp10_inter_mode_tree, cm->fc->inter_mode_probs[i],
                       counts->inter_mode[i], INTER_MODES, &header_bc);

    if (cm->interp_filter == SWITCHABLE)
      update_switchable_interp_probs(cm, &header_bc, counts);

    for (i = 0; i < INTRA_INTER_CONTEXTS; i++)
      vp10_cond_prob_diff_update(&header_bc, &fc->intra_inter_prob[i],
                                counts->intra_inter[i]);

    if (cpi->allow_comp_inter_inter) {
      const int use_hybrid_pred = cm->reference_mode == REFERENCE_MODE_SELECT;
#if !CONFIG_MISC_FIXES
      const int use_compound_pred = cm->reference_mode != SINGLE_REFERENCE;

      vpx_write_bit(&header_bc, use_compound_pred);
      if (use_compound_pred) {
        vpx_write_bit(&header_bc, use_hybrid_pred);
        if (use_hybrid_pred)
          for (i = 0; i < COMP_INTER_CONTEXTS; i++)
            vp10_cond_prob_diff_update(&header_bc, &fc->comp_inter_prob[i],
                                      counts->comp_inter[i]);
      }
#else
      if (use_hybrid_pred)
        for (i = 0; i < COMP_INTER_CONTEXTS; i++)
          vp10_cond_prob_diff_update(&header_bc, &fc->comp_inter_prob[i],
                                     counts->comp_inter[i]);
#endif
    }

    if (cm->reference_mode != COMPOUND_REFERENCE) {
      for (i = 0; i < REF_CONTEXTS; i++) {
        vp10_cond_prob_diff_update(&header_bc, &fc->single_ref_prob[i][0],
                                  counts->single_ref[i][0]);
        vp10_cond_prob_diff_update(&header_bc, &fc->single_ref_prob[i][1],
                                  counts->single_ref[i][1]);
      }
    }

    if (cm->reference_mode != SINGLE_REFERENCE)
      for (i = 0; i < REF_CONTEXTS; i++)
        vp10_cond_prob_diff_update(&header_bc, &fc->comp_ref_prob[i],
                                  counts->comp_ref[i]);

    for (i = 0; i < BLOCK_SIZE_GROUPS; ++i)
      prob_diff_update(vp10_intra_mode_tree, cm->fc->y_mode_prob[i],
                       counts->y_mode[i], INTRA_MODES, &header_bc);

#if !CONFIG_MISC_FIXES
    for (i = 0; i < PARTITION_CONTEXTS; ++i)
      prob_diff_update(vp10_partition_tree, fc->partition_prob[i],
                       counts->partition[i], PARTITION_TYPES, &header_bc);
#endif

    vp10_write_nmv_probs(cm, cm->allow_high_precision_mv, &header_bc,
                        &counts->mv);
    update_ext_tx_probs(cm, &header_bc);
  }

  vpx_stop_encode(&header_bc);
  assert(header_bc.pos <= 0xffff);

  return header_bc.pos;
}

#if CONFIG_MISC_FIXES
static int remux_tiles(uint8_t *dest, const int sz,
                       const int n_tiles, const int mag) {
  int rpos = 0, wpos = 0, n;

  for (n = 0; n < n_tiles; n++) {
    int tile_sz;

    if (n == n_tiles - 1) {
      tile_sz = sz - rpos;
    } else {
      tile_sz = mem_get_le32(&dest[rpos]) + 1;
      rpos += 4;
      switch (mag) {
        case 0:
          dest[wpos] = tile_sz - 1;
          break;
        case 1:
          mem_put_le16(&dest[wpos], tile_sz - 1);
          break;
        case 2:
          mem_put_le24(&dest[wpos], tile_sz - 1);
          break;
        case 3:  // remuxing should only happen if mag < 3
        default:
          assert("Invalid value for tile size magnitude" && 0);
      }
      wpos += mag + 1;
    }

    memmove(&dest[wpos], &dest[rpos], tile_sz);
    wpos += tile_sz;
    rpos += tile_sz;
  }

  assert(rpos > wpos);
  assert(rpos == sz);

  return wpos;
}
#endif

void vp10_pack_bitstream(VP10_COMP *const cpi, uint8_t *dest, size_t *size) {
  uint8_t *data = dest;
  size_t first_part_size, uncompressed_hdr_size, data_sz;
  struct vpx_write_bit_buffer wb = {data, 0};
  struct vpx_write_bit_buffer saved_wb;
  unsigned int max_tile;
#if CONFIG_MISC_FIXES
  VP10_COMMON *const cm = &cpi->common;
  const int n_log2_tiles = cm->log2_tile_rows + cm->log2_tile_cols;
  const int have_tiles = n_log2_tiles > 0;
#else
  const int have_tiles = 0;  // we have tiles, but we don't want to write a
                             // tile size marker in the header
#endif

  write_uncompressed_header(cpi, &wb);
  saved_wb = wb;
  // don't know in advance first part. size
  vpx_wb_write_literal(&wb, 0, 16 + have_tiles * 2);

  uncompressed_hdr_size = vpx_wb_bytes_written(&wb);
  data += uncompressed_hdr_size;

  vpx_clear_system_state();

  first_part_size = write_compressed_header(cpi, data);
  data += first_part_size;

  data_sz = encode_tiles(cpi, data, &max_tile);
#if CONFIG_MISC_FIXES
  if (max_tile > 0) {
    int mag;
    unsigned int mask;

    // Choose the (tile size) magnitude
    for (mag = 0, mask = 0xff; mag < 4; mag++) {
      if (max_tile <= mask)
        break;
      mask <<= 8;
      mask |= 0xff;
    }
    assert(n_log2_tiles > 0);
    vpx_wb_write_literal(&saved_wb, mag, 2);
    if (mag < 3)
      data_sz = remux_tiles(data, (int)data_sz, 1 << n_log2_tiles, mag);
  } else {
    assert(n_log2_tiles == 0);
  }
#endif
  data += data_sz;

  // TODO(jbb): Figure out what to do if first_part_size > 16 bits.
  vpx_wb_write_literal(&saved_wb, (int)first_part_size, 16);

  *size = data - dest;
}
