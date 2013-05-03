#include <string.h>

#include "amf0.h"
#include "io.h"
#include "types.h"

/* function common to all array types */
static void amf0_list_init(amf0_list * list) {
    if (list != NULL) {
        list->size = 0;
        list->first_element = NULL;
        list->last_element = NULL;
    }
}

static amf0_data * amf0_list_push(amf0_list * list, amf0_data * data) {
    amf0_node * node = (amf0_node*)malloc(sizeof(amf0_node));
    if (node != NULL) {
        node->data = data;
        node->next = NULL;
        node->prev = NULL;
        if (list->size == 0) {
            list->first_element = node;
            list->last_element = node;
        }
        else {
            list->last_element->next = node;
            node->prev = list->last_element;
            list->last_element = node;
        }
        ++(list->size);
        return data;
    }
    return NULL;
}

static amf0_data * amf0_list_insert_before(amf0_list * list, amf0_node * node, amf0_data * data) {
    if (node != NULL) {
        amf0_node * new_node = (amf0_node*)malloc(sizeof(amf0_node));
        if (new_node != NULL) {
            new_node->next = node;
            new_node->prev = node->prev;

            if (node->prev != NULL) {
                node->prev->next = new_node;
                node->prev = new_node;
            }
            if (node == list->first_element) {
                list->first_element = new_node;
            }
            ++(list->size);
            new_node->data = data;
            return data;
        }
    }
    return NULL;
}

static amf0_data * amf0_list_insert_after(amf0_list * list, amf0_node * node, amf0_data * data) {
    if (node != NULL) {
        amf0_node * new_node = (amf0_node*)malloc(sizeof(amf0_node));
        if (new_node != NULL) {
            new_node->next = node->next;
            new_node->prev = node;

            if (node->next != NULL) {
                node->next->prev = new_node;
                node->next = new_node;
            }
            if (node == list->last_element) {
                list->last_element = new_node;
            }
            ++(list->size);
            new_node->data = data;
            return data;
        }
    }
    return NULL;
}

static amf0_data * amf0_list_delete(amf0_list * list, amf0_node * node) {
    amf0_data * data = NULL;
    if (node != NULL) {
        if (node->next != NULL) {
            node->next->prev = node->prev;
        }
        if (node->prev != NULL) {
            node->prev->next = node->next;
        }
        if (node == list->first_element) {
            list->first_element = node->next;
        }
        if (node == list->last_element) {
            list->last_element = node->prev;
        }
        data = node->data;
        free(node);
        --(list->size);
    }
    return data;
}

static amf0_data * amf0_list_get_at(amf0_list * list, uint32_t n) {
    if (n < list->size) {
        uint32_t i;
        amf0_node * node = list->first_element;
        for (i = 0; i < n; ++i) {
            node = node->next;
        }
        return node->data;
    }
    return NULL;
}

static amf0_data * amf0_list_pop(amf0_list * list) {
    return amf0_list_delete(list, list->last_element);
}

static amf0_node * amf0_list_first(amf0_list * list) {
    return list->first_element;
}

static amf0_node * amf0_list_last(amf0_list * list) {
    return list->last_element;
}

static void amf0_list_clear(amf0_list * list) {
    amf0_node * tmp;
    amf0_node * node = list->first_element;
    while (node != NULL) {
        amf0_data_free(node->data);
        tmp = node;
        node = node->next;
        free(tmp);
    }
    list->size = 0;
}

static amf0_list * amf0_list_clone(amf0_list * list, amf0_list * out_list) {
    amf0_node * node;
    node = list->first_element;
    while (node != NULL) {
        amf0_list_push(out_list, amf0_data_clone(node->data));
        node = node->next;
    }
    return out_list;
}

/* allocate an AMF data object */
amf0_data * amf0_data_new(uint8_t type) {
    amf0_data * data = (amf0_data*)malloc(sizeof(amf0_data));
    if (data != NULL) {
        data->type = type;
    }
    return data;
}

/* read AMF data from buffer */
amf0_data * amf0_data_buffer_read(uint8_t * buffer, size_t maxbytes) {
    buffer_context ctxt;
    ctxt.start_address = ctxt.current_address = buffer;
    ctxt.buffer_size = maxbytes;
    return amf0_data_read(buffer_read, &ctxt);
}

