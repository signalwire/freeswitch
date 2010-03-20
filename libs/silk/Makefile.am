
AM_CFLAGS = -Isrc -Iinterface -fPIC -Wall -O3 
AUTOMAKE_OPTS = gnu
NAME = libSKP_SILK_SDK
AM_CPPFLAGS = $(AM_CFLAGS)

EXTRA_DIST = Silk_SDK.sln \
src/Silk_FIX.vcproj \
test/Encoder.c \
test/Enc_SDK.vcproj \
test/Decoder.c \
test/Dec_SDK.vcproj \
test/signalCompare.c \
test/SignalCompare.vcproj 

lib_LTLIBRARIES	= libSKP_SILK_SDK.la
libSKP_SILK_SDK_la_SOURCES = src/SKP_Silk_A2NLSF.c \
src/SKP_Silk_allpass_int.c \
src/SKP_Silk_ana_filt_bank_1.c \
src/SKP_Silk_apply_sine_window.c \
src/SKP_Silk_array_maxabs.c \
src/SKP_Silk_autocorr.c \
src/SKP_Silk_biquad_alt.c \
src/SKP_Silk_biquad.c \
src/SKP_Silk_burg_modified.c \
src/SKP_Silk_bwexpander_32.c \
src/SKP_Silk_bwexpander.c \
src/SKP_Silk_CNG.c \
src/SKP_Silk_code_signs.c \
src/SKP_Silk_control_codec_FIX.c \
src/SKP_Silk_corrMatrix_FIX.c \
src/SKP_Silk_create_init_destroy.c \
src/SKP_Silk_dec_API.c \
src/SKP_Silk_decode_core.c \
src/SKP_Silk_decode_frame.c \
src/SKP_Silk_decode_indices_v4.c \
src/SKP_Silk_decode_parameters.c \
src/SKP_Silk_decode_parameters_v4.c \
src/SKP_Silk_decode_pulses.c \
src/SKP_Silk_decoder_set_fs.c \
src/SKP_Silk_detect_SWB_input.c \
src/SKP_Silk_enc_API.c \
src/SKP_Silk_encode_frame_FIX.c \
src/SKP_Silk_encode_parameters.c \
src/SKP_Silk_encode_parameters_v4.c \
src/SKP_Silk_encode_pulses.c \
src/SKP_Silk_find_LPC_FIX.c \
src/SKP_Silk_find_LTP_FIX.c \
src/SKP_Silk_find_pitch_lags_FIX.c \
src/SKP_Silk_find_pred_coefs_FIX.c \
src/SKP_Silk_gain_quant.c \
src/SKP_Silk_HP_variable_cutoff_FIX.c \
src/SKP_Silk_init_encoder_FIX.c \
src/SKP_Silk_inner_prod_aligned.c \
src/SKP_Silk_interpolate.c \
src/SKP_Silk_k2a.c \
src/SKP_Silk_k2a_Q16.c \
src/SKP_Silk_LBRR_reset.c \
src/SKP_Silk_lin2log.c \
src/SKP_Silk_log2lin.c \
src/SKP_Silk_lowpass_int.c \
src/SKP_Silk_lowpass_short.c \
src/SKP_Silk_LPC_inv_pred_gain.c \
src/SKP_Silk_LPC_stabilize.c \
src/SKP_Silk_LPC_synthesis_filter.c \
src/SKP_Silk_LPC_synthesis_order16.c \
src/SKP_Silk_LP_variable_cutoff.c \
src/SKP_Silk_LSF_cos_table.c \
src/SKP_Silk_LTP_analysis_filter_FIX.c \
src/SKP_Silk_LTP_scale_ctrl_FIX.c \
src/SKP_Silk_MA.c \
src/SKP_Silk_NLSF2A.c \
src/SKP_Silk_NLSF2A_stable.c \
src/SKP_Silk_NLSF_MSVQ_decode.c \
src/SKP_Silk_NLSF_MSVQ_encode_FIX.c \
src/SKP_Silk_NLSF_stabilize.c \
src/SKP_Silk_NLSF_VQ_rate_distortion_FIX.c \
src/SKP_Silk_NLSF_VQ_sum_error_FIX.c \
src/SKP_Silk_NLSF_VQ_weights_laroia.c \
src/SKP_Silk_noise_shape_analysis_FIX.c \
src/SKP_Silk_NSQ.c \
src/SKP_Silk_NSQ_del_dec.c \
src/SKP_Silk_pitch_analysis_core.c \
src/SKP_Silk_pitch_est_tables.c \
src/SKP_Silk_PLC.c \
src/SKP_Silk_prefilter_FIX.c \
src/SKP_Silk_process_gains_FIX.c \
src/SKP_Silk_process_NLSFs_FIX.c \
src/SKP_Silk_pulses_to_bytes.c \
src/SKP_Silk_quant_LTP_gains_FIX.c \
src/SKP_Silk_range_coder.c \
src/SKP_Silk_regularize_correlations_FIX.c \
src/SKP_Silk_resample_1_2.c \
src/SKP_Silk_resample_1_2_coarse.c \
src/SKP_Silk_resample_1_2_coarsest.c \
src/SKP_Silk_resample_1_3.c \
src/SKP_Silk_resample_2_1_coarse.c \
src/SKP_Silk_resample_2_3.c \
src/SKP_Silk_resample_2_3_coarse.c \
src/SKP_Silk_resample_2_3_coarsest.c \
src/SKP_Silk_resample_2_3_rom.c \
src/SKP_Silk_resample_3_1.c \
src/SKP_Silk_resample_3_2.c \
src/SKP_Silk_resample_3_2_rom.c \
src/SKP_Silk_resample_3_4.c \
src/SKP_Silk_resample_4_3.c \
src/SKP_Silk_residual_energy16_FIX.c \
src/SKP_Silk_residual_energy_FIX.c \
src/SKP_Silk_scale_copy_vector16.c \
src/SKP_Silk_scale_vector.c \
src/SKP_Silk_schur64.c \
src/SKP_Silk_schur.c \
src/SKP_Silk_shell_coder.c \
src/SKP_Silk_sigm_Q15.c \
src/SKP_Silk_solve_LS_FIX.c \
src/SKP_Silk_sort.c \
src/SKP_Silk_sum_sqr_shift.c \
src/SKP_Silk_tables_gain.c \
src/SKP_Silk_tables_LTP.c \
src/SKP_Silk_tables_NLSF_CB0_10.c \
src/SKP_Silk_tables_NLSF_CB0_16.c \
src/SKP_Silk_tables_NLSF_CB1_10.c \
src/SKP_Silk_tables_NLSF_CB1_16.c \
src/SKP_Silk_tables_other.c \
src/SKP_Silk_tables_pitch_lag.c \
src/SKP_Silk_tables_pulses_per_block.c \
src/SKP_Silk_tables_sign.c \
src/SKP_Silk_tables_type_offset.c \
src/SKP_Silk_VAD.c \
src/SKP_Silk_VQ_nearest_neighbor_FIX.c 


