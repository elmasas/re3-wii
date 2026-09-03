#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	int index;
} __wii_key_t;

int wii_key_create(__wii_key_t *key, void (*dtor) (void *));

int wii_key_delete(__wii_key_t key);

void* wii_getspecific(__wii_key_t key);

int wii_setspecific(__wii_key_t key, const void *ptr);

#ifdef __cplusplus
}
#endif
