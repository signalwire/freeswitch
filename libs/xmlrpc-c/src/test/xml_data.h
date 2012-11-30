#ifndef XML_DATA_H_INCLUDED
#define XML_DATA_H_INCLUDED

#define XML_PROLOGUE "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"

#define APACHE_URL "http://ws.apache.org/xmlrpc/namespaces/extensions"
#define XMLNS_APACHE "xmlns:ex=\"" APACHE_URL "\""

extern char const serialized_data[];

extern char const serialized_call[];

extern char const serialized_fault[];

extern char const expat_data[];
extern char const expat_error_data[];

extern char const good_response_xml[];

extern char const unparseable_value[];

extern const char *(bad_values[]);

extern const char *(bad_responses[]);

extern const char *(bad_calls[]);

#endif
