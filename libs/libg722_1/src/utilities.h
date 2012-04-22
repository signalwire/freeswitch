/*
 * g722_1 - a library for the G.722.1 and Annex C codecs
 *
 * utilities.h
 *
 * Copyright (C) 2006 Steve Underwood
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

#if !defined(__UTILITIES_H__)
#define __UTILITIES_H__

/* Prototypes for some general purpose signal and vector functions */
#if defined(G722_1_USE_FIXED_POINT)
void vec_copyi16(int16_t z[], const int16_t x[], int n);
int32_t vec_dot_prodi16(const int16_t x[], const int16_t y[], int n);
#else
void vec_copyf(float z[], const float x[], int n);
void vec_zerof(float z[], int n);
void vec_subf(float z[], const float x[], const float y[], int n);
void vec_scalar_mulf(float z[], const float x[], float y, int n);
void vec_mulf(float z[], const float x[], const float y[], int n);
float vec_dot_prodf(const float x[], const float y[], int n);
void vec_scaled_addf(float z[], const float x[], float x_scale, const float y[], float y_scale, int n);
void vec_scaled_subf(float z[], const float x[], float x_scale, const float y[], float y_scale, int n);
#endif

#endif
/*- End of file ------------------------------------------------------------*/
