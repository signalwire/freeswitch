BASE=../../../..

LOCAL_SOURCES=arith_routines.c\
arith_routines_hist.c\
arith_routines_logist.c\
auto_correlation.c\
auto_corr_to_refl_coef.c\
bandwidth_estimator.c\
complex_bit_reverse.c\
complex_fft.c\
copy_set_operations.c\
crc.c\
cross_correlation.c\
decode_bwe.c\
decode.c\
division_operations.c\
dot_product_with_scale.c\
downsample_fast.c\
encode.c\
encode_lpc_swb.c\
energy.c\
entropy_coding.c\
fft.c\
filter_ar.c\
filter_ar_fast_q12.c\
filterbanks.c\
filterbank_tables.c\
filter_functions.c\
filter_ma_fast_q12.c\
get_hanning_window.c\
get_scaling_square.c\
ilbc_specific_functions.c\
intialize.c\
isac.c\
lattice.c\
levinson_durbin.c\
lpc_analysis.c\
lpc_gain_swb_tables.c\
lpc_shape_swb12_tables.c\
lpc_shape_swb16_tables.c\
lpc_tables.c\
lpc_to_refl_coef.c\
min_max_operations.c\
min_max_operations_neon.c\
pitch_estimator.c\
pitch_filter.c\
pitch_gain_tables.c\
pitch_lag_tables.c\
randomization_functions.c\
refl_coef_to_lpc.c\
resample_48khz.c\
resample_by_2.c\
resample_by_2_internal.c\
resample.c\
resample_fractional.c\
spectrum_ar_model_tables.c\
splitting_filter.c\
spl_sqrt.c\
spl_sqrt_floor.c\
spl_version.c\
sqrt_of_one_minus_x_squared.c\
transform.c\
vector_scaling_operations.c\
webrtc_fft_t_1024_8.c\
webrtc_fft_t_rad.c ""

LOCAL_OBJS=$(shell echo $(LOCAL_SOURCES) | sed "s/.c /.o /g")
LOCAL_CFLAGS=-w -I. 

include $(BASE)/build/modmake.rules

