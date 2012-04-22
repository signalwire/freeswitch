/**
 * host2str.h -  txt presentation of RRs
 *
 * a Net::DNS like library for C
 *
 * (c) NLnet Labs, 2005-2006
 *
 * See the file LICENSE for the license
 */

/**
 * \file
 *
 * Contains functions to translate the main structures to their text
 * representation, as well as functions to print them.
 */

#ifndef LDNS_HOST2STR_H
#define LDNS_HOST2STR_H

#include <ldns/common.h>
#include <ldns/error.h>
#include <ldns/rr.h>
#include <ldns/rdata.h>
#include <ldns/packet.h>
#include <ldns/buffer.h>
#include <ldns/resolver.h>
#include <ldns/zone.h>
#include <ctype.h>

#include "ldns/util.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LDNS_APL_IP4            1
#define LDNS_APL_IP6            2
#define LDNS_APL_MASK           0x7f
#define LDNS_APL_NEGATION       0x80

/**
 * Converts an ldns packet opcode value to its mnemonic, and adds that
 * to the output buffer
 * \param[in] *output the buffer to add the data to
 * \param[in] opcode to find the string representation of
 * \return LDNS_STATUS_OK on success, or a buffer failure mode on error
 */
ldns_status
ldns_pkt_opcode2buffer_str(ldns_buffer *output, ldns_pkt_opcode opcode);

/**
 * Converts an ldns packet rcode value to its mnemonic, and adds that
 * to the output buffer
 * \param[in] *output the buffer to add the data to
 * \param[in] rcode to find the string representation of
 * \return LDNS_STATUS_OK on success, or a buffer failure mode on error
 */
ldns_status
ldns_pkt_rcode2buffer_str(ldns_buffer *output, ldns_pkt_rcode rcode);

/**
 * Converts an ldns algorithm type to its mnemonic, and adds that
 * to the output buffer
 * \param[in] *output the buffer to add the data to
 * \param[in] algorithm to find the string representation of
 * \return LDNS_STATUS_OK on success, or a buffer failure mode on error
 */
ldns_status
ldns_algorithm2buffer_str(ldns_buffer *output,
                          ldns_algorithm algorithm);

/**
 * Converts an ldns certificate algorithm type to its mnemonic, 
 * and adds that to the output buffer
 * \param[in] *output the buffer to add the data to
 * \param[in] cert_algorithm to find the string representation of
 * \return LDNS_STATUS_OK on success, or a buffer failure mode on error
 */
ldns_status
ldns_cert_algorithm2buffer_str(ldns_buffer *output,
                               ldns_cert_algorithm cert_algorithm);


/**
 * Converts a packet opcode to its mnemonic and returns that as
 * an allocated null-terminated string.
 * Remember to free it.
 *
 * \param[in] opcode the opcode to convert to text
 * \return null terminated char * data, or NULL on error
 */
char *ldns_pkt_opcode2str(ldns_pkt_opcode opcode);

/**
 * Converts a packet rcode to its mnemonic and returns that as
 * an allocated null-terminated string.
 * Remember to free it.
 *
 * \param[in] rcode the rcode to convert to text
 * \return null terminated char * data, or NULL on error
 */
char *ldns_pkt_rcode2str(ldns_pkt_rcode rcode);

/**
 * Converts a signing algorithms to its mnemonic and returns that as
 * an allocated null-terminated string.
 * Remember to free it.
 *
 * \param[in] algorithm the algorithm to convert to text
 * \return null terminated char * data, or NULL on error
 */
char *ldns_pkt_algorithm2str(ldns_algorithm algorithm);

/**
 * Converts a cert algorithm to its mnemonic and returns that as
 * an allocated null-terminated string.
 * Remember to free it.
 *
 * \param[in] cert_algorithm to convert to text
 * \return null terminated char * data, or NULL on error
 */
char *ldns_pkt_cert_algorithm2str(ldns_cert_algorithm cert_algorithm);

/** 
 * Converts an LDNS_RDF_TYPE_A rdata element to string format and adds it to the output buffer 
 * \param[in] *rdf The rdata to convert
 * \param[in] *output The buffer to add the data to
 * \return LDNS_STATUS_OK on success, and error status on failure
 */
ldns_status ldns_rdf2buffer_str_a(ldns_buffer *output, const ldns_rdf *rdf);

