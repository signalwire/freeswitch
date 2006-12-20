/*
 * skeleton.c
 * author: Tahseen Mohammad
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <ogg/ogg.h>

#include "skeleton.h"

/* write an ogg_page to a file pointer */
int write_ogg_page_to_file(ogg_page *og, FILE *out) {
   int written;
   
   written = fwrite(og->header,1, og->header_len, out);
   written += fwrite(og->body,1, og->body_len, out);

   return written;
}

int add_message_header_field(fisbone_packet *fp, 
                                        char *header_key, 
                                        char *header_value) {

    /* size of both key and value + ': ' + CRLF */
    int this_message_size = strlen(header_key) + strlen(header_value) + 4;
    if (fp->message_header_fields == NULL) {
        fp->message_header_fields = _ogg_calloc(this_message_size, sizeof(char));
    } else {
        int new_size = (fp->current_header_size + this_message_size) * sizeof(char);
        fp->message_header_fields = _ogg_realloc(fp->message_header_fields, new_size);
    }
    snprintf(fp->message_header_fields + fp->current_header_size, 
                this_message_size+1, 
                "%s: %s\r\n", 
                header_key, 
                header_value);
    fp->current_header_size += this_message_size;

    return 0;
}

/* create a ogg_packet from a fishead_packet structure */
ogg_packet ogg_from_fishead(fishead_packet *fp) {

    ogg_packet op;

    memset(&op, 0, sizeof(op));
    op.packet = _ogg_calloc(FISHEAD_SIZE, sizeof(unsigned char));
    memset(op.packet, 0, FISHEAD_SIZE);

    memcpy (op.packet, FISHEAD_IDENTIFIER, 8); /* identifier */
    *((ogg_uint16_t*)(op.packet+8)) = SKELETON_VERSION_MAJOR; /* version major */
    *((ogg_uint16_t*)(op.packet+10)) = SKELETON_VERSION_MINOR; /* version minor */
    *((ogg_int64_t*)(op.packet+12)) = (ogg_int64_t)fp->ptime_n; /* presentationtime numerator */
    *((ogg_int64_t*)(op.packet+20)) = (ogg_int64_t)fp->ptime_d; /* presentationtime denominator */
    *((ogg_int64_t*)(op.packet+28)) = (ogg_int64_t)fp->btime_n; /* basetime numerator */
    *((ogg_int64_t*)(op.packet+36)) = (ogg_int64_t)fp->btime_d; /* basetime denominator */
    /* TODO: UTC time, set to zero for now */

    op.b_o_s = 1;   /* its the first packet of the stream */
    op.e_o_s = 0;   /* its not the last packet of the stream */
    op.bytes = FISHEAD_SIZE;  /* length of the packet in bytes */

    return op;
}

/* create a ogg_packet from a fisbone_packet structure. 
 * call this method after the fisbone_packet is filled and all message header fields are added
 * by calling add_message_header_field method.
 */
ogg_packet ogg_from_fisbone(fisbone_packet *fp) {
    
    ogg_packet op;
    int packet_size = FISBONE_SIZE + fp->current_header_size;

    memset (&op, 0, sizeof (op));       
    op.packet = _ogg_calloc (packet_size, sizeof(unsigned char));
    memset (op.packet, 0, packet_size);
    memcpy (op.packet, FISBONE_IDENTIFIER, 8); /* identifier */
    *((ogg_uint32_t*)(op.packet+8)) = FISBONE_MESSAGE_HEADER_OFFSET; /* offset of the message header fields */
    *((ogg_uint32_t*)(op.packet+12)) = fp->serial_no; /* serialno of the respective stream */
    *((ogg_uint32_t*)(op.packet+16)) = fp->nr_header_packet; /* number of header packets */
    *((ogg_int64_t*)(op.packet+20)) = fp->granule_rate_n; /* granulrate numerator */
    *((ogg_int64_t*)(op.packet+28)) = fp->granule_rate_d; /* granulrate denominator */
    *((ogg_int64_t*)(op.packet+36)) = fp->start_granule; /* start granule */
    *((ogg_uint32_t*)(op.packet+44)) = fp->preroll; /* preroll, for theora its 0 */
    *(op.packet+48) = fp->granule_shift; /* granule shift */
    memcpy((op.packet+FISBONE_SIZE), fp->message_header_fields, fp->current_header_size);

    op.b_o_s = 0;
    op.e_o_s = 0;
    op.bytes = packet_size; /* size of the packet in bytes */

    return op;
}

