/* function common to all array types */
static void amf_list_init(amf_list * list) {
    if (list != NULL) {
        list->size = 0;
        list->first_element = NULL;
        list->last_element = NULL;
    }
}

static amf_data * amf_list_push(amf_list * list, amf_data * data) {
    amf_node * node = (amf_node*)malloc(sizeof(amf_node));
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

static amf_data * amf_list_insert_before(amf_list * list, amf_node * node, amf_data * data) {
    if (node != NULL) {
        amf_node * new_node = (amf_node*)malloc(sizeof(amf_node));
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

static amf_data * amf_list_insert_after(amf_list * list, amf_node * node, amf_data * data) {
    if (node != NULL) {
        amf_node * new_node = (amf_node*)malloc(sizeof(amf_node));
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

static amf_data * amf_list_delete(amf_list * list, amf_node * node) {
    amf_data * data = NULL;
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

static amf_data * amf_list_get_at(amf_list * list, uint32 n) {
    if (n < list->size) {
        uint32 i;
        amf_node * node = list->first_element;
        for (i = 0; i < n; ++i) {
            node = node->next;
        }
        return node->data;
    }
    return NULL;
}

static amf_data * amf_list_pop(amf_list * list) {
    return amf_list_delete(list, list->last_element);
}

static amf_node * amf_list_first(amf_list * list) {
    return list->first_element;
}

static amf_node * amf_list_last(amf_list * list) {
    return list->last_element;
}

static void amf_list_clear(amf_list * list) {
    amf_node * node = list->first_element;
    while (node != NULL) {
        amf_data_free(node->data);
        amf_node * tmp = node;
        node = node->next;
        free(tmp);
    }
    list->size = 0;
}
