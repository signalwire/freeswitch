#ifndef __IO_H__
#define __IO_H__

#include "amf.h"

/* structure used to mimic a stream with a memory buffer */
typedef struct __buffer_context {
    uint8_t * start_address;
    uint8_t * current_address;
    size_t buffer_size;
} buffer_context;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* callback function to mimic fread using a memory buffer */
size_t buffer_read(void * out_buffer, size_t size, void * user_data);

/* callback function to mimic fwrite using a memory buffer */
size_t buffer_write(const void * in_buffer, size_t size, void * user_data);

/* callback function to read data from a file stream */
size_t file_read(void * out_buffer, size_t size, void * user_data);

/* callback function to write data to a file stream */
size_t file_write(const void * in_buffer, size_t size, void * user_data);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __IO_H__ */
