/*
 * skeleton.h
 * author: Tahseen Mohammad
 */

#ifndef _SKELETON_H
#define _SKELETON_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WIN32
#define snprintf _snprintf
#endif

#include <ogg/ogg.h>

#define SKELETON_VERSION_MAJOR 3
#define SKELETON_VERSION_MINOR 0
#define FISHEAD_IDENTIFIER "fishead\0"
#define FISBONE_IDENTIFIER "fisbone\0"
#define FISHEAD_SIZE 64
#define FISBONE_SIZE 52
#define FISBONE_MESSAGE_HEADER_OFFSET 44

/* fishead_packet holds a fishead header packet. */
typedef struct {
    ogg_uint16_t version_major;				    /* skeleton version major */
    ogg_uint16_t version_minor;				    /* skeleton version minor */
    /* Start time of the presentation
     * For a new stream presentationtime & basetime is same. */
    ogg_int64_t ptime_n;                                    /* presentation time numerator */
    ogg_int64_t ptime_d;                                    /* presentation time denominator */
    ogg_int64_t btime_n;                                    /* basetime numerator */
    ogg_int64_t btime_d;                                    /* basetime denominator */
    /* will holds the time of origin of the stream, a 20 bit field. */
    unsigned char UTC[20];
} fishead_packet;

/* fisbone_packet holds a fisbone header packet. */
typedef struct {
    ogg_uint32_t serial_no;                                 /* serial no of the corresponding stream */
    ogg_uint32_t nr_header_packet;                      /* number of header packets */
    /* granule rate is the temporal resolution of the logical bitstream */
    ogg_int64_t granule_rate_n;                            /* granule rate numerator */
    ogg_int64_t granule_rate_d;                            /* granule rate denominator */
    ogg_int64_t start_granule;                             /* start granule value */
    ogg_uint32_t preroll;                                   /* preroll */
    unsigned char granule_shift;		            /* 1 byte value holding the granule shift */
    char *message_header_fields;                            /* holds all the message header fields */
    /* current total size of the message header fields, for realloc purpose, initially zero */
    ogg_uint32_t current_header_size;
} fisbone_packet;

extern int write_ogg_page_to_file(ogg_page *og, FILE *out);
extern int add_message_header_field(fisbone_packet *fp, char *header_key, char *header_value);
/* remember to deallocate the returned ogg_packet properly */
extern ogg_packet ogg_from_fishead(fishead_packet *fp);
extern ogg_packet ogg_from_fisbone(fisbone_packet *fp);
extern fishead_packet fishead_from_ogg(ogg_packet *op);
extern fisbone_packet fisbone_from_ogg(ogg_packet *op);
extern int add_fishead_to_stream(ogg_stream_state *os, fishead_packet *fp);
extern int add_fisbone_to_stream(ogg_stream_state *os, fisbone_packet *fp);
extern int add_eos_packet_to_stream(ogg_stream_state *os);
extern int flush_ogg_stream_to_file(ogg_stream_state *os, FILE *out);

#ifdef __cplusplus
}
#endif

#endif  /* _SKELETON_H */