/* write AMF data to buffer */
size_t amf0_data_buffer_write(amf0_data * data, uint8_t * buffer, size_t maxbytes) {
    buffer_context ctxt;
    ctxt.start_address = ctxt.current_address = buffer;
    ctxt.buffer_size = maxbytes;
    return amf0_data_write(data, buffer_write, &ctxt);
}

/* load AMF data from a file stream */
amf0_data * amf0_data_file_read(FILE * stream) {
    return amf0_data_read(file_read, stream);
}

/* write AMF data into a file stream */
size_t amf0_data_file_write(amf0_data * data, FILE * stream) {
    return amf0_data_write(data, file_write, stream);
}

/* read a number */
static amf0_data * amf0_number_read(read_proc_t read_proc, void * user_data) {
    number64_t val;
    if (read_proc(&val, sizeof(number64_t), user_data) == sizeof(number64_t)) {
        return amf0_number_new(swap_number64(val));
    }
    return NULL;
}

/* read a boolean */
static amf0_data * amf0_boolean_read(read_proc_t read_proc, void * user_data) {
    uint8_t val;
    if (read_proc(&val, sizeof(uint8_t), user_data) == sizeof(uint8_t)) {
        return amf0_boolean_new(val);
    }
    return NULL;
}

/* read a string */
static amf0_data * amf0_string_read(read_proc_t read_proc, void * user_data) {
  uint16_t strsize;
  uint8_t * buffer = NULL;
  amf0_data *data = NULL;
    if (read_proc(&strsize, sizeof(uint16_t), user_data) == sizeof(uint16_t)) {
        strsize = swap_uint16(strsize);
        if (strsize > 0) {
	  buffer = (uint8_t*) calloc(strsize, sizeof(uint8_t));
	  if ( buffer == NULL ) {
	    return NULL; // Memory error
	  }
	  if ( read_proc(buffer, strsize, user_data) == strsize ) {
	    data = amf0_string_new(buffer, strsize);
	  }
	  free(buffer);
	  buffer = NULL;
	  return data;
        }
        else {
            return amf0_string_new(NULL, 0);
        }
    }
    return NULL;
}

/* read an object */
static amf0_data * amf0_object_read(read_proc_t read_proc, void * user_data) {
    amf0_data * data = amf0_object_new();
    if (data != NULL) {
        amf0_data * name;
        amf0_data * element;
        while (1) {
            name = amf0_string_read(read_proc, user_data);
            if (name != NULL) {
                element = amf0_data_read(read_proc, user_data);
                if (element != NULL) {
                    if (amf0_object_add(data, (char *)amf0_string_get_uint8_ts(name), element) == NULL) {
                        amf0_data_free(name);
                        amf0_data_free(element);
                        amf0_data_free(data);
                        return NULL;
                    }
                }
                else {
                    amf0_data_free(name);
                    break;
                }
            }
            else {
                /* invalid name: error */
                amf0_data_free(data);
                return NULL;
            }
        }
    }
    return data;
}

/* read an associative array */
static amf0_data * amf0_associative_array_read(read_proc_t read_proc, void * user_data) {
    amf0_data * data = amf0_associative_array_new();
    if (data != NULL) {
        amf0_data * name;
        amf0_data * element;
        uint32_t size;
        if (read_proc(&size, sizeof(uint32_t), user_data) == sizeof(uint32_t)) {
            /* we ignore the 32 bits array size marker */
            while(1) {
                name = amf0_string_read(read_proc, user_data);
                if (name != NULL) {
                    element = amf0_data_read(read_proc, user_data);
                    if (element != NULL) {
                        if (amf0_associative_array_add(data, (char *)amf0_string_get_uint8_ts(name), element) == NULL) {
                            amf0_data_free(name);
                            amf0_data_free(element);
                            amf0_data_free(data);
                            return NULL;
                        }
                    }
                    else {
                        amf0_data_free(name);
                        break;
                    }
                }
                else {
                    /* invalid name: error */
                    amf0_data_free(data);
                    return NULL;
                }
            }
        }
        else {
            amf0_data_free(data);
            return NULL;
        }
    }
    return data;
}

