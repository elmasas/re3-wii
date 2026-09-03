#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../rwbase.h"
#include "../rwerror.h"
#include "../rwplg.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "../rwengine.h"

#include "rwgx.h"

namespace rw {
namespace gx {

static void*
driverOpen(void *o, int32, int32)
{
#ifdef __WII__
	engine->driver[PLATFORM_GX]->defaultPipeline       = makeDefaultPipeline();
	engine->driver[PLATFORM_GX]->rasterNativeOffset    = nativeRasterOffset;
	engine->driver[PLATFORM_GX]->rasterCreate          = rasterCreate;
	engine->driver[PLATFORM_GX]->rasterLock            = rasterLock;
	engine->driver[PLATFORM_GX]->rasterUnlock          = rasterUnlock;
	engine->driver[PLATFORM_GX]->rasterLockPalette     = rasterLockPalette;
	engine->driver[PLATFORM_GX]->rasterUnlockPalette   = rasterUnlockPalette;
	engine->driver[PLATFORM_GX]->rasterNumLevels       = rasterNumLevels;
	engine->driver[PLATFORM_GX]->imageFindRasterFormat = imageFindRasterFormat;
	engine->driver[PLATFORM_GX]->rasterFromImage       = rasterFromImage;
	engine->driver[PLATFORM_GX]->rasterToImage         = rasterToImage;
#endif
	return o;
}

static void*
driverClose(void *o, int32, int32)
{
	return o;
}

void
registerPlatformPlugins(void)
{
	Driver::registerPlugin(PLATFORM_GX, 0, PLATFORM_GX, driverOpen, driverClose);
#ifdef __WII__
	registerNativeRaster();
	initSkin();
	initMatFX();
#endif
}

}
}
