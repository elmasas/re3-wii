#include "gfx_utils.h"

#include <stdlib.h>
#include <whb/gfx.h>
#include <whb/log.h>
#include "gfx_heap.h"

#include <cstdio>
#include <malloc.h>
#include "../rwbase.h"
#include "../rwplg.h"
#include "../rwengine.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "rwgx2.h"
#include "rwgx2shader.h"

extern unsigned char im2d_gsh[];

// dimension of the internal color buffer used for flipping
#define INTERNAL_TEXTURE_WIDTH 1920
#define INTERNAL_TEXTURE_HEIGHT 1080

static WHBGfxShaderGroup shader;
static GX2ColorBuffer colorBuffer;
static GX2ContextState* state;
static GX2Texture tex;
static GX2Sampler samp;
static int32_t u_xform;
static int32_t u_fogData;

static float fog_data[] = {
    0.0f, 0.0f, 0.0f,
    1.0f // disable fog
};

static float flip_vertex[]  __attribute__ ((aligned (GX2_VERTEX_BUFFER_ALIGNMENT))) = {
    // pos                                                         col                      uv
    0.0f,                   0.0f,                    -1.0f, 1.0f,  1.0f, 1.0f, 1.0f, 1.0f,  0.0f, 0.0f,
    0.0f,                   INTERNAL_TEXTURE_HEIGHT, -1.0f, 1.0f,  1.0f, 1.0f, 1.0f, 1.0f,  0.0f, 1.0f,
    INTERNAL_TEXTURE_WIDTH, INTERNAL_TEXTURE_HEIGHT, -1.0f, 1.0f,  1.0f, 1.0f, 1.0f, 1.0f,  1.0f, 1.0f,
    INTERNAL_TEXTURE_WIDTH, 0.0f,                    -1.0f, 1.0f,  1.0f, 1.0f, 1.0f, 1.0f,  1.0f, 0.0f,
};

static uint16_t flip_index[] __attribute__ ((aligned (GX2_INDEX_BUFFER_ALIGNMENT))) = {
    0, 1, 2, 0, 2, 3
};

void gfxUtilsInit()
{
	if (!WHBGfxLoadGFDShaderGroup(&shader, 0, im2d_gsh)) {
        WHBLogPrintf("Failed to load flip shader");
        abort();
    }

    WHBGfxInitShaderAttribute(&shader, "in_pos", 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32);
    WHBGfxInitShaderAttribute(&shader, "in_color", 0, 16, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32);
    WHBGfxInitShaderAttribute(&shader, "in_tex0", 0, 32, GX2_ATTRIB_FORMAT_FLOAT_32_32);

    if (!WHBGfxInitFetchShader(&shader)) {
		WHBLogPrintf("Failed to init flip shader");
        abort();
    }

    u_xform = rw::gx2::GX2GetVertexUniformVarOffset(shader.vertexShader, "u_xform");
    u_fogData = rw::gx2::GX2GetVertexUniformVarOffset(shader.vertexShader, "u_fogData");

    memset(&colorBuffer, 0, sizeof(GX2ColorBuffer));
	colorBuffer.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
	colorBuffer.surface.use = GX2_SURFACE_USE_TEXTURE_COLOR_BUFFER_TV;
	colorBuffer.surface.width = INTERNAL_TEXTURE_WIDTH;
	colorBuffer.surface.height = INTERNAL_TEXTURE_HEIGHT;
	colorBuffer.surface.depth = 1;
	colorBuffer.surface.mipLevels = 1;
	colorBuffer.surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
	colorBuffer.surface.aa = GX2_AA_MODE1X;
	colorBuffer.surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
	colorBuffer.viewNumSlices = 1;
	GX2CalcSurfaceSizeAndAlignment(&colorBuffer.surface);
	GX2InitColorBufferRegs(&colorBuffer);

    // alloc from mem1 for speed
    colorBuffer.surface.image = GfxHeapAllocMEM1(colorBuffer.surface.imageSize, colorBuffer.surface.alignment);
	rw::gx2::addToMEM1Buffer(&colorBuffer.surface.image, colorBuffer.surface.imageSize, colorBuffer.surface.alignment);
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU, colorBuffer.surface.image, colorBuffer.surface.imageSize);

    tex.compMap = GX2_COMP_MAP(GX2_SQ_SEL_R, GX2_SQ_SEL_G, GX2_SQ_SEL_B, GX2_SQ_SEL_A);

    GX2InitSampler(&samp, GX2_TEX_CLAMP_MODE_WRAP, GX2_TEX_XY_FILTER_MODE_POINT);

    // setup a custom context
    state = (GX2ContextState*) memalign(GX2_CONTEXT_STATE_ALIGNMENT, sizeof(GX2ContextState));
    GX2SetupContextStateEx(state, TRUE);
    GX2SetContextState(state);

    GX2SetColorBuffer(&colorBuffer, GX2_RENDER_TARGET_0);
    GX2SetViewport(0.0f, 0.0f, INTERNAL_TEXTURE_WIDTH, INTERNAL_TEXTURE_HEIGHT, 0.0f, 1.0f);
    GX2SetScissor(0.0f, 0.0f, INTERNAL_TEXTURE_WIDTH, INTERNAL_TEXTURE_HEIGHT);

    GX2SetColorControl(GX2_LOGIC_OP_COPY, 0xFF, FALSE, TRUE);
    GX2SetBlendControl(GX2_RENDER_TARGET_0,
        GX2_BLEND_MODE_ONE, GX2_BLEND_MODE_ZERO,
        GX2_BLEND_COMBINE_MODE_ADD,
        TRUE,
        GX2_BLEND_MODE_ONE, GX2_BLEND_MODE_ZERO,
        GX2_BLEND_COMBINE_MODE_ADD
    );
    GX2SetDepthOnlyControl(FALSE, FALSE, GX2_COMPARE_FUNC_ALWAYS);
}

