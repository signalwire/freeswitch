" Vim syntax file
" Language:     C libdns
" Maintainer:   miekg
" Last change:  2004-12-15

" util.h
syn keyword  ldnsMacro LDNS_MALLOC
syn keyword  ldnsMacro LDNS_XMALLOC
syn keyword  ldnsMacro LDNS_REALLOC
syn keyword  ldnsMacro LDNS_XREALLOC
syn keyword  ldnsMacro LDNS_FREE
syn keyword  ldnsMacro LDNS_DEP  

" ldns/tsig.h
syn keyword  ldnsType           ldns_tsig_credentials

" ldns/rdata.h
syn keyword  ldnsType           ldns_rdf
syn keyword  ldnsType           ldns_rdf_type
syn keyword  ldnsType           ldns_hdr
syn keyword  ldnsType           ldns_status
syn keyword  ldnsType           ldns_rrset
syn keyword  ldnsType           ldns_dname
syn keyword  ldnsConstant       true
syn keyword  ldnsConstant       false
syn keyword  ldnsFunction	ldns_rdf_get_type

syn keyword  ldnsConstant	LDNS_RDF_TYPE_NONE
syn keyword  ldnsConstant	LDNS_RDF_TYPE_DNAME
syn keyword  ldnsConstant	LDNS_RDF_TYPE_INT8
syn keyword  ldnsConstant	LDNS_RDF_TYPE_INT16
syn keyword  ldnsConstant	LDNS_RDF_TYPE_INT16_DATA
syn keyword  ldnsConstant	LDNS_RDF_TYPE_INT32
syn keyword  ldnsConstant	LDNS_RDF_TYPE_A
syn keyword  ldnsConstant	LDNS_RDF_TYPE_AAAA
syn keyword  ldnsConstant	LDNS_RDF_TYPE_STR
syn keyword  ldnsConstant	LDNS_RDF_TYPE_APL
syn keyword  ldnsConstant	LDNS_RDF_TYPE_B64
syn keyword  ldnsConstant	LDNS_RDF_TYPE_HEX
syn keyword  ldnsConstant	LDNS_RDF_TYPE_NSEC
syn keyword  ldnsConstant	LDNS_RDF_TYPE_TYPE
syn keyword  ldnsConstant	LDNS_RDF_TYPE_CLASS
syn keyword  ldnsConstant	LDNS_RDF_TYPE_CERT
syn keyword  ldnsConstant	LDNS_RDF_TYPE_CERT_ALG
syn keyword  ldnsConstant	LDNS_RDF_TYPE_ALG
syn keyword  ldnsConstant 	LDNS_RDF_TYPE_UNKNOWN
syn keyword  ldnsConstant	LDNS_RDF_TYPE_TIME
syn keyword  ldnsConstant	LDNS_RDF_TYPE_PERIOD
syn keyword  ldnsConstant	LDNS_RDF_TYPE_TSIGTIME
syn keyword  ldnsConstant	LDNS_RDF_TYPE_SERVICE
syn keyword  ldnsConstant	LDNS_RDF_TYPE_LOC
syn keyword  ldnsConstant	LDNS_RDF_TYPE_WKS
syn keyword  ldnsConstant	LDNS_RDF_TYPE_NSAP
syn keyword  ldnsConstant	LDNS_RDF_TYPE_IPSECKEY
syn keyword  ldnsConstant	LDNS_RDF_TYPE_TSIG
syn keyword  ldnsConstant	LDNS_MAX_RDFLEN
syn keyword  ldnsConstant       LDNS_RDF_SIZE_BYTE             
syn keyword  ldnsConstant       LDNS_RDF_SIZE_WORD             
syn keyword  ldnsConstant       LDNS_RDF_SIZE_DOUBLEWORD       
syn keyword  ldnsConstant       LDNS_RDF_SIZE_6BYTES           
syn keyword  ldnsConstant       LDNS_RDF_SIZE_16BYTES          

" ldns/ldns.h
syn keyword  ldnsConstant	LDNS_PORT
syn keyword  ldnsConstant	LDNS_IP4ADDRLEN
syn keyword  ldnsConstant	LDNS_IP6ADDRLEN
syn keyword  ldnsConstant	LDNS_ROOT_LABEL
syn keyword  ldnsConstant	LDNS_DEFAULT_TTL