/** 
 * Converts an LDNS_RDF_TYPE_AAAA rdata element to string format and adds it to the output buffer 
 * \param[in] *rdf The rdata to convert
 * \param[in] *output The buffer to add the data to
 * \return LDNS_STATUS_OK on success, and error status on failure
 */
ldns_status ldns_rdf2buffer_str_aaaa(ldns_buffer *output, const ldns_rdf *rdf);

/** 
 * Converts an LDNS_RDF_TYPE_STR rdata element to string format and adds it to the output buffer 
 * \param[in] *rdf The rdata to convert
 * \param[in] *output The buffer to add the data to
 * \return LDNS_STATUS_OK on success, and error status on failure
 */
ldns_status ldns_rdf2buffer_str_str(ldns_buffer *output, const ldns_rdf *rdf);

/** 
 * Converts an LDNS_RDF_TYPE_B64 rdata element to string format and adds it to the output buffer 
 * \param[in] *rdf The rdata to convert
 * \param[in] *output The buffer to add the data to
 * \return LDNS_STATUS_OK on success, and error status on failure
 */
ldns_status ldns_rdf2buffer_str_b64(ldns_buffer *output, const ldns_rdf *rdf);

/** 
 * Converts an LDNS_RDF_TYPE_B32_EXT rdata element to string format and adds it to the output buffer 
 * \param[in] *rdf The rdata to convert
 * \param[in] *output The buffer to add the data to
 * \return LDNS_STATUS_OK on success, and error status on failure
 */
ldns_status ldns_rdf2buffer_str_b32_ext(ldns_buffer *output, const ldns_rdf *rdf);

/** 
 * Converts an LDNS_RDF_TYPE_HEX rdata element to string format and adds it to the output buffer 
 * \param[in] *rdf The rdata to convert
 * \param[in] *output The buffer to add the data to
 * \return LDNS_STATUS_OK on success, and error status on failure
 */
ldns_status ldns_rdf2buffer_str_hex(ldns_buffer *output, const ldns_rdf *rdf);

/** 
 * Converts an LDNS_RDF_TYPE_TYPE rdata element to string format and adds it to the output buffer 
 * \param[in] *rdf The rdata to convert
 * \param[in] *output The buffer to add the data to
 * \return LDNS_STATUS_OK on success, and error status on failure
 */
ldns_status ldns_rdf2buffer_str_type(ldns_buffer *output, const ldns_rdf *rdf);

/** 
 * Converts an LDNS_RDF_TYPE_CLASS rdata element to string format and adds it to the output buffer 
 * \param[in] *rdf The rdata to convert
 * \param[in] *output The buffer to add the data to
 * \return LDNS_STATUS_OK on success, and error status on failure
 */
ldns_status ldns_rdf2buffer_str_class(ldns_buffer *output, const ldns_rdf *rdf);

/** 
 * Converts an LDNS_RDF_TYPE_ALG rdata element to string format and adds it to the output buffer 
 * \param[in] *rdf The rdata to convert
 * \param[in] *output The buffer to add the data to
 * \return LDNS_STATUS_OK on success, and error status on failure
 */
ldns_status ldns_rdf2buffer_str_alg(ldns_buffer *output, const ldns_rdf *rdf);

/**
 * Converts an ldns_rr_type value to its string representation,
 * and places it in the given buffer
 * \param[in] *output The buffer to add the data to
 * \param[in] type the ldns_rr_type to convert
 * \return LDNS_STATUS_OK on success, and error status on failure
 */
ldns_status ldns_rr_type2buffer_str(ldns_buffer *output,
                                    const ldns_rr_type type);

/**
 * Converts an ldns_rr_type value to its string representation,
 * and returns that string. For unknown types, the string
 * "TYPE<id>" is returned. This function allocates data that must be
 * freed by the caller
 * \param[in] type the ldns_rr_type to convert
 * \return a newly allocated string
 */
char *ldns_rr_type2str(const ldns_rr_type type);

/**
 * Converts an ldns_rr_class value to its string representation,
 * and places it in the given buffer
 * \param[in] *output The buffer to add the data to
 * \param[in] klass the ldns_rr_class to convert
 * \return LDNS_STATUS_OK on success, and error status on failure
 */
ldns_status ldns_rr_class2buffer_str(ldns_buffer *output,
                                     const ldns_rr_class klass);

