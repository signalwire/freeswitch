/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_ENCODER_VP9_FIRSTPASS_H_
#define VP9_ENCODER_VP9_FIRSTPASS_H_

#include "vp9/encoder/vp9_lookahead.h"
#include "vp9/encoder/vp9_ratectrl.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_FP_MB_STATS

#define FPMB_DCINTRA_MASK 0x01

#define FPMB_MOTION_ZERO_MASK 0x02
#define FPMB_MOTION_LEFT_MASK 0x04
#define FPMB_MOTION_RIGHT_MASK 0x08
#define FPMB_MOTION_UP_MASK 0x10
#define FPMB_MOTION_DOWN_MASK 0x20

#define FPMB_ERROR_SMALL_MASK 0x40
#define FPMB_ERROR_LARGE_MASK 0x80
#define FPMB_ERROR_SMALL_TH 2000
#define FPMB_ERROR_LARGE_TH 48000

typedef struct {
  uint8_t *mb_stats_start;
  uint8_t *mb_stats_end;
} FIRSTPASS_MB_STATS;
#endif

#define INVALID_ROW -1

typedef struct {
  double frame_mb_intra_factor;
  double frame_mb_brightness_factor;
  double frame_mb_neutral_count;
} FP_MB_FLOAT_STATS;

typedef struct {
  double intra_factor;
  double brightness_factor;
  int64_t coded_error;
  int64_t sr_coded_error;
  int64_t frame_noise_energy;
  int64_t intra_error;
  int intercount;
  int second_ref_count;
  double neutral_count;
  double intra_count_low;   // Coded intra but low variance
  double intra_count_high;  // Coded intra high variance
  int intra_skip_count;
  int image_data_start_row;
  int mvcount;
  int sum_mvr;
  int sum_mvr_abs;
  int sum_mvc;
  int sum_mvc_abs;
  int64_t sum_mvrs;
  int64_t sum_mvcs;
  int sum_in_vectors;
  int intra_smooth_count;
} FIRSTPASS_DATA;

typedef struct {
  double frame;
  double weight;
  double intra_error;
  double coded_error;
  double sr_coded_error;
  double frame_noise_energy;
  double pcnt_inter;
  double pcnt_motion;
  double pcnt_second_ref;
  double pcnt_neutral;
  double pcnt_intra_low;   // Coded intra but low variance
  double pcnt_intra_high;  // Coded intra high variance
  double intra_skip_pct;
  double intra_smooth_pct;    // % of blocks that are smooth
  double inactive_zone_rows;  // Image mask rows top and bottom.
  double inactive_zone_cols;  // Image mask columns at left and right edges.
  double MVr;
  double mvr_abs;
  double MVc;
  double mvc_abs;
  double MVrv;
  double MVcv;
  double mv_in_out_count;
  double duration;
  double count;
  int64_t spatial_layer_id;
} FIRSTPASS_STATS;

typedef enum {
  KF_UPDATE = 0,
  LF_UPDATE = 1,
  GF_UPDATE = 2,
  ARF_UPDATE = 3,
  OVERLAY_UPDATE = 4,
  FRAME_UPDATE_TYPES = 5
} FRAME_UPDATE_TYPE;

#define FC_ANIMATION_THRESH 0.15
typedef enum {
  FC_NORMAL = 0,
  FC_GRAPHICS_ANIMATION = 1,
  FRAME_CONTENT_TYPES = 2
} FRAME_CONTENT_TYPE;

typedef struct {
  unsigned char index;
  unsigned char first_inter_index;
  RATE_FACTOR_LEVEL rf_level[(MAX_LAG_BUFFERS * 2) + 1];
  FRAME_UPDATE_TYPE update_type[(MAX_LAG_BUFFERS * 2) + 1];
  unsigned char arf_src_offset[(MAX_LAG_BUFFERS * 2) + 1];
  unsigned char arf_update_idx[(MAX_LAG_BUFFERS * 2) + 1];
  unsigned char arf_ref_idx[(MAX_LAG_BUFFERS * 2) + 1];
  int bit_allocation[(MAX_LAG_BUFFERS * 2) + 1];
} GF_GROUP;

typedef struct {
  unsigned int section_intra_rating;
  FIRSTPASS_STATS total_stats;
  FIRSTPASS_STATS this_frame_stats;
  const FIRSTPASS_STATS *stats_in;
  const FIRSTPASS_STATS *stats_in_start;
  const FIRSTPASS_STATS *stats_in_end;
  FIRSTPASS_STATS total_left_stats;
  int first_pass_done;
  int64_t bits_left;
  double mean_mod_score;
  double normalized_score_left;
  double mb_av_energy;
  double mb_smooth_pct;

#if CONFIG_FP_MB_STATS
  uint8_t *frame_mb_stats_buf;
  uint8_t *this_frame_mb_stats;
  FIRSTPASS_MB_STATS firstpass_mb_stats;
#endif

  FP_MB_FLOAT_STATS *fp_mb_float_stats;

  // An indication of the content type of the current frame
  FRAME_CONTENT_TYPE fr_content_type;

  // Projected total bits available for a key frame group of frames
  int64_t kf_group_bits;

  // Error score of frames still to be coded in kf group
  double kf_group_error_left;

  double bpm_factor;
  int rolling_arf_group_target_bits;
  int rolling_arf_group_actual_bits;

  int sr_update_lag;
  int kf_zeromotion_pct;
  int last_kfgroup_zeromotion_pct;
  int active_worst_quality;
  int baseline_active_worst_quality;
  int extend_minq;
  int extend_maxq;
  int extend_minq_fast;
  int arnr_strength_adjustment;

  GF_GROUP gf_group;
} TWO_PASS;

struct VP9_COMP;
struct ThreadData;
struct TileDataEnc;

void vp9_init_first_pass(struct VP9_COMP *cpi);
void vp9_rc_get_first_pass_params(struct VP9_COMP *cpi);
void vp9_first_pass(struct VP9_COMP *cpi, const struct lookahead_entry *source);
void vp9_end_first_pass(struct VP9_COMP *cpi);

void vp9_first_pass_encode_tile_mb_row(struct VP9_COMP *cpi,
                                       struct ThreadData *td,
                                       FIRSTPASS_DATA *fp_acc_data,
                                       struct TileDataEnc *tile_data,
                                       MV *best_ref_mv, int mb_row);

void vp9_init_second_pass(struct VP9_COMP *cpi);
void vp9_rc_get_second_pass_params(struct VP9_COMP *cpi);
void vp9_twopass_postencode_update(struct VP9_COMP *cpi);

// Post encode update of the rate control parameters for 2-pass
void vp9_twopass_postencode_update(struct VP9_COMP *cpi);

void calculate_coded_size(struct VP9_COMP *cpi, int *scaled_frame_width,
                          int *scaled_frame_height);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_ENCODER_VP9_FIRSTPASS_H_