" ldns/packet.h
syn keyword  ldnsType           ldns_pkt
syn keyword  ldnsType           ldns_pkt_section
syn keyword  ldnsType		ldns_pkt_type
syn keyword  ldnsType		ldns_pkt_opcode
syn keyword  ldnsType		ldns_pkt_rcode
syn keyword  ldnsConstant	LDNS_QR
syn keyword  ldnsConstant	LDNS_AA
syn keyword  ldnsConstant	LDNS_TC
syn keyword  ldnsConstant	LDNS_CD
syn keyword  ldnsConstant	LDNS_RA
syn keyword  ldnsConstant	LDNS_AD
syn keyword  ldnsConstant	LDNS_PACKET_QUESTION
syn keyword  ldnsConstant	LDNS_PACKET_REFERRAL
syn keyword  ldnsConstant	LDNS_PACKET_ANSWER
syn keyword  ldnsConstant	LDNS_PACKET_NXDOMAIN
syn keyword  ldnsConstant	LDNS_PACKET_NODATA
syn keyword  ldnsConstant	LDNS_PACKET_UNKNOWN
syn keyword  ldnsConstant	LDNS_SECTION_QUESTION
syn keyword  ldnsConstant	LDNS_SECTION_ANSWER
syn keyword  ldnsConstant	LDNS_SECTION_AUTHORITY
syn keyword  ldnsConstant	LDNS_SECTION_ADDITIONAL
syn keyword  ldnsConstant	LDNS_SECTION_ANY
syn keyword  ldnsConstant	LDNS_SECTION_ANY_NOQUESTION
syn keyword  ldnsConstant	LDNS_MAX_PACKETLEN
syn keyword  ldnsConstant	LDNS_PACKET_QUERY
syn keyword  ldnsConstant	LDNS_PACKET_IQUERY
syn keyword  ldnsConstant	LDNS_PACKET_STATUS
syn keyword  ldnsConstant	LDNS_PACKET_NOTIFY
syn keyword  ldnsConstant	LDNS_PACKET_UPDATE

syn keyword  ldnsConstant       LDNS_RCODE_NOERROR
syn keyword  ldnsConstant       LDNS_RCODE_FORMERR
syn keyword  ldnsConstant       LDNS_RCODE_SERVFAIL
syn keyword  ldnsConstant       LDNS_RCODE_NXDOMAIN
syn keyword  ldnsConstant       LDNS_RCODE_NOTIMPL
syn keyword  ldnsConstant       LDNS_RCODE_REFUSED
syn keyword  ldnsConstant       LDNS_RCODE_YXDOMAIN 
syn keyword  ldnsConstant       LDNS_RCODE_YXRRSET
syn keyword  ldnsConstant       LDNS_RCODE_NXRRSET
syn keyword  ldnsConstant       LDNS_RCODE_NOTAUTH
syn keyword  ldnsConstant       LDNS_RCODE_NOTZONE