/**
 * Converts an ldns_rr_class value to its string representation,
 * and returns that string. For unknown types, the string
 * "CLASS<id>" is returned. This function allocates data that must be
 * freed by the caller
 * \param[in] klass the ldns_rr_class to convert
 * \return a newly allocated string
 */
char *ldns_rr_class2str(const ldns_rr_class klass);


/** 
 * Converts an LDNS_RDF_TYPE_CERT rdata element to string format and adds it to the output buffer 
 * \param[in] *rdf The rdata to convert
 * \param[in] *output The buffer to add the data to
 * \return LDNS_STATUS_OK on success, and error status on failure
 */
ldns_status ldns_rdf2buffer_str_cert_alg(ldns_buffer *output, const ldns_rdf *rdf);

/** 
 * Converts an LDNS_RDF_TYPE_LOC rdata element to string format and adds it to the output buffer 
 * \param[in] *rdf The rdata to convert
 * \param[in] *output The buffer to add the data to
 * \return LDNS_STATUS_OK on success, and error status on failure
 */
ldns_status ldns_rdf2buffer_str_loc(ldns_buffer *output, const ldns_rdf *rdf);

/** 
 * Converts an LDNS_RDF_TYPE_UNKNOWN rdata element to string format and adds it to the output buffer 
 * \param[in] *rdf The rdata to convert
 * \param[in] *output The buffer to add the data to
 * \return LDNS_STATUS_OK on success, and error status on failure
 */
ldns_status ldns_rdf2buffer_str_unknown(ldns_buffer *output, const ldns_rdf *rdf);

/** 
 * Converts an LDNS_RDF_TYPE_NSAP rdata element to string format and adds it to the output buffer 
 * \param[in] *rdf The rdata to convert
 * \param[in] *output The buffer to add the data to
 * \return LDNS_STATUS_OK on success, and error status on failure
 */
ldns_status ldns_rdf2buffer_str_nsap(ldns_buffer *output, const ldns_rdf *rdf);

/** 
 * Converts an LDNS_RDF_TYPE_ATMA rdata element to string format and adds it to the output buffer 
 * \param[in] *rdf The rdata to convert
 * \param[in] *output The buffer to add the data to
 * \return LDNS_STATUS_OK on success, and error status on failure
 */
ldns_status ldns_rdf2buffer_str_atma(ldns_buffer *output, const ldns_rdf *rdf);

/** 
 * Converts an LDNS_RDF_TYPE_WKS rdata element to string format and adds it to the output buffer 
 * \param[in] *rdf The rdata to convert
 * \param[in] *output The buffer to add the data to
 * \return LDNS_STATUS_OK on success, and error status on failure
 */
ldns_status ldns_rdf2buffer_str_wks(ldns_buffer *output, const ldns_rdf *rdf);

/** 
 * Converts an LDNS_RDF_TYPE_NSEC rdata element to string format and adds it to the output buffer 
 * \param[in] *rdf The rdata to convert
 * \param[in] *output The buffer to add the data to
 * \return LDNS_STATUS_OK on success, and error status on failure
 */
ldns_status ldns_rdf2buffer_str_nsec(ldns_buffer *output, const ldns_rdf *rdf);

/** 
 * Converts an LDNS_RDF_TYPE_PERIOD rdata element to string format and adds it to the output buffer 
 * \param[in] *rdf The rdata to convert
 * \param[in] *output The buffer to add the data to
 * \return LDNS_STATUS_OK on success, and error status on failure
 */
ldns_status ldns_rdf2buffer_str_period(ldns_buffer *output, const ldns_rdf *rdf);

/** 
 * Converts an LDNS_RDF_TYPE_TSIGTIME rdata element to string format and adds it to the output buffer 
 * \param[in] *rdf The rdata to convert
 * \param[in] *output The buffer to add the data to
 * \return LDNS_STATUS_OK on success, and error status on failure
 */
ldns_status ldns_rdf2buffer_str_tsigtime(ldns_buffer *output, const ldns_rdf *rdf);

/** 
 * Converts an LDNS_RDF_TYPE_APL rdata element to string format and adds it to the output buffer 
 * \param[in] *rdf The rdata to convert
 * \param[in] *output The buffer to add the data to
 * \return LDNS_STATUS_OK on success, and error status on failure
 */
ldns_status ldns_rdf2buffer_str_apl(ldns_buffer *output, const ldns_rdf *rdf);