/* read an array */
static amf0_data * amf0_array_read(read_proc_t read_proc, void * user_data) {
    size_t i;
    amf0_data * element;
    amf0_data * data = amf0_array_new();
    if (data != NULL) {
        uint32_t array_size;
        if (read_proc(&array_size, sizeof(uint32_t), user_data) == sizeof(uint32_t)) {
            array_size = swap_uint32(array_size);
            
            for (i = 0; i < array_size; ++i) {
                element = amf0_data_read(read_proc, user_data);

                if (element != NULL) {
                    if (amf0_array_push(data, element) == NULL) {
                        amf0_data_free(element);
                        amf0_data_free(data);
                        return NULL;
                    }
                }
                else {
                    amf0_data_free(data);
                    return NULL;
                }
            }
        }
        else {
            amf0_data_free(data);
            return NULL;
        }
    }
    return data;
}

/* read a date */
static amf0_data * amf0_date_read(read_proc_t read_proc, void * user_data) {
    number64_t milliseconds;
    int16_t timezone;
    if (read_proc(&milliseconds, sizeof(number64_t), user_data) == sizeof(number64_t) &&
        read_proc(&timezone, sizeof(int16_t), user_data) == sizeof(int16_t)) {
        return amf0_date_new(swap_number64(milliseconds), swap_sint16(timezone));
    }
    else {
        return NULL;
    }
}

/* load AMF data from stream */
amf0_data * amf0_data_read(read_proc_t read_proc, void * user_data) {
    uint8_t type;
    if (read_proc(&type, sizeof(uint8_t), user_data) == sizeof(uint8_t)) {
        switch (type) {
            case AMF0_TYPE_NUMBER:
                return amf0_number_read(read_proc, user_data);
            case AMF0_TYPE_BOOLEAN:
                return amf0_boolean_read(read_proc, user_data);
            case AMF0_TYPE_STRING:
                return amf0_string_read(read_proc, user_data);
            case AMF0_TYPE_OBJECT:
                return amf0_object_read(read_proc, user_data);
            case AMF0_TYPE_NULL:
                return amf0_null_new();
            case AMF0_TYPE_UNDEFINED:
                return amf0_undefined_new();
            /*case AMF0_TYPE_REFERENCE:*/
            case AMF0_TYPE_ECMA_ARRAY:
                return amf0_associative_array_read(read_proc, user_data);
            case AMF0_TYPE_STRICT_ARRAY:
                return amf0_array_read(read_proc, user_data);
            case AMF0_TYPE_DATE:
                return amf0_date_read(read_proc, user_data);
            /*case AMF0_TYPE_SIMPLEOBJECT:*/
            case AMF0_TYPE_XML_DOCUMENT:
            case AMF0_TYPE_TYPED_OBJECT:
            case AMF0_TYPE_OBJECT_END:
                return NULL; /* end of composite object */
            default:
                break;
        }
    }
    return NULL;
}

/* determines the size of the given AMF data */
size_t amf0_data_size(amf0_data * data) {
    size_t s = 0;
    amf0_node * node;
    if (data != NULL) {
        s += sizeof(uint8_t);
        switch (data->type) {
            case AMF0_TYPE_NUMBER:
                s += sizeof(number64_t);
                break;
            case AMF0_TYPE_BOOLEAN:
                s += sizeof(uint8_t);
                break;
            case AMF0_TYPE_STRING:
                s += sizeof(uint16_t) + (size_t)amf0_string_get_size(data);
                break;
            case AMF0_TYPE_OBJECT:
                node = amf0_object_first(data);
                while (node != NULL) {
                    s += sizeof(uint16_t) + (size_t)amf0_string_get_size(amf0_object_get_name(node));
                    s += (size_t)amf0_data_size(amf0_object_get_data(node));
                    node = amf0_object_next(node);
                }
                s += sizeof(uint16_t) + sizeof(uint8_t);
                break;
            case AMF0_TYPE_NULL:
            case AMF0_TYPE_UNDEFINED:
                break;
            /*case AMF0_TYPE_REFERENCE:*/
            case AMF0_TYPE_ECMA_ARRAY:
                s += sizeof(uint32_t);
                node = amf0_associative_array_first(data);
                while (node != NULL) {
                    s += sizeof(uint16_t) + (size_t)amf0_string_get_size(amf0_associative_array_get_name(node));
                    s += (size_t)amf0_data_size(amf0_associative_array_get_data(node));
                    node = amf0_associative_array_next(node);
                }
                s += sizeof(uint16_t) + sizeof(uint8_t);
                break;
            case AMF0_TYPE_STRICT_ARRAY:
                s += sizeof(uint32_t);
                node = amf0_array_first(data);
                while (node != NULL) {
                    s += (size_t)amf0_data_size(amf0_array_get(node));
                    node = amf0_array_next(node);
                }
                break;
            case AMF0_TYPE_DATE:
                s += sizeof(number64_t) + sizeof(int16_t);
                break;
            /*case AMF0_TYPE_SIMPLEOBJECT:*/
            case AMF0_TYPE_XML_DOCUMENT:
            case AMF0_TYPE_TYPED_OBJECT:
            case AMF0_TYPE_OBJECT_END:
                break; /* end of composite object */
            default:
                break;
        }
    }
    return s;
}

