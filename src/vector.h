#ifndef VECTOR_H
#define VECTOR_H

#include <stddef.h>

typedef struct vector Vector;

#define create_vector(type) _create_vector(sizeof(type))

Vector* _create_vector(size_t size);

size_t vector_len(Vector* vector);

size_t vector_cap(Vector* vector);

size_t vector_val_size(Vector* vector);

void vector_push(Vector* vector, void* value);

void* vector_get(Vector* vector, size_t i);

void destroy_vector(Vector* vector, void* free_callback);

#endif /* VECTOR_H */