void gfxUtilsShutdown()
{
    rw::gx2::removeFromMEM1Buffer(colorBuffer.surface.image);
    if (rw::gx2::gfxInForeground) {
        GfxHeapFreeMEM1(colorBuffer.surface.image);
    }

    free(state);

    WHBGfxFreeShaderGroup(&shader);
}

void gfxUtilsFlipSurface(GX2Surface* src, uint32_t srcLevel, uint32_t srcDepth, GX2Surface* dst, uint32_t dstLevel, uint32_t dstDepth)
{
    memcpy(&tex.surface, src, sizeof(GX2Surface));
    tex.viewFirstMip = srcLevel;
    tex.viewFirstSlice = srcDepth;
    tex.viewNumMips = srcLevel + 1;
    tex.viewNumSlices = srcDepth + 1;
    GX2InitTextureRegs(&tex);

    GX2ClearColor(&colorBuffer, 0.0f, 0.0f, 0.0f, 0.0f);
    GX2SetContextState(state);

    GX2SetFetchShader(&shader.fetchShader);
    GX2SetVertexShader(shader.vertexShader);
    GX2SetPixelShader(shader.pixelShader);

	float xform[4];
	xform[0] = 2.0f / INTERNAL_TEXTURE_WIDTH;
	xform[1] = -2.0f / INTERNAL_TEXTURE_HEIGHT;
	xform[2] = -1.0f;
	xform[3] = 1.0f;
	GX2SetVertexUniformReg(u_xform, 4, xform);
    GX2SetVertexUniformReg(u_fogData, 4, fog_data);

    GX2SetPixelTexture(&tex, shader.pixelShader->samplerVars[0].location);
    GX2SetPixelSampler(&samp, shader.pixelShader->samplerVars[0].location);

    // draw the texture flipped
    GX2SetAttribBuffer(0, sizeof(flip_vertex), 10 * sizeof(float), flip_vertex);
    GX2DrawIndexedImmediateEx(GX2_PRIMITIVE_MODE_TRIANGLES, sizeof(flip_index) / 2, GX2_INDEX_TYPE_U16, flip_index, 0, 1);
    GX2DrawDone();

    // copy the finished texture
    GX2CopySurface(&colorBuffer.surface, 0, 0, dst, dstLevel, dstDepth);
}