/* write a number */
static size_t amf0_number_write(amf0_data * data, write_proc_t write_proc, void * user_data) {
    number64_t n = swap_number64(data->u.number_data);
    return write_proc(&n, sizeof(number64_t), user_data);
}

/* write a boolean */
static size_t amf0_boolean_write(amf0_data * data, write_proc_t write_proc, void * user_data) {
    return write_proc(&(data->u.boolean_data), sizeof(uint8_t), user_data);
}

/* write a string */
static size_t amf0_string_write(amf0_data * data, write_proc_t write_proc, void * user_data) {
    uint16_t s;
    size_t w = 0;

    s = swap_uint16(data->u.string_data.size);
    w = write_proc(&s, sizeof(uint16_t), user_data);
    if (data->u.string_data.size > 0) {
        w += write_proc(data->u.string_data.mbstr, (size_t)(data->u.string_data.size), user_data);
    }

    return w;
}

/* write an object */
static size_t amf0_object_write(amf0_data * data, write_proc_t write_proc, void * user_data) {
    amf0_node * node;
    size_t w = 0;
    uint16_t filler = swap_uint16(0);
    uint8_t terminator = AMF0_TYPE_OBJECT_END;

    node = amf0_object_first(data);
    while (node != NULL) {
        w += amf0_string_write(amf0_object_get_name(node), write_proc, user_data);
        w += amf0_data_write(amf0_object_get_data(node), write_proc, user_data);
        node = amf0_object_next(node);
    }

    /* empty string is the last element */
    w += write_proc(&filler, sizeof(uint16_t), user_data);
    /* an object ends with 0x09 */
    w += write_proc(&terminator, sizeof(uint8_t), user_data);

    return w;
}

/* write an associative array */
static size_t amf0_associative_array_write(amf0_data * data, write_proc_t write_proc, void * user_data) {
    amf0_node * node;
    size_t w = 0;
    uint32_t s;
    uint16_t filler = swap_uint16(0);
    uint8_t terminator = AMF0_TYPE_OBJECT_END;

    s = swap_uint32(data->u.list_data.size) / 2;
    w += write_proc(&s, sizeof(uint32_t), user_data);
    node = amf0_associative_array_first(data);
    while (node != NULL) {
        w += amf0_string_write(amf0_associative_array_get_name(node), write_proc, user_data);
        w += amf0_data_write(amf0_associative_array_get_data(node), write_proc, user_data);
        node = amf0_associative_array_next(node);
    }

    /* empty string is the last element */
    w += write_proc(&filler, sizeof(uint16_t), user_data);
    /* an object ends with 0x09 */
    w += write_proc(&terminator, sizeof(uint8_t), user_data);

    return w;
}

/* write an array */
static size_t amf0_array_write(amf0_data * data, write_proc_t write_proc, void * user_data) {
    amf0_node * node;
    size_t w = 0;
    uint32_t s;

    s = swap_uint32(data->u.list_data.size);
    w += write_proc(&s, sizeof(uint32_t), user_data);
    node = amf0_array_first(data);
    while (node != NULL) {
        w += amf0_data_write(amf0_array_get(node), write_proc, user_data);
        node = amf0_array_next(node);
    }

    return w;
}

/* write a date */
static size_t amf0_date_write(amf0_data * data, write_proc_t write_proc, void * user_data) {
    size_t w = 0;
    number64_t milli;
    int16_t tz;

    milli = swap_number64(data->u.date_data.milliseconds);
    w += write_proc(&milli, sizeof(number64_t), user_data);
    tz = swap_sint16(data->u.date_data.timezone);
    w += write_proc(&tz, sizeof(int16_t), user_data);

    return w;
}