/* fills up a fishead_packet from a fishead ogg_packet of a skeleton bistream */
fishead_packet fishead_from_ogg(ogg_packet *op) {
	
    fishead_packet fp;

    if (memcmp(op->packet, FISHEAD_IDENTIFIER, 8))
	; /* invalid packet what do we do? */

    fp.version_major = *((ogg_uint16_t*)(op->packet+8)); /* version major */
    fp.version_minor = *((ogg_uint16_t*)(op->packet+10)); /* version minor */
    fp.ptime_n = *((ogg_int64_t*)(op->packet+12)); /* presentationtime numerator */
    fp.ptime_d = *((ogg_int64_t*)(op->packet+20)); /* presentationtime denominator */
    fp.btime_n = *((ogg_int64_t*)(op->packet+28)); /* basetime numerator */
    fp.btime_d = *((ogg_int64_t*)(op->packet+36)); /* basetime denominator */
    memcpy(fp.UTC, op->packet+44, 20);

    return fp;
}

/* fills up a fisbone_packet from a fisbone ogg_packet of a skeleton bitstream */
fisbone_packet fisbone_from_ogg(ogg_packet *op) {

    fisbone_packet fp;
    
    if (memcmp(op->packet, FISBONE_IDENTIFIER, 8))
	; /* invalid value, what do we do? */
    fp.serial_no = *((ogg_uint32_t*)(op->packet+12)); /* serialno of the stream represented by this fisbone packet */
    fp.nr_header_packet = *((ogg_uint32_t*)(op->packet+16)); /* number of header packets */
    fp.granule_rate_n = *((ogg_int64_t*)(op->packet+20)); /* granulrate numerator */
    fp.granule_rate_d = *((ogg_int64_t*)(op->packet+28)); /* granulrate denominator */
    fp.start_granule = *((ogg_int64_t*)(op->packet+36)); /* start granule */
    fp.preroll = *((ogg_uint32_t*)(op->packet+44)); /* preroll, for theora its 0 */
    fp.granule_shift = *(op->packet+48); /* granule shift */
    fp.current_header_size = op->bytes - FISBONE_SIZE;
    fp.message_header_fields = _ogg_calloc(fp.current_header_size+1, sizeof(char));
    memcpy(fp.message_header_fields, op->packet+FISBONE_SIZE, fp.current_header_size);

    return fp;
}

int add_fishead_to_stream(ogg_stream_state *os, fishead_packet *fp) {

    ogg_packet op;

    op = ogg_from_fishead(fp);
    ogg_stream_packetin(os, &op);
    _ogg_free(op.packet);

    return 0;
}

int add_fisbone_to_stream(ogg_stream_state *os, fisbone_packet *fp) {

    ogg_packet op;

    op = ogg_from_fisbone(fp);
    ogg_stream_packetin(os, &op);
    _ogg_free(op.packet);

    return 0;
}

int add_eos_packet_to_stream(ogg_stream_state *os) {

    ogg_packet op;

    memset (&op, 0, sizeof(op));
    op.e_o_s = 1;
    ogg_stream_packetin(os, &op);

    return 0;
}

int flush_ogg_stream_to_file(ogg_stream_state *os, FILE *out) {

    ogg_page og;
    int result;

    while((result = ogg_stream_flush(os, &og)))
    {
        if(!result) break;
        result = write_ogg_page_to_file(&og, out);
        if(result != og.header_len + og.body_len)
            return 1;
    }

    return 0;
}
