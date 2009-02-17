/*
 * iLBC - a library for the iLBC codec
 *
 * ilbc.h - The iLBC low bit rate speech codec.
 *
 * Adapted by Steve Underwood <steveu@coppice.org> from the reference
 * iLBC code supplied in RFC3951.
 *
 * Copyright (C) The Internet Society (2004).
 * All Rights Reserved.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: ilbc.h,v 1.1.1.1 2008/02/15 12:15:55 steveu Exp $
 */

#if !defined(_ILBC_ILBC_H_)
#define _ILBC_ILBC_H_

#define ILBC_BLOCK_LEN_20MS     160
#define ILBC_BLOCK_LEN_30MS     240
#define ILBC_BLOCK_LEN_MAX      240

#define ILBC_NO_OF_BYTES_20MS   38
#define ILBC_NO_OF_BYTES_30MS   50
#define ILBC_NO_OF_BYTES_MAX    50

#define ILBC_NUM_SUB_MAX        6

#define SUBL                    40

#define ENH_BLOCKL              80  /* block length */
#define ENH_NBLOCKS_TOT         8   /* ENH_NBLOCKS + ENH_NBLOCKS_EXTRA */
#define ENH_BUFL                (ENH_NBLOCKS_TOT*ENH_BLOCKL)

#define ILBC_LPC_FILTERORDER    10
#define LPC_LOOKBACK            60

#define CB_NSTAGES              3

#define STATE_BITS              3
#define BYTE_LEN                8
#define ILBC_ULP_CLASSES        3

typedef struct
{
    int lsf_bits[6][ILBC_ULP_CLASSES + 2];
    int start_bits[ILBC_ULP_CLASSES + 2];
    int startfirst_bits[ILBC_ULP_CLASSES + 2];
    int scale_bits[ILBC_ULP_CLASSES + 2];
    int state_bits[ILBC_ULP_CLASSES + 2];
    int extra_cb_index[CB_NSTAGES][ILBC_ULP_CLASSES + 2];
    int extra_cb_gain[CB_NSTAGES][ILBC_ULP_CLASSES + 2];
    int cb_index[ILBC_NUM_SUB_MAX][CB_NSTAGES][ILBC_ULP_CLASSES + 2];
    int cb_gain[ILBC_NUM_SUB_MAX][CB_NSTAGES][ILBC_ULP_CLASSES + 2];
} ilbc_ulp_inst_t;

/* Type definition encoder instance */
typedef struct
{
    /* flag for frame size mode */
    int mode;

    /* basic parameters for different frame sizes */
    int blockl;
    int nsub;
    int nasub;
    int no_of_bytes;
    int lpc_n;
    int state_short_len;
    const ilbc_ulp_inst_t *ULP_inst;

    /* analysis filter state */
    float anaMem[ILBC_LPC_FILTERORDER];

    /* old lsf parameters for interpolation */
    float lsfold[ILBC_LPC_FILTERORDER];
    float lsfdeqold[ILBC_LPC_FILTERORDER];

    /* signal buffer for LP analysis */
    float lpc_buffer[LPC_LOOKBACK + ILBC_BLOCK_LEN_MAX];

    /* state of input HP filter */
    float hpimem[4];
} ilbc_encode_state_t;

/* Type definition decoder instance */
typedef struct
{
    /* Flag for frame size mode */
    int mode;

    /* Basic parameters for different frame sizes */
    int blockl;
    int nsub;
    int nasub;
    int no_of_bytes;
    int lpc_n;
    int state_short_len;
    const ilbc_ulp_inst_t *ULP_inst;

    /* Synthesis filter state */
    float syntMem[ILBC_LPC_FILTERORDER];

    /* Old LSF for interpolation */
    float lsfdeqold[ILBC_LPC_FILTERORDER];

    /* Pitch lag estimated in enhancer and used in PLC */
    int last_lag;

    /* PLC state information */
    int prevLag, consPLICount, prevPLI, prev_enh_pl;
    float prevLpc[ILBC_LPC_FILTERORDER + 1];
    float prevResidual[ILBC_NUM_SUB_MAX*SUBL];
    float per;
    unsigned long seed;

    /* Previous synthesis filter parameters */
    float old_syntdenum[(ILBC_LPC_FILTERORDER + 1)*ILBC_NUM_SUB_MAX];

    /* State of output HP filter */
    float hpomem[4];

    /* Enhancer state information */
    int use_enhancer;
    float enh_buf[ENH_BUFL];
    float enh_period[ENH_NBLOCKS_TOT];
} ilbc_decode_state_t;

ilbc_encode_state_t *ilbc_encode_init(ilbc_encode_state_t *s,    /* (i/o) Encoder instance */
                                      int mode);                 /* (i) frame size mode */

int ilbc_encode(ilbc_encode_state_t *s,         /* (i/o) the general encoder state */
                uint8_t bytes[],                /* (o) encoded data bits iLBC */
                const int16_t amp[],            /* (o) speech vector to encode */
                int len);

ilbc_decode_state_t *ilbc_decode_init(ilbc_decode_state_t *s,   /* (i/o) Decoder instance */
                                      int mode,                 /* (i) frame size mode */
                                      int use_enhancer);        /* (i) 1 to use enhancer
                                                                       0 to run without enhancer */

int ilbc_decode(ilbc_decode_state_t *s,     /* (i/o) the decoder state structure */
                int16_t amp[],              /* (o) decoded signal block */
                const uint8_t bytes[],      /* (i) encoded signal bits */
                int len);

int ilbc_fillin(ilbc_decode_state_t *s,     /* (i/o) the decoder state structure */
                int16_t amp[],              /* (o) decoded signal block */
                int len);

#endif