libSKP_SILK_SDK_la_CFLAGS = $(AM_CFLAGS)
libSKP_SILK_SDK_la_LDFLAGS = $(LIBS)

library_includedir = $(prefix)/include/silk
library_include_HEADERS	= src/SKP_Silk_common_pitch_est_defines.h \
src/SKP_Silk_define_FIX.h \
src/SKP_Silk_define.h \
src/SKP_Silk_Inlines.h \
src/SKP_Silk_macros.h \
src/SKP_Silk_main_FIX.h \
src/SKP_Silk_main.h \
src/SKP_Silk_perceptual_parameters_FIX.h \
src/SKP_Silk_pitch_est_defines.h \
src/SKP_Silk_PLC.h \
src/SKP_Silk_resample_rom.h \
src/SKP_Silk_SigProc_FIX.h \
src/SKP_Silk_structs_FIX.h \
src/SKP_Silk_structs.h \
src/SKP_Silk_tables.h \
src/SKP_Silk_tables_NLSF_CB0_10.h \
src/SKP_Silk_tables_NLSF_CB0_16.h \
src/SKP_Silk_tables_NLSF_CB1_10.h \
src/SKP_Silk_tables_NLSF_CB1_16.h \
interface/SKP_Silk_control.h \
interface/SKP_Silk_errors.h \
interface/SKP_Silk_SDK_API.h \
interface/SKP_Silk_typedef.h 

bin_PROGRAMS = Encoder Decoder signalCompare
Encoder_SOURCES = test/Encoder.c $(top_builddir)/interface/SKP_Silk_SDK_API.h
Encoder_LDADD = $(lib_LTLIBRARIES) 
Encoder_LDFLAGS = $(LIBS)

Decoder_SOURCES = test/Decoder.c $(top_builddir)/interface/SKP_Silk_SDK_API.h
Decoder_LDADD = $(lib_LTLIBRARIES) 
Decoder_LDFLAGS = $(LIBS)

signalCompare_SOURCES = test/signalCompare.c $(top_builddir)/interface/SKP_Silk_SDK_API.h
signalCompare_LDADD = $(lib_LTLIBRARIES) 
signalCompare_LDFLAGS = $(LIBS)