/* write amf data to stream */
size_t amf0_data_write(amf0_data * data, write_proc_t write_proc, void * user_data) {
    size_t s = 0;
    if (data != NULL) {
        s += write_proc(&(data->type), sizeof(uint8_t), user_data);
        switch (data->type) {
            case AMF0_TYPE_NUMBER:
                s += amf0_number_write(data, write_proc, user_data);
                break;
            case AMF0_TYPE_BOOLEAN:
                s += amf0_boolean_write(data, write_proc, user_data);
                break;
            case AMF0_TYPE_STRING:
                s += amf0_string_write(data, write_proc, user_data);
                break;
            case AMF0_TYPE_OBJECT:
                s += amf0_object_write(data, write_proc, user_data);
                break;
            case AMF0_TYPE_NULL:
            case AMF0_TYPE_UNDEFINED:
                break;
            /*case AMF0_TYPE_REFERENCE:*/
            case AMF0_TYPE_ECMA_ARRAY:
                s += amf0_associative_array_write(data, write_proc, user_data);
                break;
            case AMF0_TYPE_STRICT_ARRAY:
                s += amf0_array_write(data, write_proc, user_data);
                break;
            case AMF0_TYPE_DATE:
                s += amf0_date_write(data, write_proc, user_data);
                break;
            /*case AMF0_TYPE_SIMPLEOBJECT:*/
            case AMF0_TYPE_XML_DOCUMENT:
            case AMF0_TYPE_TYPED_OBJECT:
            case AMF0_TYPE_OBJECT_END:
                break; /* end of composite object */
            default:
                break;
        }
    }
    return s;
}

/* data type */
uint8_t amf0_data_get_type(amf0_data * data) {
    return (data != NULL) ? data->type : AMF0_TYPE_NULL;
}

/* clone AMF data */
amf0_data * amf0_data_clone(amf0_data * data) {
    /* we copy data recursively */
    if (data != NULL) {
        switch (data->type) {
            case AMF0_TYPE_NUMBER: return amf0_number_new(amf0_number_get_value(data));
            case AMF0_TYPE_BOOLEAN: return amf0_boolean_new(amf0_boolean_get_value(data));
            case AMF0_TYPE_STRING:
                if (data->u.string_data.mbstr != NULL) {
                    return amf0_string_new((uint8_t *)strdup((char *)amf0_string_get_uint8_ts(data)), amf0_string_get_size(data));
                }
                else {
                    return amf0_str(NULL);
                }
            case AMF0_TYPE_NULL: return NULL;
            case AMF0_TYPE_UNDEFINED: return NULL;
            /*case AMF0_TYPE_REFERENCE:*/
            case AMF0_TYPE_OBJECT:
            case AMF0_TYPE_ECMA_ARRAY:
            case AMF0_TYPE_STRICT_ARRAY:
                {
                    amf0_data * d = amf0_data_new(data->type);
                    if (d != NULL) {
                        amf0_list_init(&d->u.list_data);
                        amf0_list_clone(&data->u.list_data, &d->u.list_data);
                    }
                    return d;
                }
            case AMF0_TYPE_DATE: return amf0_date_new(amf0_date_get_milliseconds(data), amf0_date_get_timezone(data));
            /*case AMF0_TYPE_SIMPLEOBJECT:*/
            case AMF0_TYPE_XML_DOCUMENT: return NULL;
            case AMF0_TYPE_TYPED_OBJECT: return NULL;
        }
    }
    return NULL;
}

/* free AMF data */
void amf0_data_free(amf0_data * data) {
    if (data != NULL) {
        switch (data->type) {
            case AMF0_TYPE_NUMBER: break;
            case AMF0_TYPE_BOOLEAN: break;
            case AMF0_TYPE_STRING:
                if (data->u.string_data.mbstr) {
                    free(data->u.string_data.mbstr);
		    data->u.string_data.mbstr = NULL;
                } 
		break;
            case AMF0_TYPE_NULL: break;
            case AMF0_TYPE_UNDEFINED: break;
            /*case AMF0_TYPE_REFERENCE:*/
            case AMF0_TYPE_OBJECT:
            case AMF0_TYPE_ECMA_ARRAY:
            case AMF0_TYPE_STRICT_ARRAY: amf0_list_clear(&data->u.list_data); break;
            case AMF0_TYPE_DATE: break;
            /*case AMF0_TYPE_SIMPLEOBJECT:*/
            case AMF0_TYPE_XML_DOCUMENT: break;
            case AMF0_TYPE_TYPED_OBJECT: break;
            default: break;
        }
        free(data);
    }
}