" dns/error.h
syn keyword ldnsMacro	LDNS_STATUS_OK
syn keyword ldnsMacro	LDNS_STATUS_EMPTY_LABEL
syn keyword ldnsMacro	LDNS_STATUS_LABEL_OVERFLOW
syn keyword ldnsMacro	LDNS_STATUS_LABEL_UNDERFLOW
syn keyword ldnsMacro	LDNS_STATUS_DOMAINNAME_OVERFLOW
syn keyword ldnsMacro	LDNS_STATUS_DOMAINNAME_UNDERFLOW
syn keyword ldnsMacro	LDNS_STATUS_DDD_OVERFLOW
syn keyword ldnsMacro	LDNS_STATUS_PACKET_OVERFLOW
syn keyword ldnsMacro	LDNS_STATUS_MEM_ERR
syn keyword ldnsMacro	LDNS_STATUS_INTERNAL_ERR
syn keyword ldnsMacro	LDNS_STATUS_ERR
syn keyword ldnsMacro	LDNS_STATUS_ADDRESS_ERR
syn keyword ldnsMacro	LDNS_STATUS_NETWORK_ERR
syn keyword ldnsMacro	LDNS_STATUS_NO_NAMESERVERS_ERR
syn keyword ldnsMacro	LDNS_STATUS_INVALID_POINTER
syn keyword ldnsMacro	LDNS_STATUS_INVALID_INT
syn keyword ldnsMacro	LDNS_STATUS_INVALID_IP4
syn keyword ldnsMacro	LDNS_STATUS_INVALID_IP6
syn keyword ldnsMacro	LDNS_STATUS_INVALID_STR
syn keyword ldnsMacro	LDNS_STATUS_INVALID_B64
syn keyword ldnsMacro	LDNS_STATUS_INVALID_HEX
syn keyword ldnsMacro	LDNS_STATUS_UNKNOWN_INET
syn keyword ldnsMacro	LDNS_STATUS_NOT_IMPL
syn keyword ldnsMacro	LDNS_STATUS_CRYPTO_UNKNOWN_ALGO
syn keyword ldnsMacro	LDNS_STATUS_CRYPTO_VALIDATED
syn keyword ldnsMacro	LDNS_STATUS_CRYPTO_BOGUS
syn keyword ldnsMacro	LDNS_STATUS_INVALID_INT
syn keyword ldnsMacro	LDNS_STATUS_INVALID_TIME
syn keyword ldnsMacro	LDNS_STATUS_NETWORK_ERR
syn keyword ldnsMacro	LDNS_STATUS_ADDRESS_ERR
syn keyword ldnsMacro 	LDNS_STATUS_CRYPTO_NO_RRSIG
syn keyword ldnsMacro 	LDNS_STATUS_NULL
syn keyword ldnsMacro 	LDNS_STATUS_CRYPTO_ALGO_NOT_IMPL
syn keyword ldnsMacro 	LDNS_STATUS_CRYPTO_NO_DNSKEY
syn keyword ldnsMacro 	LDNS_STATUS_CRYPTO_NO_TRUSTED_DNSKEY
syn keyword ldnsMacro 	LDNS_STATUS_CRYPTO_NO_MATCHING_KEYTAG_DNSKEY
syn keyword ldnsMacro 	LDNS_STATUS_CRYPTO_SIG_EXPIRED
syn keyword ldnsMacro 	LDNS_STATUS_CRYPTO_SIG_NOT_INCEPTED
syn keyword ldnsMacro 	LDNS_STATUS_CRYPTO_TSIG_ERR
syn keyword ldnsMacro 	LDNS_STATUS_CRYPTO_TYPE_COVERED_ERR
syn keyword ldnsMacro 	LDNS_STATUS_CRYPTO_TSIG_BOGUS
syn keyword ldnsMacro 	LDNS_STATUS_CRYPTO_EXPIRATION_BEFORE_INCEPTION
syn keyword ldnsMacro   LDNS_STATUS_CRYPTO_TSIG_ERR
syn keyword ldnsMacro   LDNS_STATUS_RES_NO_NS 
syn keyword ldnsMacro   LDNS_STATUS_RES_QUERY
syn keyword ldnsMacro   LDNS_STATUS_WIRE_INCOMPLETE_HEADER
syn keyword ldnsMacro   LDNS_STATUS_WIRE_INCOMPLETE_QUESTION
syn keyword ldnsMacro   LDNS_STATUS_WIRE_INCOMPLETE_ANSWER
syn keyword ldnsMacro   LDNS_STATUS_WIRE_INCOMPLETE_AUTHORITY
syn keyword ldnsMacro   LDNS_STATUS_WIRE_INCOMPLETE_ADDITIONAL
syn keyword ldnsMacro   LDNS_STATUS_NO_DATA
syn keyword ldnsMacro   LDNS_STATUS_CERT_BAD_ALGORITHM

" ldns/resolver.h
syn keyword  ldnsType	  	ldns_resolver

" ldns/zone.h
syn keyword  ldnsType	  	ldns_zone

" ldns/rr.h 
syn keyword  ldnsType	  	ldns_rr_list 
syn keyword  ldnsType           ldns_rr_descriptor
syn keyword  ldnsType           ldns_rr
syn keyword  ldnsType           ldns_rr_type
syn keyword  ldnsType           ldns_rr_class
syn keyword  ldnsType		ldns_rr_compress

syn keyword  ldnsConstant	LDNS_RR_CLASS_IN
syn keyword  ldnsConstant	LDNS_RR_CLASS_CH
syn keyword  ldnsConstant	LDNS_RR_CLASS_HS  
syn keyword  ldnsConstant	LDNS_RR_CLASS_NONE
syn keyword  ldnsConstant	LDNS_RR_CLASS_ANY 

