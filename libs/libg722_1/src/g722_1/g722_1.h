/*
 * g722_1 - a library for the G.722.1 and Annex C codecs
 *
 * g722_1.h
 *
 * Adapted by Steve Underwood <steveu@coppice.org> from the reference
 * code supplied with ITU G.722.1, which is:
 *
 *   © 2004 Polycom, Inc.
 *   All rights reserved.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id: g722_1.h,v 1.14 2008/10/17 13:18:21 steveu Exp $
 */

#if !defined(_G722_1_G722_1_H_)
#define _G722_1_G722_1_H_

typedef enum
{
    /*! \brief Basic G.722.1 sampling rate */
    G722_1_SAMPLE_RATE_16000 = 16000,
    /*! \brief G.722.1 Annex C sampling rate */
    G722_1_SAMPLE_RATE_32000 = 32000
} g722_1_sample_rates_t;

typedef enum
{
    /*! \brief Bit rate usable at either sampling rate. */
    G722_1_BIT_RATE_24000 = 24000,
    /*! \brief Bit rate usable at either sampling rate. */
    G722_1_BIT_RATE_32000 = 32000,
    /*! \brief Bit rate usable at 32000 samples per second. */
    G722_1_BIT_RATE_48000 = 48000
} g722_1_bit_rates_t;

#define MAX_SAMPLE_RATE         32000
/* Frames are 20ms */
#define MAX_FRAME_SIZE          (MAX_SAMPLE_RATE/50)
#define MAX_DCT_LENGTH          640

/* Max bit rate is 48000 bits/sec. */
#define MAX_BITS_PER_FRAME      960

#define NUMBER_OF_REGIONS       14
#define MAX_NUMBER_OF_REGIONS   28

/*! Bitstream handler state */
typedef struct
{
    /*! The bit stream. */
    uint32_t bitstream;
    /*! The residual bits in bitstream. */
    int residue;
} g722_1_bitstream_state_t;

typedef struct
{
    int16_t code_bit_count;      /* bit count of the current word */
    int16_t current_word;        /* current word in the bitstream being processed */
    uint16_t *code_word_ptr;     /* pointer to the bitstream */
} g722_1_bitstream_t;

typedef struct
{
    int16_t seed0;
    int16_t seed1;
    int16_t seed2;
    int16_t seed3;
} g722_1_rand_t;

typedef struct
{
    int bit_rate;
    int sample_rate;
    int frame_size;
    int number_of_regions;
    int number_of_bits_per_frame;
    int bytes_per_frame;
    int number_of_16bit_words_per_frame;
#if defined(G722_1_USE_FIXED_POINT)
    int16_t history[MAX_FRAME_SIZE];
#else
    float history[MAX_FRAME_SIZE];
    float scale_factor;
#endif
    g722_1_bitstream_state_t bitstream;
} g722_1_encode_state_t;

typedef struct
{
    int bit_rate;
    int sample_rate;
    int frame_size;
    int number_of_regions;
    int number_of_bits_per_frame;
    int bytes_per_frame;
    int number_of_16bit_words_per_frame;
    int16_t words;
    int16_t old_mag_shift;
#if defined(G722_1_USE_FIXED_POINT)
    int16_t old_decoder_mlt_coefs[MAX_DCT_LENGTH];
    int16_t old_samples[MAX_DCT_LENGTH >> 1];
#else
    float old_decoder_mlt_coefs[MAX_DCT_LENGTH];
    float old_samples[MAX_DCT_LENGTH >> 1];
#endif
    g722_1_bitstream_t bitobj;
    g722_1_bitstream_state_t bitstream;
    const uint8_t *code_ptr;
    int16_t number_of_bits_left;
    g722_1_rand_t randobj;
} g722_1_decode_state_t;

#if defined(__cplusplus)
extern "C"
{
#endif

/*! Initialise a G.722.1 encode context.
    \param s The G.722.1 encode context.
    \param bit_rate The required bit rate for the G.722.1 data.
           The valid rates are 48000, 32000 and 24000.
    \param sample_rate The required sampling rate.
           The valid rates are 16000 and 32000.
    \return A pointer to the G.722.1 encode context, or NULL for error. */
g722_1_encode_state_t *g722_1_encode_init(g722_1_encode_state_t *s, int bit_rate, int sample_rate);

/*! Release a G.722.1 encode context.
    \param s The G.722.1 encode context.
    \return 0. */
int g722_1_encode_release(g722_1_encode_state_t *s);

/*! Encode a buffer of linear PCM data to G.722.1
    \param s The G.722.1 encode context.
    \param g722_1_data The G.722.1 data produced.
    \param amp The audio sample buffer.
    \param len The number of samples in the buffer.
    \return The number of bytes of G.722.1 data produced. */
int g722_1_encode(g722_1_encode_state_t *s, uint8_t g722_1_data[], const int16_t amp[], int len);

/*! Change the bit rate for an G.722.1 decode context.
    \param s The G.722.1 decode context.
    \param bit_rate The required bit rate for the G.722.1 data.
           The valid rates are 48000, 32000 and 24000.
    \return 0 for OK, or -1 for a bad parameter. */
int g722_1_encode_set_rate(g722_1_encode_state_t *s, int bit_rate);

/*! Initialise a G.722.1 decode context.
    \param s The G.722.1 decode context.
    \param bit_rate The required bit rate for the G.722.1 data.
           The valid rates are 48000, 32000 and 24000.
    \param sample_rate The required sampling rate.
           The valid rates are 16000 and 32000.
    \return A pointer to the G.722.1 decode context, or NULL for error. */
g722_1_decode_state_t *g722_1_decode_init(g722_1_decode_state_t *s, int bit_rate, int sample_rate);

/*! Release a G.722.1 decode context.
    \param s The G.722.1 decode context.
    \return 0. */
int g722_1_decode_release(g722_1_decode_state_t *s);

/*! Decode a buffer of G.722.1 data to linear PCM.
    \param s The G.722.1 decode context.
    \param amp The audio sample buffer.
    \param g722_1_data
    \param len
    \return The number of samples returned. */
int g722_1_decode(g722_1_decode_state_t *s, int16_t amp[], const uint8_t g722_1_data[], int len);

/*! Produce linear PCM data to fill in where received G.722.1 data is missing.
    \param s The G.722.1 decode context.
    \param amp The audio sample buffer.
    \param g722_1_data
    \param len
    \return The number of samples returned. */
int g722_1_fillin(g722_1_decode_state_t *s, int16_t amp[], const uint8_t g722_1_data[], int len);

/*! Change the bit rate for an G.722.1 decode context.
    \param s The G.722.1 decode context.
    \param bit_rate The required bit rate for the G.722.1 data.
           The valid rates are 48000, 32000 and 24000.
    \return 0 for OK, or -1 for a bad parameter. */
int g722_1_decode_set_rate(g722_1_decode_state_t *s, int bit_rate);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
