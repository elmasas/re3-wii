#include "wii_thread_compat.h"

#include <ogc/lwp.h>
#include <ogc/mutex.h>
#include <string.h>

#define WII_TSS_MAX_KEYS 8
#define WII_TSS_MAX_THREADS 16

struct TssSlot {
	lwp_t thread;
	void *value;
	bool used;
};

struct TssKey {
	bool used;
	void (*dtor)(void*);
	TssSlot slots[WII_TSS_MAX_THREADS];
};

static TssKey s_keys[WII_TSS_MAX_KEYS];
static mutex_t s_lock;
static bool s_lockInit = false;

static void EnsureLockInit()
{
	if (!s_lockInit) {
		LWP_MutexInit(&s_lock, false);
		s_lockInit = true;
	}
}

extern "C" int wii_key_create(__wii_key_t *key, void (*dtor)(void *))
{
	EnsureLockInit();
	LWP_MutexLock(s_lock);
	for (int i = 0; i < WII_TSS_MAX_KEYS; i++) {
		if (!s_keys[i].used) {
			s_keys[i].used = true;
			s_keys[i].dtor = dtor;
			memset(s_keys[i].slots, 0, sizeof(s_keys[i].slots));
			key->index = i;
			LWP_MutexUnlock(s_lock);
			return 0;
		}
	}
	LWP_MutexUnlock(s_lock);
	return -1;
}

extern "C" int wii_key_delete(__wii_key_t key)
{
	EnsureLockInit();
	LWP_MutexLock(s_lock);
	s_keys[key.index].used = false;
	LWP_MutexUnlock(s_lock);
	return 0;
}

extern "C" void *wii_getspecific(__wii_key_t key)
{
	EnsureLockInit();
	lwp_t self = LWP_GetSelf();
	void *result = NULL;
	LWP_MutexLock(s_lock);
	TssKey &k = s_keys[key.index];
	for (int i = 0; i < WII_TSS_MAX_THREADS; i++) {
		if (k.slots[i].used && k.slots[i].thread == self) {
			result = k.slots[i].value;
			break;
		}
	}
	LWP_MutexUnlock(s_lock);
	return result;
}

extern "C" int wii_setspecific(__wii_key_t key, const void *ptr)
{
	EnsureLockInit();
	lwp_t self = LWP_GetSelf();
	LWP_MutexLock(s_lock);
	TssKey &k = s_keys[key.index];
	int freeSlot = -1;
	for (int i = 0; i < WII_TSS_MAX_THREADS; i++) {
		if (k.slots[i].used && k.slots[i].thread == self) {
			k.slots[i].value = (void*)ptr;
			LWP_MutexUnlock(s_lock);
			return 0;
		}
		if (!k.slots[i].used && freeSlot == -1)
			freeSlot = i;
	}
	if (freeSlot >= 0) {
		k.slots[freeSlot].used = true;
		k.slots[freeSlot].thread = self;
		k.slots[freeSlot].value = (void*)ptr;
		LWP_MutexUnlock(s_lock);
		return 0;
	}
	LWP_MutexUnlock(s_lock);
	return -1;
}
