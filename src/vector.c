#include <stdlib.h>
#include <string.h>

#include "vector.h"

struct vector {
    void* data;
    size_t len;
    size_t cap;
    size_t val_size;
};

Vector* _create_vector(size_t size) {
    Vector* vector = malloc(sizeof(Vector));
    *vector = (Vector) {
        .data = NULL,
        .len = 0,
        .cap = 0,
        .val_size = size,
    };
    return vector;
}

size_t vector_len(Vector* vector) {
    return vector->len;
}

size_t vector_cap(Vector* vector) {
    return vector->cap;
}

size_t vector_val_size(Vector* vector) {
    return vector->val_size;
}

void vector_push(Vector* vector, void* value) {
    if (vector->len >= vector->cap) {
        if (vector->cap == 0)
            vector->cap = 8;
        else vector->cap <<= 1;
        vector->data = realloc(vector->data, vector->cap * vector->val_size);
    }

    memcpy(vector->data + vector->len * vector->val_size, value, vector->val_size);
    vector->len++;
}

void* vector_get(Vector* vector, size_t i) {
    if (i >= vector->len)
        return NULL;
    return vector->data + i * vector->val_size;
}

void destroy_vector(Vector* vector, void* free_callback) {
    if (free_callback != NULL) {
        for (size_t i = 0; i < vector->len; i++) {
            ((void (*)(void*)) free_callback)(vector->data + i * vector->val_size);
        }
    }

    free(vector->data);
    free(vector);
}