/* dump AMF data into a stream as text */
void amf0_data_dump(FILE * stream, amf0_data * data, int indent_level) {
    if (data != NULL) {
        amf0_node * node;
        time_t time;
        struct tm * t;
        char datestr[128];
        switch (data->type) {
            case AMF0_TYPE_NUMBER:
                fprintf(stream, "%.12g", (double)data->u.number_data);
                break;
            case AMF0_TYPE_BOOLEAN:
                fprintf(stream, "%s", (data->u.boolean_data) ? "true" : "false");
                break;
            case AMF0_TYPE_STRING:
                fprintf(stream, "\'%.*s\'", data->u.string_data.size, data->u.string_data.mbstr);
                break;
            case AMF0_TYPE_OBJECT:
                node = amf0_object_first(data);
                fprintf(stream, "{\n");
                while (node != NULL) {
                    fprintf(stream, "%*s", (indent_level+1)*4, "");
                    amf0_data_dump(stream, amf0_object_get_name(node), indent_level+1);
                    fprintf(stream, ": ");
                    amf0_data_dump(stream, amf0_object_get_data(node), indent_level+1);
                    node = amf0_object_next(node);
                    fprintf(stream, "\n");
                }
                fprintf(stream, "%*s", indent_level*4 + 1, "}");
                break;
            case AMF0_TYPE_NULL:
                fprintf(stream, "null");
                break;
            case AMF0_TYPE_UNDEFINED:
                fprintf(stream, "undefined");
                break;
            /*case AMF0_TYPE_REFERENCE:*/
            case AMF0_TYPE_ECMA_ARRAY:
                node = amf0_associative_array_first(data);
                fprintf(stream, "{\n");
                while (node != NULL) {
                    fprintf(stream, "%*s", (indent_level+1)*4, "");
                    amf0_data_dump(stream, amf0_associative_array_get_name(node), indent_level+1);
                    fprintf(stream, " => ");
                    amf0_data_dump(stream, amf0_associative_array_get_data(node), indent_level+1);
                    node = amf0_associative_array_next(node);
                    fprintf(stream, "\n");
                }
                fprintf(stream, "%*s", indent_level*4 + 1, "}");
                break;
            case AMF0_TYPE_STRICT_ARRAY:
                node = amf0_array_first(data);
                fprintf(stream, "[\n");
                while (node != NULL) {
                    fprintf(stream, "%*s", (indent_level+1)*4, "");
                    amf0_data_dump(stream, node->data, indent_level+1);
                    node = amf0_array_next(node);
                    fprintf(stream, "\n");
                }
                fprintf(stream, "%*s", indent_level*4 + 1, "]");
                break;
            case AMF0_TYPE_DATE:
                time = amf0_date_to_time_t(data);
                tzset();
                t = localtime(&time);
                strftime(datestr, sizeof(datestr), "%a, %d %b %Y %H:%M:%S %z", t);
                fprintf(stream, "%s", datestr);
                break;
            /*case AMF0_TYPE_SIMPLEOBJECT:*/
            case AMF0_TYPE_XML_DOCUMENT: break;
            case AMF0_TYPE_TYPED_OBJECT: break;
            default: break;
        }
    }
}

/* number functions */
amf0_data * amf0_number_new(number64_t value) {
    amf0_data * data = amf0_data_new(AMF0_TYPE_NUMBER);
    if (data != NULL) {
        data->u.number_data = value;
    }
    return data;
}

number64_t amf0_number_get_value(amf0_data * data) {
    return (data != NULL) ? data->u.number_data : 0;
}

void amf0_number_set_value(amf0_data * data, number64_t value) {
    if (data != NULL) {
        data->u.number_data = value;
    }
}

