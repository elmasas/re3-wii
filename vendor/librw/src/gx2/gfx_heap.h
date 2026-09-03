// from https://github.com/devkitPro/wut/blob/master/libraries/libwhb/src/gfx_heap.h

#pragma once
#include <wut.h>

#ifdef __cplusplus
extern "C" {
#endif

BOOL
GfxHeapInitMEM1();

BOOL
GfxHeapDestroyMEM1();

BOOL
GfxHeapInitForeground();

BOOL
GfxHeapDestroyForeground();

void *
GfxHeapAllocMEM1(uint32_t size,
                 uint32_t alignment);

void
GfxHeapFreeMEM1(void *block);

void *
GfxHeapAllocForeground(uint32_t size,
                       uint32_t alignment);

void
GfxHeapFreeForeground(void *block);

void *
GfxHeapAllocMEM2(uint32_t size,
                 uint32_t alignment);

void
GfxHeapFreeMEM2(void *block);

#ifdef __cplusplus
}
#endif
