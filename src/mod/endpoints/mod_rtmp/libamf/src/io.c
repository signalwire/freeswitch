#include "io.h"

#include <stdio.h>
#include <string.h>

/* callback function to mimic fread using a memory buffer */
size_t buffer_read(void * out_buffer, size_t size, void * user_data) {
    buffer_context * ctxt = (buffer_context *)user_data;
    if (ctxt->current_address >= ctxt->start_address &&
        ctxt->current_address + size <= ctxt->start_address + ctxt->buffer_size) {

        memcpy(out_buffer, ctxt->current_address, size);
        ctxt->current_address += size;
        return size;
    }
    else {
        return 0;
    }
}

/* callback function to mimic fwrite using a memory buffer */
size_t buffer_write(const void * in_buffer, size_t size, void * user_data) {
    buffer_context * ctxt = (buffer_context *)user_data;
    if (ctxt->current_address >= ctxt->start_address &&
        ctxt->current_address + size <= ctxt->start_address + ctxt->buffer_size) {

        memcpy(ctxt->current_address, in_buffer, size);
        ctxt->current_address += size;
        return size;
    }
    else {
        return 0;
    }
}

/* callback function to read data from a file stream */
size_t file_read(void * out_buffer, size_t size, void * user_data) {
    return fread(out_buffer, sizeof(uint8_t), size, (FILE *)user_data);
}

/* callback function to write data to a file stream */
size_t file_write(const void * in_buffer, size_t size, void * user_data) {
    return fwrite(in_buffer, sizeof(uint8_t), size, (FILE *)user_data);
}