/** 
 * Converts an LDNS_RDF_TYPE_INT16_DATA rdata element to string format and adds it to the output buffer 
 * \param[in] *rdf The rdata to convert
 * \param[in] *output The buffer to add the data to
 * \return LDNS_STATUS_OK on success, and error status on failure
 */
ldns_status ldns_rdf2buffer_str_int16_data(ldns_buffer *output, const ldns_rdf *rdf);

/** 
 * Converts an LDNS_RDF_TYPE_IPSECKEY rdata element to string format and adds it to the output buffer 
 * \param[in] *rdf The rdata to convert
 * \param[in] *output The buffer to add the data to
 * \return LDNS_STATUS_OK on success, and error status on failure
 */
ldns_status ldns_rdf2buffer_str_ipseckey(ldns_buffer *output, const ldns_rdf *rdf);

/** 
 * Converts an LDNS_RDF_TYPE_TSIG rdata element to string format and adds it to the output buffer 
 * \param[in] *rdf The rdata to convert
 * \param[in] *output The buffer to add the data to
 * \return LDNS_STATUS_OK on success, and error status on failure
 */
ldns_status ldns_rdf2buffer_str_tsig(ldns_buffer *output, const ldns_rdf *rdf);


/**
 * Converts the data in the rdata field to presentation
 * format (as char *) and appends it to the given buffer
 *
 * \param[in] output pointer to the buffer to append the data to
 * \param[in] rdf the pointer to the rdafa field containing the data
 * \return status
 */
ldns_status ldns_rdf2buffer_str(ldns_buffer *output, const ldns_rdf *rdf);

/**
 * Converts the data in the resource record to presentation
 * format (as char *) and appends it to the given buffer
 *
 * \param[in] output pointer to the buffer to append the data to
 * \param[in] rr the pointer to the rr field to convert
 * \return status
 */
ldns_status ldns_rr2buffer_str(ldns_buffer *output, const ldns_rr *rr);

/**
 * Converts the data in the DNS packet to presentation
 * format (as char *) and appends it to the given buffer
 *
 * \param[in] output pointer to the buffer to append the data to
 * \param[in] pkt the pointer to the packet to convert
 * \return status
 */
ldns_status ldns_pkt2buffer_str(ldns_buffer *output, const ldns_pkt *pkt);

/** 
 * Converts an LDNS_RDF_TYPE_NSEC3_SALT rdata element to string format and adds it to the output buffer 
 * \param[in] *rdf The rdata to convert
 * \param[in] *output The buffer to add the data to
 * \return LDNS_STATUS_OK on success, and error status on failure
 */
ldns_status ldns_rdf2buffer_str_nsec3_salt(ldns_buffer *output, const ldns_rdf *rdf);


/**
 * Converts the data in the DNS packet to presentation
 * format (as char *) and appends it to the given buffer
 *
 * \param[in] output pointer to the buffer to append the data to
 * \param[in] k the pointer to the private key to convert
 * \return status
 */
ldns_status ldns_key2buffer_str(ldns_buffer *output, const ldns_key *k);

/**
 * Converts an LDNS_RDF_TYPE_INT8 rdata element to string format and adds it to the output buffer
 * \param[in] *rdf The rdata to convert
 * \param[in] *output The buffer to add the data to
 * \return LDNS_STATUS_OK on success, and error status on failure
 */
ldns_status ldns_rdf2buffer_str_int8(ldns_buffer *output, const ldns_rdf *rdf);

/**
 * Converts an LDNS_RDF_TYPE_INT16 rdata element to string format and adds it to the output buffer
 * \param[in] *rdf The rdata to convert
 * \param[in] *output The buffer to add the data to
 * \return LDNS_STATUS_OK on success, and error status on failure
 */
ldns_status ldns_rdf2buffer_str_int16(ldns_buffer *output, const ldns_rdf *rdf);

/**
 * Converts an LDNS_RDF_TYPE_INT32 rdata element to string format and adds it to the output buffer
 * \param[in] *rdf The rdata to convert
 * \param[in] *output The buffer to add the data to
 * \return LDNS_STATUS_OK on success, and error status on failure
 */
ldns_status ldns_rdf2buffer_str_int32(ldns_buffer *output, const ldns_rdf *rdf);

/**
 * Converts an LDNS_RDF_TYPE_TIME rdata element to string format and adds it to the output buffer
 * \param[in] *rdf The rdata to convert
 * \param[in] *output The buffer to add the data to
 * \return LDNS_STATUS_OK on success, and error status on failure
 */