syn keyword  ldnsConstant LDNS_RR_TYPE_A          
syn keyword  ldnsConstant LDNS_RR_TYPE_NS        
syn keyword  ldnsConstant LDNS_RR_TYPE_MD       
syn keyword  ldnsConstant LDNS_RR_TYPE_MF         
syn keyword  ldnsConstant LDNS_RR_TYPE_CNAME     
syn keyword  ldnsConstant LDNS_RR_TYPE_SOA       
syn keyword  ldnsConstant LDNS_RR_TYPE_MB         
syn keyword  ldnsConstant LDNS_RR_TYPE_MG         
syn keyword  ldnsConstant LDNS_RR_TYPE_MR       
syn keyword  ldnsConstant LDNS_RR_TYPE_NULL       
syn keyword  ldnsConstant LDNS_RR_TYPE_WKS        
syn keyword  ldnsConstant LDNS_RR_TYPE_PTR        
syn keyword  ldnsConstant LDNS_RR_TYPE_HINFO      
syn keyword  ldnsConstant LDNS_RR_TYPE_MINFO      
syn keyword  ldnsConstant LDNS_RR_TYPE_MX         
syn keyword  ldnsConstant LDNS_RR_TYPE_TXT        
syn keyword  ldnsConstant LDNS_RR_TYPE_RP         
syn keyword  ldnsConstant LDNS_RR_TYPE_AFSDB      
syn keyword  ldnsConstant LDNS_RR_TYPE_X25        
syn keyword  ldnsConstant LDNS_RR_TYPE_ISDN       
syn keyword  ldnsConstant LDNS_RR_TYPE_RT         
syn keyword  ldnsConstant LDNS_RR_TYPE_NSAP       
syn keyword  ldnsConstant LDNS_RR_TYPE_SIG        
syn keyword  ldnsConstant LDNS_RR_TYPE_KEY        
syn keyword  ldnsConstant LDNS_RR_TYPE_PX         
syn keyword  ldnsConstant LDNS_RR_TYPE_GPOS
syn keyword  ldnsConstant LDNS_RR_TYPE_AAAA       
syn keyword  ldnsConstant LDNS_RR_TYPE_LOC        
syn keyword  ldnsConstant LDNS_RR_TYPE_NXT        
syn keyword  ldnsConstant LDNS_RR_TYPE_SRV        
syn keyword  ldnsConstant LDNS_RR_TYPE_NAPTR      
syn keyword  ldnsConstant LDNS_RR_TYPE_KX         
syn keyword  ldnsConstant LDNS_RR_TYPE_CERT       
syn keyword  ldnsConstant LDNS_RR_TYPE_DNAME      
syn keyword  ldnsConstant LDNS_RR_TYPE_OPT        
syn keyword  ldnsConstant LDNS_RR_TYPE_APL        
syn keyword  ldnsConstant LDNS_RR_TYPE_DS         
syn keyword  ldnsConstant LDNS_RR_TYPE_SSHFP      
syn keyword  ldnsConstant LDNS_RR_TYPE_RRSIG      
syn keyword  ldnsConstant LDNS_RR_TYPE_NSEC       
syn keyword  ldnsConstant LDNS_RR_TYPE_DNSKEY     
syn keyword  ldnsConstant LDNS_RR_TYPE_EID
syn keyword  ldnsConstant LDNS_RR_TYPE_NIMLOC
syn keyword  ldnsConstant LDNS_RR_TYPE_ATMA
syn keyword  ldnsConstant LDNS_RR_TYPE_A6
syn keyword  ldnsConstant LDNS_RR_TYPE_SINK
syn keyword  ldnsConstant LDNS_RR_TYPE_IPSECKEY
syn keyword  ldnsConstant LDNS_RR_TYPE_UINFO
syn keyword  ldnsConstant LDNS_RR_TYPE_UID
syn keyword  ldnsConstant LDNS_RR_TYPE_GID
syn keyword  ldnsConstant LDNS_RR_TYPE_UNSPEC
syn keyword  ldnsConstant LDNS_RR_TYPE_TSIG       
syn keyword  ldnsConstant LDNS_RR_TYPE_IXFR       
syn keyword  ldnsConstant LDNS_RR_TYPE_AXFR       
syn keyword  ldnsConstant LDNS_RR_TYPE_MAILB      
syn keyword  ldnsConstant LDNS_RR_TYPE_MAILA      
syn keyword  ldnsConstant LDNS_RR_TYPE_ANY        
syn keyword  ldnsConstant LDNS_MAX_LABELLEN     
syn keyword  ldnsConstant LDNS_MAX_DOMAINLEN
syn keyword  ldnsConstant LDNS_RR_COMPRESS
syn keyword  ldnsConstant LDNS_RR_NO_COMPRESS

