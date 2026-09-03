#pragma once

#include <gx2/texture.h>

void gfxUtilsInit();
void gfxUtilsShutdown();

// just like GX2CopySurface this will change the current context state
void gfxUtilsFlipSurface(GX2Surface* src, uint32_t srcLevel, uint32_t srcDepth, GX2Surface* dst, uint32_t dstLevel, uint32_t dstDepth);