/* boolean functions */
amf0_data * amf0_boolean_new(uint8_t value) {
    amf0_data * data = amf0_data_new(AMF0_TYPE_BOOLEAN);
    if (data != NULL) {
        data->u.boolean_data = value;
    }
    return data;
}

uint8_t amf0_boolean_get_value(amf0_data * data) {
    return (data != NULL) ? data->u.boolean_data : 0;
}

void amf0_boolean_set_value(amf0_data * data, uint8_t value) {
    if (data != NULL) {
        data->u.boolean_data = value;
    }
}

/* string functions */
amf0_data * amf0_string_new(uint8_t * str, uint16_t size) {
    amf0_data * data = amf0_data_new(AMF0_TYPE_STRING);
    if (data != NULL) {
        data->u.string_data.size = size;
        data->u.string_data.mbstr = (uint8_t*)calloc(size+1, sizeof(uint8_t));
        if (data->u.string_data.mbstr != NULL) {
            if (size > 0) {
                memcpy(data->u.string_data.mbstr, str, size);
            }
        }
        else {
            amf0_data_free(data);
            return NULL;
        }
    }
    return data;
}

amf0_data * amf0_str(const char * str) {
    return amf0_string_new((uint8_t *)str, (uint16_t)(str != NULL ? strlen(str) : 0));
}

uint16_t amf0_string_get_size(amf0_data * data) {
    return (data != NULL) ? data->u.string_data.size : 0;
}

uint8_t * amf0_string_get_uint8_ts(amf0_data * data) {
    return (data != NULL) ? data->u.string_data.mbstr : NULL;
}

/* object functions */
amf0_data * amf0_object_new(void) {
    amf0_data * data = amf0_data_new(AMF0_TYPE_OBJECT);
    if (data != NULL) {
        amf0_list_init(&data->u.list_data);
    }
    return data;
}

uint32_t amf0_object_size(amf0_data * data) {
    return (data != NULL) ? data->u.list_data.size / 2 : 0;
}

amf0_data * amf0_object_add(amf0_data * data, const char * name, amf0_data * element) {
  if (data != NULL) {
    amf0_data *str_name = amf0_str(name);
    if (amf0_list_push(&data->u.list_data, str_name) != NULL) {
      if (amf0_list_push(&data->u.list_data, element) != NULL) {
	return element;
      }
      else {
	amf0_data_free(amf0_list_pop(&data->u.list_data));
      }
    }
    amf0_data_free(str_name);
  }
  return NULL;
}

amf0_data * amf0_object_get(amf0_data * data, const char * name) {
    if (data != NULL) {
        amf0_node * node = amf0_list_first(&(data->u.list_data));
        while (node != NULL) {
            if (strncmp((char*)(node->data->u.string_data.mbstr), name, (size_t)(node->data->u.string_data.size)) == 0) {
                node = node->next;
                return (node != NULL) ? node->data : NULL;
            }
            /* we have to skip the element data to reach the next name */
            node = node->next->next;
        }
    }
    return NULL;
}

amf0_data * amf0_object_set(amf0_data * data, const char * name, amf0_data * element) {
    if (data != NULL) {
        amf0_node * node = amf0_list_first(&(data->u.list_data));
        while (node != NULL) {
            if (strncmp((char*)(node->data->u.string_data.mbstr), name, (size_t)(node->data->u.string_data.size)) == 0) {
                node = node->next;
                if (node != NULL && node->data != NULL) {
                    amf0_data_free(node->data);
                    node->data = element;
                    return element;
                }
            }
            /* we have to skip the element data to reach the next name */
            node = node->next->next;
        }
    }
    return NULL;
}

amf0_data * amf0_object_delete(amf0_data * data, const char * name) {
    if (data != NULL) {
        amf0_node * node = amf0_list_first(&data->u.list_data);
        while (node != NULL) {
            node = node->next;
            if (strncmp((char*)(node->data->u.string_data.mbstr), name, (size_t)(node->data->u.string_data.size)) == 0) {
                amf0_node * data_node = node->next;
                amf0_data_free(amf0_list_delete(&data->u.list_data, node));
                return amf0_list_delete(&data->u.list_data, data_node);
            }
            else {
                node = node->next;
            }
        }
    }
    return NULL;
}

amf0_node * amf0_object_first(amf0_data * data) {
    return (data != NULL) ? amf0_list_first(&data->u.list_data) : NULL;
}