syn keyword  ldnsMacro	QHEADERSZ
syn keyword  ldnsMacro	RD_MASK
syn keyword  ldnsMacro	RD_SHIFT
syn keyword  ldnsMacro	LDNS_RD
syn keyword  ldnsMacro	RD_SET
syn keyword  ldnsMacro	RD_CLR
syn keyword  ldnsMacro  TC_MASK
syn keyword  ldnsMacro  TC_SHIFT
syn keyword  ldnsMacro	LDNS_TC
syn keyword  ldnsMacro	TC_SET
syn keyword  ldnsMacro	TC_CLR
syn keyword  ldnsMacro	AA_MASK
syn keyword  ldnsMacro	AA_SHIFT
syn keyword  ldnsMacro	LDNS_AA
syn keyword  ldnsMacro	AA_SET
syn keyword  ldnsMacro	AA_CLR
syn keyword  ldnsMacro	OPCODE_MASK
syn keyword  ldnsMacro	OPCODE_SHIFT
syn keyword  ldnsMacro	OPCODE
syn keyword  ldnsMacro	OPCODE_SET
syn keyword  ldnsMacro	QR_MASK
syn keyword  ldnsMacro	QR_SHIFT
syn keyword  ldnsMacro	LDNS_QR
syn keyword  ldnsMacro	QR_SET
syn keyword  ldnsMacro	QR_CLR
syn keyword  ldnsMacro	RCODE_MASK
syn keyword  ldnsMacro	RCODE_SHIFT
syn keyword  ldnsMacro	RCODE
syn keyword  ldnsMacro	RCODE_SET
syn keyword  ldnsMacro	CD_MASK
syn keyword  ldnsMacro	CD_SHIFT
syn keyword  ldnsMacro	LDNS_CD
syn keyword  ldnsMacro	CD_SET
syn keyword  ldnsMacro	CD_CLR
syn keyword  ldnsMacro	AD_MASK
syn keyword  ldnsMacro	AD_SHIFT
syn keyword  ldnsMacro	LDNS_AD
syn keyword  ldnsMacro	AD_SET
syn keyword  ldnsMacro	AD_CLR
syn keyword  ldnsMacro	Z_MASK
syn keyword  ldnsMacro	Z_SHIFT
syn keyword  ldnsMacro	LDNS_Z
syn keyword  ldnsMacro	Z_SET
syn keyword  ldnsMacro	Z_CLR
syn keyword  ldnsMacro	RA_MASK
syn keyword  ldnsMacro	RA_SHIFT
syn keyword  ldnsMacro	LDNS_RA
syn keyword  ldnsMacro	RA_SET
syn keyword  ldnsMacro	RA_CLR
syn keyword  ldnsMacro	LDNS_ID
syn keyword  ldnsMacro  QDCOUNT_OFF
syn keyword  ldnsMacro	QDCOUNT
syn keyword  ldnsMacro  ANCOUNT_OFF
syn keyword  ldnsMacro	ANCOUNT
syn keyword  ldnsMacro  NSCOUNT_OFF
syn keyword  ldnsMacro	NSCOUNT
syn keyword  ldnsMacro  ARCOUNT_OFF
syn keyword  ldnsMacro 	ARCOUNT

" ldns/buffer.h
syn keyword  ldnsType		ldns_buffer
syn keyword  ldnsConstant	LDNS_MIN_BUFLEN

" ldns/host2str.h
syn keyword  ldnsType	ldns_lookup_table
syn keyword  ldnsConstant LDNS_APL_IP4
syn keyword  ldnsConstant LDNS_APL_IP6

" ldns/keys.h
syn keyword  ldnsType   ldns_key
syn keyword  ldnsType   ldns_key_list
syn keyword  ldnsType   ldns_signing_algorithm
syn keyword  ldnsType   ldns_hash

" ldns/dnssec.h
syn keyword  ldnsConstant	LDNS_MAX_KEYLEN

" Default highlighting
command -nargs=+ HiLink hi def link <args>
HiLink ldnsType                Type
HiLink ldnsFunction            Function
HiLink ldnsMacro               Macro
HiLink ldnsConstant            Constant
delcommand HiLink