ldns_status ldns_rdf2buffer_str_time(ldns_buffer *output, const ldns_rdf *rdf);

/**
 * Converts the data in the rdata field to presentation format and
 * returns that as a char *.
 * Remember to free it.
 *
 * \param[in] rdf The rdata field to convert
 * \return null terminated char * data, or NULL on error
 */
char *ldns_rdf2str(const ldns_rdf *rdf);

/**
 * Converts the data in the resource record to presentation format and
 * returns that as a char *.
 * Remember to free it.
 *
 * \param[in] rr The rdata field to convert
 * \return null terminated char * data, or NULL on error
 */
char *ldns_rr2str(const ldns_rr *rr);

/**
 * Converts the data in the DNS packet to presentation format and
 * returns that as a char *.
 * Remember to free it.
 *
 * \param[in] pkt The rdata field to convert
 * \return null terminated char * data, or NULL on error
 */
char *ldns_pkt2str(const ldns_pkt *pkt);

/**
 * Converts a private key to the test presentation fmt and
 * returns that as a char *.
 * Remember to free it.
 *
 * \param[in] k the key to convert to text
 * \return null terminated char * data, or NULL on error
 */
char *ldns_key2str(const ldns_key *k);

/**
 * Converts a list of resource records to presentation format
 * and returns that as a char *.
 * Remember to free it.
 *
 * \param[in] rr_list the rr_list to convert to text
 * \return null terminated char * data, or NULL on error
 */
char *ldns_rr_list2str(const ldns_rr_list *rr_list);

/**
 * Returns the data in the buffer as a null terminated char * string
 * Buffer data must be char * type, and must be freed by the caller
 *
 * \param[in] buffer buffer containing char * data
 * \return null terminated char * data, or NULL on error
 */
char *ldns_buffer2str(ldns_buffer *buffer);

/**
 * Prints the data in the rdata field to the given file stream
 * (in presentation format)
 *
 * \param[in] output the file stream to print to
 * \param[in] rdf the rdata field to print
 * \return void
 */
void ldns_rdf_print(FILE *output, const ldns_rdf *rdf);

/**
 * Prints the data in the resource record to the given file stream
 * (in presentation format)
 *
 * \param[in] output the file stream to print to
 * \param[in] rr the resource record to print
 * \return void
 */
void ldns_rr_print(FILE *output, const ldns_rr *rr);

/**
 * Prints the data in the DNS packet to the given file stream
 * (in presentation format)
 *
 * \param[in] output the file stream to print to
 * \param[in] pkt the packet to print
 * \return void
 */
void ldns_pkt_print(FILE *output, const ldns_pkt *pkt);

/**
 * Converts a rr_list to presentation format and appends it to
 * the output buffer
 * \param[in] output the buffer to append output to
 * \param[in] list the ldns_rr_list to print
 * \return ldns_status
 */
ldns_status ldns_rr_list2buffer_str(ldns_buffer *output, const ldns_rr_list *list);

/**
 * Converts the header of a packet to presentation format and appends it to
 * the output buffer
 * \param[in] output the buffer to append output to
 * \param[in] pkt the packet to convert the header of
 * \return ldns_status
 */
ldns_status ldns_pktheader2buffer_str(ldns_buffer *output, const ldns_pkt *pkt);

/**
 * print a rr_list to output
 * param[in] output the fd to print to
 * param[in] list the rr_list to print
 */
void ldns_rr_list_print(FILE *output, const ldns_rr_list *list);

/**
 * Print a resolver (in sofar that is possible) state
 * to output.
 * \param[in] output the fd to print to
 * \param[in] r the resolver to print
 */
void ldns_resolver_print(FILE *output, const ldns_resolver *r);

/**
 * Print a zone structure * to output. Note the SOA record
 * is included in this output
 * \param[in] output the fd to print to
 * \param[in] z the zone to print
 */
void ldns_zone_print(FILE *output, const ldns_zone *z);

/**
 * Print the ldns_rdf containing a dname to the buffer
 * \param[in] output the buffer to print to
 * \param[in] dname the dname to print
 * \return ldns_status message if the printing succeeded
 */
ldns_status ldns_rdf2buffer_str_dname(ldns_buffer *output, const ldns_rdf *dname);

#ifdef __cplusplus
}
#endif

#endif /* LDNS_HOST2STR_H */