amf0_node * amf0_object_last(amf0_data * data) {
    if (data != NULL) {
        amf0_node * node = amf0_list_last(&data->u.list_data);
        if (node != NULL) {
            return node->prev;
        }
    }
    return NULL;
}

amf0_node * amf0_object_next(amf0_node * node) {
    if (node != NULL) {
        amf0_node * next = node->next;
        if (next != NULL) {
            return next->next;
        }
    }
    return NULL;
}

amf0_node * amf0_object_prev(amf0_node * node) {
    if (node != NULL) {
        amf0_node * prev = node->prev;
        if (prev != NULL) {
            return prev->prev;
        }
    }
    return NULL;
}

amf0_data * amf0_object_get_name(amf0_node * node) {
    return (node != NULL) ? node->data : NULL;
}

amf0_data * amf0_object_get_data(amf0_node * node) {
    if (node != NULL) {
        amf0_node * next = node->next;
        if (next != NULL) {
            return next->data;
        }
    }
    return NULL;
}

/* associative array functions */
amf0_data * amf0_associative_array_new(void) {
    amf0_data * data = amf0_data_new(AMF0_TYPE_ECMA_ARRAY);
    if (data != NULL) {
        amf0_list_init(&data->u.list_data);
    }
    return data;
}

/* array functions */
amf0_data * amf0_array_new(void) {
    amf0_data * data = amf0_data_new(AMF0_TYPE_STRICT_ARRAY);
    if (data != NULL) {
        amf0_list_init(&data->u.list_data);
    }
    return data;
}

uint32_t amf0_array_size(amf0_data * data) {
    return (data != NULL) ? data->u.list_data.size : 0;
}

amf0_data * amf0_array_push(amf0_data * data, amf0_data * element) {
    return (data != NULL) ? amf0_list_push(&data->u.list_data, element) : NULL;
}

amf0_data * amf0_array_pop(amf0_data * data) {
    return (data != NULL) ? amf0_list_pop(&data->u.list_data) : NULL;
}

amf0_node * amf0_array_first(amf0_data * data) {
    return (data != NULL) ? amf0_list_first(&data->u.list_data) : NULL;
}

amf0_node * amf0_array_last(amf0_data * data) {
    return (data != NULL) ? amf0_list_last(&data->u.list_data) : NULL;
}

amf0_node * amf0_array_next(amf0_node * node) {
    return (node != NULL) ? node->next : NULL;
}

amf0_node * amf0_array_prev(amf0_node * node) {
    return (node != NULL) ? node->prev : NULL;
}

amf0_data * amf0_array_get(amf0_node * node) {
    return (node != NULL) ? node->data : NULL;
}

amf0_data * amf0_array_get_at(amf0_data * data, uint32_t n) {
    return (data != NULL) ? amf0_list_get_at(&data->u.list_data, n) : NULL;
}

amf0_data * amf0_array_delete(amf0_data * data, amf0_node * node) {
    return (data != NULL) ? amf0_list_delete(&data->u.list_data, node) : NULL;
}

amf0_data * amf0_array_insert_before(amf0_data * data, amf0_node * node, amf0_data * element) {
    return (data != NULL) ? amf0_list_insert_before(&data->u.list_data, node, element) : NULL;
}

amf0_data * amf0_array_insert_after(amf0_data * data, amf0_node * node, amf0_data * element) {
    return (data != NULL) ? amf0_list_insert_after(&data->u.list_data, node, element) : NULL;
}

/* date functions */
amf0_data * amf0_date_new(number64_t milliseconds, int16_t timezone) {
    amf0_data * data = amf0_data_new(AMF0_TYPE_DATE);
    if (data != NULL) {
        data->u.date_data.milliseconds = milliseconds;
        data->u.date_data.timezone = timezone;
    }
    return data;
}

number64_t amf0_date_get_milliseconds(amf0_data * data) {
    return (data != NULL) ? data->u.date_data.milliseconds : 0.0;
}

int16_t amf0_date_get_timezone(amf0_data * data) {
    return (data != NULL) ? data->u.date_data.timezone : 0;
}

time_t amf0_date_to_time_t(amf0_data * data) {
    return (time_t)((data != NULL) ? data->u.date_data.milliseconds / 1000 : 0);
}
