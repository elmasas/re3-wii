#pragma once

#include <stdint.h>
#include <coreinit/thread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
   uint32_t index;
} __wut_key_t;

int wut_key_create(__wut_key_t *key, void (*dtor) (void *));

int wut_key_delete(__wut_key_t key);

void* wut_getspecific(__wut_key_t key);

int wut_setspecific(__wut_key_t key, const void *ptr);

#ifdef __cplusplus
}
#endif