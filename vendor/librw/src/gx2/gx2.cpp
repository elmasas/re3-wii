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

#include "rwgx2.h"
#include "rwgx2impl.h"

namespace rw {
namespace gx2 {

static void*
driverOpen(void *o, int32, int32)
{
	engine->driver[PLATFORM_GX2]->defaultPipeline       = makeDefaultPipeline();
	engine->driver[PLATFORM_GX2]->rasterNativeOffset    = nativeRasterOffset;
	engine->driver[PLATFORM_GX2]->rasterCreate          = rasterCreate;
	engine->driver[PLATFORM_GX2]->rasterLock            = rasterLock;
	engine->driver[PLATFORM_GX2]->rasterUnlock          = rasterUnlock;
	engine->driver[PLATFORM_GX2]->rasterNumLevels       = rasterNumLevels;
	engine->driver[PLATFORM_GX2]->imageFindRasterFormat = imageFindRasterFormat;
	engine->driver[PLATFORM_GX2]->rasterFromImage       = rasterFromImage;

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
	Driver::registerPlugin(PLATFORM_GX2, 0, PLATFORM_GX2, driverOpen, driverClose);
	registerNativeRaster();
}

}
}
