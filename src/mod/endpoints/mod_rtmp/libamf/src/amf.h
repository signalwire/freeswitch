#ifndef __AMF_H__
#define __AMF_H__

#include <switch.h>

/* AMF number */
typedef double number64_t;

/* custom read/write function type */
typedef size_t (*read_proc_t)(void * out_buffer, size_t size, void * user_data);
typedef size_t (*write_proc_t)(const void * in_buffer, size_t size, void * user_data);

#endif /* __AMF_H__ */
