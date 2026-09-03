#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <malloc.h>
#include <vector>

#include "../rwbase.h"
#include "../rwerror.h"
#include "../rwplg.h"
#include "../rwrender.h"
#include "../rwengine.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"

#ifdef __WIIU__

#include "rwgx2.h"
#include "rwgx2shader.h"
#include "rwgx2impl.h"
#include "gfx_heap.h"
#include "gfx_utils.h"

#include "shaders/default.h"

#include <proc_ui/procui.h>
#include <whb/log.h>

namespace rw {
namespace gx2 {

struct GX2Globals
{
	void* commandBufferPool = NULL;
	GX2DrcRenderMode drcRenderMode;
	void* drcScanBuffer = NULL;
	uint32_t drcScanBufferSize = 0;
	GX2SurfaceFormat drcSurfaceFormat;
	GX2TVRenderMode tvRenderMode;
	void* tvScanBuffer = NULL;
	uint32_t tvScanBufferSize = 0;
	GX2SurfaceFormat tvSurfaceFormat;
	GX2ContextState* contextState = NULL;
	uint32 width = 0, height = 0;
} globals;

struct MEM1Buffer
{
	void** buffer;
	uint32 size;
	uint32 alignment;
};
std::vector<MEM1Buffer> gfxMEM1Buffers;
bool gfxInForeground = false;

struct UniformState
{
	float32 fogStart;
	float32 fogEnd;
	float32 fogRange;
	float32 fogDisable;

	RGBAf   fogColor;
};

struct UniformScene
{
	float32 proj[16];
	float32 view[16];
};

#define MAX_LIGHTS 8

struct UniformObject
{
	RawMatrix    world;
	RGBAf        ambLight;
	struct {
		float type;
		float radius;
		float minusCosAngle;
		float hardSpot;
	} lightParams[MAX_LIGHTS];
	V4d lightPosition[MAX_LIGHTS];
	V4d lightDirection[MAX_LIGHTS];
	RGBAf lightColor[MAX_LIGHTS];
};

static UniformState uniformState;
static UniformScene uniformScene;
static UniformObject uniformObject;

GX2Texture whitetex;
GX2Sampler whitesamp;

// State
int32 u_fogData;
int32 u_fogColor;

// Scene
int32 u_proj;
int32 u_view;

// Object
int32 u_world;
int32 u_ambLight;
int32 u_lightParams;
int32 u_lightPosition;
int32 u_lightDirection;
int32 u_lightColor;

int32 u_matColor;
int32 u_surfProps;

Shader *defaultShader;

static bool32 stateDirty = 1;
static bool32 sceneDirty = 1;
static bool32 objectDirty = 1;

struct RwRasterStateCache
{
	Raster *raster;
	Texture::Addressing addressingU;
	Texture::Addressing addressingV;
	Texture::FilterMode filter;
};

#define MAXNUMSTAGES 8

// cached RW render states
struct RwStateCache {
	bool32 vertexAlpha;
	uint32 alphaTestEnable;
	uint32 alphaFunc;
	uint32 alphaRef;
	bool32 textureAlpha;
	bool32 blendEnable;
	uint32 srcblend, destblend;
	uint32 zwrite;
	uint32 ztest;
	uint32 cullmode;
	uint32 stencilenable;
	uint32 stencilpass;
	uint32 stencilfail;
	uint32 stencilzfail;
	uint32 stencilfunc;
	uint32 stencilref;
	uint32 stencilmask;
	uint32 stencilwritemask;
	uint32 fogEnable;
	float32 fogStart;
	float32 fogEnd;

	// emulation of PS2 GS
	bool32 gsalpha;
	uint32 gsalpharef;

	RwRasterStateCache texstage[MAXNUMSTAGES];
};
static RwStateCache rwStateCache;

enum
{
	// states
	RWGX2_BLEND,
	RWGX2_SRCBLEND,
	RWGX2_DESTBLEND,
	RWGX2_DEPTHTEST,
	RWGX2_DEPTHFUNC,
	RWGX2_DEPTHMASK,
	RWGX2_CULLFRONT,
	RWGX2_CULLBACK,
	RWGX2_STENCIL,
	RWGX2_STENCILFUNC,
	RWGX2_STENCILFAIL,
	RWGX2_STENCILZFAIL,
	RWGX2_STENCILPASS,
	RWGX2_STENCILREF,
	RWGX2_STENCILMASK,
	RWGX2_STENCILWRITEMASK,
	RWGX2_ALPHATEST,
	RWGX2_ALPHAFUNC,
	RWGX2_ALPHAREF,

	// uniforms
	RWGX2_FOG,
	RWGX2_FOGSTART,
	RWGX2_FOGEND,
	RWGX2_FOGCOLOR,

	RWGX2_NUM_STATES
};
static bool uniformStateDirty[RWGX2_NUM_STATES];

struct GX2State {
	bool32 blendEnable;
	GX2BlendMode srcblend, destblend;

	bool32 depthTestEnable;
	GX2CompareFunction depthFunc;
	uint32 depthMask;

	bool32 cullFront;
	bool32 cullBack;

	bool32 alphaTestEnable;
	GX2CompareFunction alphaFunc;
	uint32 alphaRef;

	bool32 stencilEnable;
	GX2CompareFunction stencilFunc;
	GX2StencilFunction stencilFail;
	GX2StencilFunction stencilZFail;
	GX2StencilFunction stencilPass;
	uint8 stencilRef;
	uint8 stencilMask;
	uint8 stencilWriteMask;
	
};
static GX2State curGX2State, oldGX2State;

static GX2BlendMode blendMap[] = {
	GX2_BLEND_MODE_ZERO,
	GX2_BLEND_MODE_ZERO,
	GX2_BLEND_MODE_ONE,
	GX2_BLEND_MODE_SRC_COLOR,
	GX2_BLEND_MODE_INV_SRC_COLOR,
	GX2_BLEND_MODE_SRC_ALPHA,
	GX2_BLEND_MODE_INV_SRC_ALPHA,
	GX2_BLEND_MODE_DST_ALPHA,
	GX2_BLEND_MODE_INV_DST_ALPHA,
	GX2_BLEND_MODE_DST_COLOR,
	GX2_BLEND_MODE_INV_DST_COLOR,
	GX2_BLEND_MODE_SRC_ALPHA_SAT,
};

static GX2StencilFunction stencilOpMap[] = {
	GX2_STENCIL_FUNCTION_KEEP, // actually invalid
	GX2_STENCIL_FUNCTION_KEEP,
	GX2_STENCIL_FUNCTION_ZERO,
	GX2_STENCIL_FUNCTION_REPLACE,
	GX2_STENCIL_FUNCTION_INCR_CLAMP,
	GX2_STENCIL_FUNCTION_DECR_CLAMP,
	GX2_STENCIL_FUNCTION_INV,
	GX2_STENCIL_FUNCTION_INCR_WRAP,
	GX2_STENCIL_FUNCTION_DECR_WRAP,
};

static GX2CompareFunction stencilFuncMap[] = {
	GX2_COMPARE_FUNC_NEVER,	// actually invalid
	GX2_COMPARE_FUNC_NEVER,
	GX2_COMPARE_FUNC_LESS,
	GX2_COMPARE_FUNC_EQUAL,
	GX2_COMPARE_FUNC_LEQUAL,
	GX2_COMPARE_FUNC_GREATER,
	GX2_COMPARE_FUNC_NOT_EQUAL,
	GX2_COMPARE_FUNC_GEQUAL,
	GX2_COMPARE_FUNC_ALWAYS,
};

std::vector<void*> freeBuf;

void
addToVertexFreeBuffer(void* vert)
{
	freeBuf.push_back(vert);
}

void
freeVertexBuffers(void)
{
	for (uint32_t i = 0; i < freeBuf.size(); i++) {
		free(freeBuf[i]);
	}

	freeBuf.clear();
}

/*
 * GX2 state cache
 */

void
setGX2RenderState(uint32 state, uint32 value)
{
	switch(state){
	case RWGX2_BLEND:  curGX2State.blendEnable = value; break;
	case RWGX2_SRCBLEND: curGX2State.srcblend = (GX2BlendMode) value; break;
	case RWGX2_DESTBLEND: curGX2State.destblend = (GX2BlendMode) value; break;
	case RWGX2_DEPTHTEST: curGX2State.depthTestEnable = value; break;
	case RWGX2_DEPTHFUNC:  curGX2State.depthFunc = (GX2CompareFunction) value;  break;
	case RWGX2_DEPTHMASK: curGX2State.depthMask = value; break;
	case RWGX2_CULLFRONT: curGX2State.cullFront = value; break;
	case RWGX2_CULLBACK: curGX2State.cullBack = value; break;
	case RWGX2_ALPHATEST: curGX2State.alphaTestEnable = value; break;
	case RWGX2_ALPHAFUNC: curGX2State.alphaFunc = (GX2CompareFunction) value; break;
	case RWGX2_ALPHAREF: curGX2State.alphaRef = value; break;
	case RWGX2_STENCIL: curGX2State.stencilEnable = value; break;
	case RWGX2_STENCILFUNC: curGX2State.stencilFunc = (GX2CompareFunction) value; break;
	case RWGX2_STENCILFAIL: curGX2State.stencilFail = (GX2StencilFunction) value; break;
	case RWGX2_STENCILZFAIL: curGX2State.stencilZFail = (GX2StencilFunction) value; break;
	case RWGX2_STENCILPASS: curGX2State.stencilPass = (GX2StencilFunction) value; break;
	case RWGX2_STENCILREF: curGX2State.stencilRef = (uint8) value; break;
	case RWGX2_STENCILMASK: curGX2State.stencilMask = (uint8) value; break;
	case RWGX2_STENCILWRITEMASK: curGX2State.stencilWriteMask = (uint8) value; break;
	}
}

void
flushGX2RenderState(void)
{
	if(oldGX2State.blendEnable != curGX2State.blendEnable){
		oldGX2State.blendEnable = curGX2State.blendEnable;
		GX2SetColorControl(GX2_LOGIC_OP_COPY, oldGX2State.blendEnable ? 0xFF : 0, FALSE, TRUE);
	}

	if(oldGX2State.srcblend != curGX2State.srcblend ||
	   oldGX2State.destblend != curGX2State.destblend){
		oldGX2State.srcblend = curGX2State.srcblend;
		oldGX2State.destblend = curGX2State.destblend;
		GX2SetBlendControl(GX2_RENDER_TARGET_0,
					oldGX2State.srcblend,
					oldGX2State.destblend,
					GX2_BLEND_COMBINE_MODE_ADD,
					TRUE,
					oldGX2State.srcblend,
					oldGX2State.destblend,
					GX2_BLEND_COMBINE_MODE_ADD);
	}

	if(oldGX2State.depthTestEnable != curGX2State.depthTestEnable ||
		oldGX2State.depthFunc != curGX2State.depthFunc ||
		oldGX2State.depthMask != curGX2State.depthMask ||
		oldGX2State.stencilEnable != curGX2State.stencilEnable ||
		oldGX2State.stencilFunc != curGX2State.stencilFunc ||
		oldGX2State.stencilFail != curGX2State.stencilFail ||
		oldGX2State.stencilZFail != curGX2State.stencilZFail ||
		oldGX2State.stencilPass != curGX2State.stencilPass)
	{
		oldGX2State.depthTestEnable = curGX2State.depthTestEnable;
		oldGX2State.depthFunc = curGX2State.depthFunc;
		oldGX2State.depthMask = curGX2State.depthMask;
		oldGX2State.stencilEnable = curGX2State.stencilEnable;
		oldGX2State.stencilFunc = curGX2State.stencilFunc;
		oldGX2State.stencilFail = curGX2State.stencilFail;
		oldGX2State.stencilZFail = curGX2State.stencilZFail;
		oldGX2State.stencilPass = curGX2State.stencilPass;
		GX2SetDepthStencilControl(
			// depth
			oldGX2State.depthTestEnable, oldGX2State.depthMask, oldGX2State.depthFunc,
			// stencil
			oldGX2State.stencilEnable, TRUE,
			oldGX2State.stencilFunc, oldGX2State.stencilPass, oldGX2State.stencilZFail, oldGX2State.stencilFail,
			oldGX2State.stencilFunc, oldGX2State.stencilPass, oldGX2State.stencilZFail, oldGX2State.stencilFail);
	}

	if(oldGX2State.stencilRef != curGX2State.stencilRef ||
		oldGX2State.stencilMask != curGX2State.stencilMask ||
		oldGX2State.stencilWriteMask != curGX2State.stencilWriteMask)
	{
		oldGX2State.stencilRef = curGX2State.stencilRef;
		oldGX2State.stencilMask = curGX2State.stencilMask;
		oldGX2State.stencilWriteMask = curGX2State.stencilWriteMask;
		GX2SetStencilMask(oldGX2State.stencilMask, oldGX2State.stencilWriteMask, oldGX2State.stencilRef,
			oldGX2State.stencilMask, oldGX2State.stencilWriteMask, oldGX2State.stencilRef);
	}

	if(oldGX2State.cullFront != curGX2State.cullFront
		|| oldGX2State.cullBack != curGX2State.cullBack){
		oldGX2State.cullFront = curGX2State.cullFront;
		oldGX2State.cullBack = curGX2State.cullBack;
		GX2SetCullOnlyControl(GX2_FRONT_FACE_CCW, oldGX2State.cullFront, oldGX2State.cullBack);
	}

	if (oldGX2State.alphaTestEnable != curGX2State.alphaTestEnable
		|| oldGX2State.alphaFunc != curGX2State.alphaFunc
		|| oldGX2State.alphaRef != curGX2State.alphaRef) {
		oldGX2State.alphaTestEnable = curGX2State.alphaTestEnable;
		oldGX2State.alphaFunc = curGX2State.alphaFunc;
		oldGX2State.alphaRef = curGX2State.alphaRef;
		GX2SetAlphaTest(oldGX2State.alphaTestEnable, oldGX2State.alphaFunc, oldGX2State.alphaRef/255.0f);
	}
}

void
setAlphaBlend(bool32 enable)
{
	if(rwStateCache.blendEnable != enable){
		rwStateCache.blendEnable = enable;
		setGX2RenderState(RWGX2_BLEND, enable);
	}
}

bool32
getAlphaBlend(void)
{
	return rwStateCache.blendEnable;
}

bool32
getAlphaTest(void)
{
	return rwStateCache.alphaTestEnable;
}

static void
setDepthTest(bool32 enable)
{
	if(rwStateCache.ztest != enable){
		rwStateCache.ztest = enable;
		if(rwStateCache.zwrite && !enable){
			// If we still want to write, enable but set mode to always
			setGX2RenderState(RWGX2_DEPTHTEST, true);
			setGX2RenderState(RWGX2_DEPTHFUNC, GX2_COMPARE_FUNC_ALWAYS);
		}else{
			setGX2RenderState(RWGX2_DEPTHTEST, rwStateCache.ztest);
			setGX2RenderState(RWGX2_DEPTHFUNC, GX2_COMPARE_FUNC_LEQUAL);
		}
	}
}

static void
setDepthWrite(bool32 enable)
{
	enable = enable ? true : false;
	if(rwStateCache.zwrite != enable){
		rwStateCache.zwrite = enable;
		if(enable && !rwStateCache.ztest){
			// Have to switch on ztest so writing can work
			setGX2RenderState(RWGX2_DEPTHTEST, true);
			setGX2RenderState(RWGX2_DEPTHFUNC, GX2_COMPARE_FUNC_ALWAYS);
		}
		setGX2RenderState(RWGX2_DEPTHMASK, rwStateCache.zwrite);
	}
}

static void
setAlphaTest(bool32 enable)
{
	if(rwStateCache.alphaTestEnable != enable){
		rwStateCache.alphaTestEnable = enable;
		setGX2RenderState(RWGX2_ALPHATEST, enable);
	}
}

static GX2CompareFunction alphafuncMap[] = {
	GX2_COMPARE_FUNC_ALWAYS,
	GX2_COMPARE_FUNC_GEQUAL,
	GX2_COMPARE_FUNC_LESS
};

static void
setAlphaTestFunction(uint32 function)
{
	if(rwStateCache.alphaFunc != function){
		rwStateCache.alphaFunc = function;
		setGX2RenderState(RWGX2_ALPHAFUNC, alphafuncMap[function]);
	}
}

static void
setVertexAlpha(bool32 enable)
{
	if(rwStateCache.vertexAlpha != enable){
		if(!rwStateCache.textureAlpha){
			setAlphaBlend(enable);
			setAlphaTest(enable);
		}
		rwStateCache.vertexAlpha = enable;
	}
}

static GX2TexXYFilterMode filterConvMap_NoMIP[] = {
	GX2_TEX_XY_FILTER_MODE_POINT, 
	GX2_TEX_XY_FILTER_MODE_POINT, GX2_TEX_XY_FILTER_MODE_LINEAR,
	GX2_TEX_XY_FILTER_MODE_POINT, GX2_TEX_XY_FILTER_MODE_LINEAR,
	GX2_TEX_XY_FILTER_MODE_POINT, GX2_TEX_XY_FILTER_MODE_LINEAR
};

static GX2TexMipFilterMode filterConvMap_MIP[] = {
	GX2_TEX_MIP_FILTER_MODE_NONE, 
	GX2_TEX_MIP_FILTER_MODE_POINT, GX2_TEX_MIP_FILTER_MODE_LINEAR,
	GX2_TEX_MIP_FILTER_MODE_POINT, GX2_TEX_MIP_FILTER_MODE_POINT,
	GX2_TEX_MIP_FILTER_MODE_LINEAR, GX2_TEX_MIP_FILTER_MODE_LINEAR
};

static GX2TexClampMode addressConvMap[] = {
	GX2_TEX_CLAMP_MODE_WRAP, GX2_TEX_CLAMP_MODE_WRAP, GX2_TEX_CLAMP_MODE_MIRROR,
	GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_CLAMP_MODE_CLAMP_BORDER
};

static void
setFilterMode(uint32 stage, int32 filter, int32 maxAniso = 1)
{
	if(rwStateCache.texstage[stage].filter != (Texture::FilterMode)filter)
	{
		rwStateCache.texstage[stage].filter = (Texture::FilterMode)filter;
		Raster *raster = rwStateCache.texstage[stage].raster;
		if(raster)
		{
			GX2Raster *natras = PLUGINOFFSET(GX2Raster, rwStateCache.texstage[stage].raster, nativeRasterOffset);
			if((natras->filterMode != filter || natras->maxAnisotropy != maxAniso) && natras->sampler)
			{
				GX2InitSamplerXYFilter(natras->sampler, filterConvMap_NoMIP[filter], filterConvMap_NoMIP[filter], (GX2TexAnisoRatio) maxAniso);
				if(natras->autogenMipmap || natras->numLevels > 1) {
					GX2InitSamplerZMFilter(natras->sampler, GX2_TEX_Z_FILTER_MODE_NONE, filterConvMap_MIP[filter]);
				}
				natras->filterMode = filter;
				natras->maxAnisotropy = maxAniso;
			}
		}
	}
}

static void
setAddressU(uint32 stage, int32 addressing)
{
	if(rwStateCache.texstage[stage].addressingU != (Texture::Addressing)addressing)
	{
		rwStateCache.texstage[stage].addressingU = (Texture::Addressing)addressing;
		Raster *raster = rwStateCache.texstage[stage].raster;
		if(raster)
		{
			GX2Raster *natras = PLUGINOFFSET(GX2Raster, raster, nativeRasterOffset);
			if(natras->addressU != addressing && natras->sampler)
			{
				GX2InitSamplerClamping(natras->sampler, addressConvMap[addressing], addressConvMap[natras->addressV], GX2_TEX_CLAMP_MODE_WRAP);
				natras->addressU = addressing;
			}
		}
	}
}

static void
setAddressV(uint32 stage, int32 addressing)
{
	if(rwStateCache.texstage[stage].addressingV != (Texture::Addressing)addressing)
	{
		rwStateCache.texstage[stage].addressingV = (Texture::Addressing)addressing;
		Raster *raster = rwStateCache.texstage[stage].raster;
		if(raster){
			GX2Raster *natras = PLUGINOFFSET(GX2Raster, raster, nativeRasterOffset);
			if(natras->addressV != addressing && natras->sampler)
			{
				GX2InitSamplerClamping(natras->sampler, addressConvMap[natras->addressU], addressConvMap[addressing], GX2_TEX_CLAMP_MODE_WRAP);
				natras->addressV = addressing;
			}
		}
	}
}

static void
setTexAndSampler(GX2Texture* tex, GX2Sampler* samp, uint32 stage)
{
	if (stage == 0 && currentShader->samplerLocation != -1)
	{
		GX2SetPixelTexture(tex, currentShader->samplerLocation);
		GX2SetPixelSampler(samp, currentShader->samplerLocation);
	}
	else if (stage == 1 && currentShader->sampler2Location != -1)
	{
		GX2SetPixelTexture(tex, currentShader->sampler2Location);
		GX2SetPixelSampler(samp, currentShader->sampler2Location);
	}
}

static void
setRasterStageOnly(uint32 stage, Raster *raster)
{
	bool32 alpha;
	if(raster != rwStateCache.texstage[stage].raster)
	{
		rwStateCache.texstage[stage].raster = raster;
		if(raster)
		{
			assert(raster->platform == PLATFORM_GX2);
			GX2Raster *natras = PLUGINOFFSET(GX2Raster, raster, nativeRasterOffset);
			
			setTexAndSampler((GX2Texture*) natras->texture, natras->sampler, stage);

			rwStateCache.texstage[stage].filter = (rw::Texture::FilterMode)natras->filterMode;
			rwStateCache.texstage[stage].addressingU = (rw::Texture::Addressing)natras->addressU;
			rwStateCache.texstage[stage].addressingV = (rw::Texture::Addressing)natras->addressV;

			alpha = natras->hasAlpha;
		}
		else
		{
			setTexAndSampler(&whitetex, &whitesamp, stage);
			alpha = 0;
		}

		if(stage == 0)
		{
			if(alpha != rwStateCache.textureAlpha)
			{
				rwStateCache.textureAlpha = alpha;
				if(!rwStateCache.vertexAlpha)
				{
					setAlphaBlend(alpha);
					setAlphaTest(alpha);
				}
			}
		}
	}
}

static void
setRasterStage(uint32 stage, Raster *raster)
{
	bool32 alpha;
	if(raster != rwStateCache.texstage[stage].raster)
	{
		rwStateCache.texstage[stage].raster = raster;
		if(raster)
		{
			assert(raster->platform == PLATFORM_GX2);
			GX2Raster *natras = PLUGINOFFSET(GX2Raster, raster, nativeRasterOffset);

			uint32 filter = rwStateCache.texstage[stage].filter;
			uint32 addrU = rwStateCache.texstage[stage].addressingU;
			uint32 addrV = rwStateCache.texstage[stage].addressingV;
			if(natras->filterMode != filter)
			{
				GX2InitSamplerXYFilter(natras->sampler, filterConvMap_NoMIP[filter], filterConvMap_NoMIP[filter], (GX2TexAnisoRatio) natras->maxAnisotropy);
				if(natras->autogenMipmap || natras->numLevels > 1) {
					GX2InitSamplerZMFilter(natras->sampler, GX2_TEX_Z_FILTER_MODE_POINT, filterConvMap_MIP[filter]);
				}
				natras->filterMode = filter;
			}
			if(natras->addressU != addrU || natras->addressV != addrV)
			{
				GX2InitSamplerClamping(natras->sampler, addressConvMap[addrU], addressConvMap[addrV], GX2_TEX_CLAMP_MODE_WRAP);
				natras->addressU = addrU;
				natras->addressV = addrV;
			}
			alpha = natras->hasAlpha;

			setTexAndSampler((GX2Texture*) natras->texture, natras->sampler, stage);
		}else
		{
			setTexAndSampler(&whitetex, &whitesamp, stage);
			alpha = 0;
		}

		if(stage == 0){
			if(alpha != rwStateCache.textureAlpha){
				rwStateCache.textureAlpha = alpha;
				if(!rwStateCache.vertexAlpha){
					setAlphaBlend(alpha);
					setAlphaTest(alpha);
				}
			}
		}
	}
}

void
evictRaster(Raster *raster)
{
	int i;
	for(i = 0; i < MAXNUMSTAGES; i++){
		//assert(rwStateCache.texstage[i].raster != raster);
		if(rwStateCache.texstage[i].raster != raster)
			continue;
		setRasterStage(i, nil);
	}
}

void
setTexture(int32 stage, Texture *tex)
{
	if(tex == nil || tex->raster == nil){
		setRasterStage(stage, nil);
		return;
	}
	setRasterStageOnly(stage, tex->raster);
	setFilterMode(stage, tex->getFilter());
	setAddressU(stage, tex->getAddressU());
	setAddressV(stage, tex->getAddressV());
}


static void
setRenderState(int32 state, void *pvalue)
{
	uint32 value = (uint32)(uintptr)pvalue;
	switch(state){
	case TEXTURERASTER:
		setRasterStage(0, (Raster*)pvalue);
		break;
	case TEXTUREADDRESS:
		setAddressU(0, value);
		setAddressV(0, value);
		break;
	case TEXTUREADDRESSU:
		setAddressU(0, value);
		break;
	case TEXTUREADDRESSV:
		setAddressV(0, value);
		break;
	case TEXTUREFILTER:
		setFilterMode(0, value);
		break;
	case VERTEXALPHA:
		setVertexAlpha(value);
		break;
	case SRCBLEND:
		if(rwStateCache.srcblend != value){
			rwStateCache.srcblend = value;
			setGX2RenderState(RWGX2_SRCBLEND, blendMap[rwStateCache.srcblend]);
		}
		break;
	case DESTBLEND:
		if(rwStateCache.destblend != value){
			rwStateCache.destblend = value;
			setGX2RenderState(RWGX2_DESTBLEND, blendMap[rwStateCache.destblend]);
		}
		break;
	case ZTESTENABLE:
		setDepthTest(value);
		break;
	case ZWRITEENABLE:
		setDepthWrite(value);
		break;
	case FOGENABLE:
		if(rwStateCache.fogEnable != value){
			rwStateCache.fogEnable = value;
			uniformStateDirty[RWGX2_FOG] = true;
			stateDirty = 1;
		}
		break;
	case FOGCOLOR:
		// no cache check here...too lazy
		RGBA c;
		c.red = value;
		c.green = value>>8;
		c.blue = value>>16;
		c.alpha = value>>24;
		convColor(&uniformState.fogColor, &c);
		uniformStateDirty[RWGX2_FOGCOLOR] = true;
		stateDirty = 1;
		break;
	case CULLMODE:
		if(rwStateCache.cullmode != value){
			rwStateCache.cullmode = value;
			if(rwStateCache.cullmode == CULLNONE) {
				setGX2RenderState(RWGX2_CULLFRONT, false);
				setGX2RenderState(RWGX2_CULLBACK, false);
			} else{
				setGX2RenderState(RWGX2_CULLFRONT, rwStateCache.cullmode == CULLBACK ? false : true);
				setGX2RenderState(RWGX2_CULLBACK, rwStateCache.cullmode == CULLBACK ? true : false);
			}
		}
		break;
	
	case STENCILENABLE:
		if(rwStateCache.stencilenable != value){
			rwStateCache.stencilenable = value;
			setGX2RenderState(RWGX2_STENCIL, value);
		}
		break;
	case STENCILFAIL:
		if(rwStateCache.stencilfail != value){
			rwStateCache.stencilfail = value;
			setGX2RenderState(RWGX2_STENCILFAIL, stencilOpMap[value]);
		}
		break;
	case STENCILZFAIL:
		if(rwStateCache.stencilzfail != value){
			rwStateCache.stencilzfail = value;
			setGX2RenderState(RWGX2_STENCILZFAIL, stencilOpMap[value]);
		}
		break;
	case STENCILPASS:
		if(rwStateCache.stencilpass != value){
			rwStateCache.stencilpass = value;
			setGX2RenderState(RWGX2_STENCILPASS, stencilOpMap[value]);
		}
		break;
	case STENCILFUNCTION:
		if(rwStateCache.stencilfunc != value){
			rwStateCache.stencilfunc = value;
			setGX2RenderState(RWGX2_STENCILFUNC, stencilFuncMap[value]);
		}
		break;
	case STENCILFUNCTIONREF:
		if(rwStateCache.stencilref != value){
			rwStateCache.stencilref = value;
			setGX2RenderState(RWGX2_STENCILREF, value);
		}
		break;
	case STENCILFUNCTIONMASK:
		if(rwStateCache.stencilmask != value){
			rwStateCache.stencilmask = value;
			setGX2RenderState(RWGX2_STENCILMASK, value);
		}
		break;
	case STENCILFUNCTIONWRITEMASK:
		if(rwStateCache.stencilwritemask != value){
			rwStateCache.stencilwritemask = value;
			setGX2RenderState(RWGX2_STENCILWRITEMASK, value);
		}
		break;

	case ALPHATESTFUNC:
		setAlphaTestFunction(value);
		break;
	case ALPHATESTREF:
		if(rwStateCache.alphaRef != value){
			rwStateCache.alphaRef = value;
			setGX2RenderState(RWGX2_ALPHAREF, value);
		}
		break;
	case GSALPHATEST:
		rwStateCache.gsalpha = value;
		break;
	case GSALPHATESTREF:
		rwStateCache.gsalpharef = value;
	}
}

static void*
getRenderState(int32 state)
{
	uint32 val;
	RGBA rgba;
	switch(state)
	{
	case TEXTURERASTER:
		return rwStateCache.texstage[0].raster;
	case TEXTUREADDRESS:
		if(rwStateCache.texstage[0].addressingU == rwStateCache.texstage[0].addressingV)
			val = rwStateCache.texstage[0].addressingU;
		else
			val = 0;	// invalid
		break;
	case TEXTUREADDRESSU:
		val = rwStateCache.texstage[0].addressingU;
		break;
	case TEXTUREADDRESSV:
		val = rwStateCache.texstage[0].addressingV;
		break;
	case TEXTUREFILTER:
		val = rwStateCache.texstage[0].filter;
		break;

	case VERTEXALPHA:
		val = rwStateCache.vertexAlpha;
		break;
	case SRCBLEND:
		val = rwStateCache.srcblend;
		break;
	case DESTBLEND:
		val = rwStateCache.destblend;
		break;
	case ZTESTENABLE:
		val = rwStateCache.ztest;
		break;
	case ZWRITEENABLE:
		val = rwStateCache.zwrite;
		break;
	case FOGENABLE:
		val = rwStateCache.fogEnable;
		break;
	case FOGCOLOR:
		convColor(&rgba, &uniformState.fogColor);
		val = RWRGBAINT(rgba.red, rgba.green, rgba.blue, rgba.alpha);
		break;
	case CULLMODE:
		val = rwStateCache.cullmode;
		break;
	
	case STENCILENABLE:
		val = rwStateCache.stencilenable;
		break;
	case STENCILFAIL:
		val = rwStateCache.stencilfail;
		break;
	case STENCILZFAIL:
		val = rwStateCache.stencilzfail;
		break;
	case STENCILPASS:
		val = rwStateCache.stencilpass;
		break;
	case STENCILFUNCTION:
		val = rwStateCache.stencilfunc;
		break;
	case STENCILFUNCTIONREF:
		val = rwStateCache.stencilref;
		break;
	case STENCILFUNCTIONMASK:
		val = rwStateCache.stencilmask;
		break;
	case STENCILFUNCTIONWRITEMASK:
		val = rwStateCache.stencilwritemask;
		break;
	
	case ALPHATESTFUNC:
		val = rwStateCache.alphaFunc;
		break;
	case ALPHATESTREF:
		val = rwStateCache.alphaRef;
		break;
	case GSALPHATEST:
		val = rwStateCache.gsalpha;
		break;
	case GSALPHATESTREF:
		val = rwStateCache.gsalpharef;
		break;
	default:
		val = 0;
	}
	return (void*)(uintptr)val;
}


static void
resetRenderState(void)
{	
	uniformState.fogDisable = 1.0f;
	uniformState.fogStart = 0.0f;
	uniformState.fogEnd = 0.0f;
	uniformState.fogRange = 0.0f;
	uniformState.fogColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	rwStateCache.gsalpha = 0;
	rwStateCache.gsalpharef = 128;
	stateDirty = 1;

	rwStateCache.vertexAlpha = 0;
	rwStateCache.textureAlpha = 0;

	memset(&oldGX2State, 0xFE, sizeof(oldGX2State));

	rwStateCache.blendEnable = 0;
	setGX2RenderState(RWGX2_BLEND, false);
	rwStateCache.srcblend = BLENDSRCALPHA;
	rwStateCache.destblend = BLENDINVSRCALPHA;
	setGX2RenderState(RWGX2_SRCBLEND, blendMap[rwStateCache.srcblend]);
	setGX2RenderState(RWGX2_DESTBLEND, blendMap[rwStateCache.destblend]);

	rwStateCache.zwrite = true;
	setGX2RenderState(RWGX2_DEPTHMASK, rwStateCache.zwrite);

	rwStateCache.ztest = 1;
	setGX2RenderState(RWGX2_DEPTHTEST, true);
	setGX2RenderState(RWGX2_DEPTHFUNC, GX2_COMPARE_FUNC_LEQUAL);

	rwStateCache.cullmode = CULLNONE;
	setGX2RenderState(RWGX2_CULLFRONT, false);
	setGX2RenderState(RWGX2_CULLBACK, false);

	rwStateCache.stencilenable = 0;
	setGX2RenderState(RWGX2_STENCIL, FALSE);
	rwStateCache.stencilfail = STENCILKEEP;
	setGX2RenderState(RWGX2_STENCILFAIL, GX2_STENCIL_FUNCTION_KEEP);
	rwStateCache.stencilzfail = STENCILKEEP;
	setGX2RenderState(RWGX2_STENCILZFAIL, GX2_STENCIL_FUNCTION_KEEP);
	rwStateCache.stencilpass = STENCILKEEP;
	setGX2RenderState(RWGX2_STENCILPASS, GX2_STENCIL_FUNCTION_KEEP);
	rwStateCache.stencilfunc = STENCILALWAYS;
	setGX2RenderState(RWGX2_STENCILFUNC, GX2_COMPARE_FUNC_ALWAYS);
	rwStateCache.stencilref = 0;
	setGX2RenderState(RWGX2_STENCILREF, 0);
	rwStateCache.stencilmask = 0xFFFFFFFF;
	setGX2RenderState(RWGX2_STENCILMASK, 0xFFFFFFFF);
	rwStateCache.stencilwritemask = 0xFFFFFFFF;
	setGX2RenderState(RWGX2_STENCILWRITEMASK, 0xFFFFFFFF);

	rwStateCache.alphaFunc = ALPHAGREATEREQUAL;
	rwStateCache.alphaRef = 10;
	rwStateCache.alphaTestEnable = false;
	setGX2RenderState(RWGX2_ALPHATEST, false);
	setGX2RenderState(RWGX2_ALPHAFUNC, GX2_COMPARE_FUNC_GEQUAL);
	setGX2RenderState(RWGX2_ALPHAREF, 10);
}

void
setWorldMatrix(Matrix *mat)
{
	convMatrix(&uniformObject.world, mat);
	setUniform(u_world, &uniformObject.world);
	objectDirty = 1;
}

int32
setLights(WorldLights *lightData)
{
	int i, n;
	Light *l;
	int32 bits;

	uniformObject.ambLight = lightData->ambient;

	bits = 0;

	if(lightData->numAmbients)
		bits |= VSLIGHT_AMBIENT;

	n = 0;
	for(i = 0; i < lightData->numDirectionals && i < 8; i++){
		l = lightData->directionals[i];
		uniformObject.lightParams[n].type = 1.0f;
		uniformObject.lightColor[n] = l->color;
		memcpy(&uniformObject.lightDirection[n], &l->getFrame()->getLTM()->at, sizeof(V3d));
		bits |= VSLIGHT_DIRECT;
		n++;
		if(n >= MAX_LIGHTS)
			goto out;
	}

	for(i = 0; i < lightData->numLocals; i++){
		Light *l = lightData->locals[i];

		switch(l->getType()){
		case Light::POINT:
			uniformObject.lightParams[n].type = 2.0f;
			uniformObject.lightParams[n].radius = l->radius;
			uniformObject.lightColor[n] = l->color;
			memcpy(&uniformObject.lightPosition[n], &l->getFrame()->getLTM()->pos, sizeof(V3d));
			bits |= VSLIGHT_POINT;
			n++;
			if(n >= MAX_LIGHTS)
				goto out;
			break;
		case Light::SPOT:
		case Light::SOFTSPOT:
			uniformObject.lightParams[n].type = 3.0f;
			uniformObject.lightParams[n].minusCosAngle = l->minusCosAngle;
			uniformObject.lightParams[n].radius = l->radius;
			uniformObject.lightColor[n] = l->color;
			memcpy(&uniformObject.lightPosition[n], &l->getFrame()->getLTM()->pos, sizeof(V3d));
			memcpy(&uniformObject.lightDirection[n], &l->getFrame()->getLTM()->at, sizeof(V3d));
			// lower bound of falloff
			if(l->getType() == Light::SOFTSPOT)
				uniformObject.lightParams[n].hardSpot = 0.0f;
			else
				uniformObject.lightParams[n].hardSpot = 1.0f;
			bits |= VSLIGHT_SPOT;
			n++;
			if(n >= MAX_LIGHTS)
				goto out;
			break;
		}
	}

	uniformObject.lightParams[n].type = 0.0f;

	setUniform(u_ambLight, &uniformObject.ambLight);
	setUniform(u_lightParams, uniformObject.lightParams);
	// setUniform(u_lightPosition, uniformObject.lightPosition);
	setUniform(u_lightDirection, uniformObject.lightDirection);
	setUniform(u_lightColor, uniformObject.lightColor);
out:
	objectDirty = 1;
	return bits;
}

void
setProjectionMatrix(float32 *mat)
{
	memcpy(&uniformScene.proj, mat, 64);
	setUniform(u_proj, uniformScene.proj);
	sceneDirty = 1;
}

void
setViewMatrix(float32 *mat)
{
	memcpy(&uniformScene.view, mat, 64);
	setUniform(u_view, uniformScene.view);
	sceneDirty = 1;
}

Shader *lastShaderUploaded;

void
setMaterial(const RGBA &color, const SurfaceProperties &surfaceprops, float extraSurfProp)
{
	rw::RGBAf col;
	convColor(&col, &color);
	setUniform(u_matColor, &col);

	float surfProps[4];
	surfProps[0] = surfaceprops.ambient;
	surfProps[1] = surfaceprops.specular;
	surfProps[2] = surfaceprops.diffuse;
	surfProps[3] = extraSurfProp;
	setUniform(u_surfProps, surfProps);
}

void
flushCache(void)
{
	flushGX2RenderState();

	// what's this doing here??
	uniformState.fogDisable = rwStateCache.fogEnable ? 0.0f : 1.0f;
	uniformState.fogStart = rwStateCache.fogStart;
	uniformState.fogEnd = rwStateCache.fogEnd;
	uniformState.fogRange = 1.0f/(rwStateCache.fogStart - rwStateCache.fogEnd);

	if(uniformStateDirty[RWGX2_FOG] ||
	   uniformStateDirty[RWGX2_FOGSTART] ||
	   uniformStateDirty[RWGX2_FOGEND]) {
		float fogData[4] = {
			uniformState.fogStart,
			uniformState.fogEnd,
			uniformState.fogRange,
			uniformState.fogDisable
		};
		setUniform(u_fogData, fogData);
		uniformStateDirty[RWGX2_FOG] = false;
		uniformStateDirty[RWGX2_FOGSTART] = false;
		uniformStateDirty[RWGX2_FOGEND] = false;
	}

	if(uniformStateDirty[RWGX2_FOGCOLOR]) {
		setUniform(u_fogColor, &uniformState.fogColor);
		uniformStateDirty[RWGX2_FOGCOLOR] = false;
	}

	flushUniforms();

	// Textures need to be set again if the shader changes
	if (lastShaderUploaded != currentShader) {
		// we currently support 2 texture stages
		for (int stage = 0; stage < 2; stage++) {
			if (rwStateCache.texstage[stage].raster) {
				GX2Raster *natras = PLUGINOFFSET(GX2Raster, rwStateCache.texstage[stage].raster, nativeRasterOffset);
				setTexAndSampler((GX2Texture*) natras->texture, natras->sampler, stage);
			}
			else {
				setTexAndSampler(&whitetex, &whitesamp, stage);
			}
		}
	}
}

static void
clearCamera(Camera *cam, RGBA *col, uint32 mode)
{
	assert(cam->frameBuffer->type == Raster::CAMERA || cam->frameBuffer->type == Raster::CAMERATEXTURE);

	GX2Raster *natfb = PLUGINOFFSET(GX2Raster, cam->frameBuffer, nativeRasterOffset);
	GX2Raster *natzb = PLUGINOFFSET(GX2Raster, cam->zBuffer, nativeRasterOffset);

	GX2ColorBuffer* frameBuf = nullptr;
	GX2DepthBuffer* depthBuf = (GX2DepthBuffer*) natzb->texture;

	if (cam->frameBuffer->type == Raster::CAMERA) {
		frameBuf = (GX2ColorBuffer*) natfb->texture;
	}
	else {
		frameBuf = (GX2ColorBuffer*) natfb->texture2;
	}

	if(mode & Camera::CLEARIMAGE)
		GX2ClearColor(frameBuf, col->red / 255.0f, col->green / 255.0f, col->blue / 255.0f, col->alpha / 255.0f);

	uint32 clearFlags = 0;
	if(mode & Camera::CLEARZ)
		clearFlags |= GX2_CLEAR_FLAGS_DEPTH;
	if (mode & Camera::CLEARSTENCIL)
		clearFlags |= GX2_CLEAR_FLAGS_STENCIL;

	if (clearFlags != 0)
		GX2ClearDepthStencilEx(depthBuf, depthBuf->depthClear, depthBuf->stencilClear, (GX2ClearFlags) clearFlags);

	GX2SetContextState(globals.contextState);
}

static void
beginUpdate(Camera *cam)
{
	assert(cam->frameBuffer->type == Raster::CAMERA || cam->frameBuffer->type == Raster::CAMERATEXTURE);

	float view[16], proj[16];
	// View Matrix
	Matrix inv;
	Matrix::invert(&inv, cam->getFrame()->getLTM());
	// Since we're looking into positive Z,
	// flip X to ge a left handed view space.
	view[0]  = -inv.right.x;
	view[1]  =  inv.right.y;
	view[2]  =  inv.right.z;
	view[3]  =  0.0f;
	view[4]  = -inv.up.x;
	view[5]  =  inv.up.y;
	view[6]  =  inv.up.z;
	view[7]  =  0.0f;
	view[8]  =  -inv.at.x;
	view[9]  =   inv.at.y;
	view[10] =  inv.at.z;
	view[11] =  0.0f;
	view[12] = -inv.pos.x;
	view[13] =  inv.pos.y;
	view[14] =  inv.pos.z;
	view[15] =  1.0f;
	memcpy(&cam->devView, &view, sizeof(RawMatrix));
	setViewMatrix(view);

	// Projection Matrix
	float32 invwx = 1.0f/cam->viewWindow.x;
	float32 invwy = 1.0f/cam->viewWindow.y;
	float32 invz = 1.0f/(cam->farPlane-cam->nearPlane);

	proj[0] = invwx;
	proj[1] = 0.0f;
	proj[2] = 0.0f;
	proj[3] = 0.0f;

	proj[4] = 0.0f;
	proj[5] = invwy;
	proj[6] = 0.0f;
	proj[7] = 0.0f;

	proj[8] = cam->viewOffset.x*invwx;
	proj[9] = cam->viewOffset.y*invwy;
	proj[12] = -proj[8];
	proj[13] = -proj[9];
	if(cam->projection == Camera::PERSPECTIVE){
		proj[10] = (cam->farPlane+cam->nearPlane)*invz;
		proj[11] = 1.0f;

		proj[14] = -2.0f*cam->nearPlane*cam->farPlane*invz;
		proj[15] = 0.0f;
	}else{
		proj[10] = -(cam->farPlane+cam->nearPlane)*invz;
		proj[11] = 0.0f;

		proj[14] = 2.0f*invz;
		proj[15] = 1.0f;
	}
	memcpy(&cam->devProj, &proj, sizeof(RawMatrix));
	setProjectionMatrix(proj);

	if(rwStateCache.fogStart != cam->fogPlane){
		rwStateCache.fogStart = cam->fogPlane;
		uniformStateDirty[RWGX2_FOGSTART] = true;
		stateDirty = 1;
	}
	if(rwStateCache.fogEnd != cam->farPlane){
		rwStateCache.fogEnd = cam->farPlane;
		uniformStateDirty[RWGX2_FOGEND] = true;
		stateDirty = 1;
	}

	if (cam->frameBuffer->type == Raster::CAMERA) {
		uint32_t swapCount, flipCount;
		OSTime lastFlip, lastVsync;
		uint32_t waitCount = 0;
		while (1) {
			GX2GetSwapStatus(&swapCount, &flipCount, &lastFlip, &lastVsync);

			if (flipCount >= swapCount) {
				break;
			}

			if (waitCount >= 10) {
				WHBLogPrint("GPU Timed out. This should never happen.");
				break;
			}

			waitCount++;
			GX2WaitForVsync();
		}
	}

	GX2Raster *natfb = PLUGINOFFSET(GX2Raster, cam->frameBuffer, nativeRasterOffset);
	GX2Raster *natzb = PLUGINOFFSET(GX2Raster, cam->zBuffer, nativeRasterOffset);

	GX2ColorBuffer* frameBuf = nullptr;
	if (cam->frameBuffer->type == Raster::CAMERA) {
		frameBuf = (GX2ColorBuffer*) natfb->texture;
	}
	else {
		frameBuf = (GX2ColorBuffer*) natfb->texture2;
	}
	GX2DepthBuffer* depthBuf = (GX2DepthBuffer*) natzb->texture;

	GX2SetContextState(globals.contextState);

	GX2SetColorBuffer(frameBuf, GX2_RENDER_TARGET_0);
	GX2SetDepthBuffer(depthBuf);

	if (globals.width != frameBuf->surface.width || globals.height != frameBuf->surface.height) {
		GX2SetViewport(0.0f, 0.0f, frameBuf->surface.width, frameBuf->surface.height, 0.0f, 1.0f);
		GX2SetScissor(0, 0, frameBuf->surface.width, frameBuf->surface.height);
		globals.width = frameBuf->surface.width;
		globals.height = frameBuf->surface.height;
	}

	if (cam->frameBuffer->type == Raster::CAMERA) {
		GX2SetDRCScale(frameBuf->surface.width, frameBuf->surface.height);
	}
}

static void
endUpdate(Camera *cam)
{
	GX2DrawDone();

	freeVertexBuffers();
}

static void
showRaster(Raster *raster, uint32 flags)
{
	assert(raster->type == Raster::CAMERA);

	GX2Raster *natfb = PLUGINOFFSET(GX2Raster, raster, nativeRasterOffset);
	GX2ColorBuffer* frameBuf = (GX2ColorBuffer*) natfb->texture;

	GX2CopyColorBufferToScanBuffer(frameBuf, GX2_SCAN_TARGET_TV);
	GX2CopyColorBufferToScanBuffer(frameBuf, GX2_SCAN_TARGET_DRC);
	GX2SwapScanBuffers();
	GX2Flush();

	GX2SetTVEnable(TRUE);
	GX2SetDRCEnable(TRUE);
}

static bool32
rasterRenderFast(Raster *raster, int32 x, int32 y)
{
	Raster *src = raster;
	Raster *dst = Raster::getCurrentContext();
	GX2Raster *natdst = PLUGINOFFSET(GX2Raster, dst, nativeRasterOffset);
	GX2Raster *natsrc = PLUGINOFFSET(GX2Raster, src, nativeRasterOffset);

	switch(dst->type){
	case Raster::NORMAL:
	case Raster::TEXTURE:
	case Raster::CAMERATEXTURE:
		switch(src->type){
		case Raster::CAMERA:
			{
				GX2ColorBuffer* frameBuf = (GX2ColorBuffer*) natsrc->texture;
				GX2Texture* texture = (GX2Texture*) natdst->texture;

				GX2Rect rect = { x, y, x + src->width, y + src->height };
				GX2Point point = { x, y }; // { x, dst->height - src->height };
				GX2CopySurfaceEx(&frameBuf->surface, 0, 0, &texture->surface, texture->viewFirstMip, texture->viewFirstSlice, 1, &rect, &point);
				GX2DrawDone();

				gfxUtilsFlipSurface(&texture->surface, texture->viewFirstMip, texture->viewFirstSlice, &texture->surface, texture->viewFirstMip, texture->viewFirstSlice);
				GX2DrawDone();

				GX2SetContextState(globals.contextState);
			}
			return 1;
		}
		break;
	}
	return 0;
}

static void*
GfxGX2RAlloc(GX2RResourceFlags flags, uint32_t size, uint32_t alignment)
{
	// Color, depth, scan buffers all belong in MEM1
	if ((flags & (GX2R_RESOURCE_BIND_COLOR_BUFFER
				| GX2R_RESOURCE_BIND_DEPTH_BUFFER
				| GX2R_RESOURCE_BIND_SCAN_BUFFER
				| GX2R_RESOURCE_USAGE_FORCE_MEM1))
		&& !(flags & GX2R_RESOURCE_USAGE_FORCE_MEM2)) {
		return GfxHeapAllocMEM1(size, alignment);
	} else {
		return GfxHeapAllocMEM2(size, alignment);
	}
}

static void
GfxGX2RFree(GX2RResourceFlags flags, void *block)
{
	if ((flags & (GX2R_RESOURCE_BIND_COLOR_BUFFER
				| GX2R_RESOURCE_BIND_DEPTH_BUFFER
				| GX2R_RESOURCE_BIND_SCAN_BUFFER
				| GX2R_RESOURCE_USAGE_FORCE_MEM1))
		&& !(flags & GX2R_RESOURCE_USAGE_FORCE_MEM2)) {
		return GfxHeapFreeMEM1(block);
	} else {
		return GfxHeapFreeMEM2(block);
	}
}

void addToMEM1Buffer(void** buffer, uint32 size, uint32 alignment)
{
	MEM1Buffer buf;
	buf.buffer = buffer;
	buf.size = size;
	buf.alignment = alignment;
	gfxMEM1Buffers.push_back(buf);
}

void removeFromMEM1Buffer(void* buffer)
{
	auto it = gfxMEM1Buffers.begin();
	while (it != gfxMEM1Buffers.end()) {
		if ((*it).buffer && *(*it).buffer == buffer) {
			gfxMEM1Buffers.erase(it);
		}
		else {
			++it;
		}
	}
}

static uint32_t
GfxProcCallbackAcquired(void *context)
{
	if (!GfxHeapInitMEM1()) {
		WHBLogPrintf("GfxHeapInitMEM1 failed");
		return -1;
	}

	if (!GfxHeapInitForeground()) {
		WHBLogPrintf("GfxHeapInitForeground failed");
		return -1;
	}

	gfxInForeground = true;

	// Allocate TV scan buffer.
	globals.tvScanBuffer = GfxHeapAllocForeground(globals.tvScanBufferSize, GX2_SCAN_BUFFER_ALIGNMENT);
	if (!globals.tvScanBuffer)
	{
		WHBLogPrintf("Cannot allocate TV scan buffer");
		return -1;
	}
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU, globals.tvScanBuffer, globals.tvScanBufferSize);
	GX2SetTVBuffer(globals.tvScanBuffer, globals.tvScanBufferSize, globals.tvRenderMode, globals.tvSurfaceFormat, GX2_BUFFERING_MODE_DOUBLE);

	// Allocate DRC scan buffer.
	globals.drcScanBuffer = GfxHeapAllocForeground(globals.drcScanBufferSize, GX2_SCAN_BUFFER_ALIGNMENT);
	if (!globals.drcScanBuffer)
	{
		WHBLogPrintf("Cannot allocate DRC scan buffer");
		return -1;
	}
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU, globals.drcScanBuffer, globals.drcScanBufferSize);
	GX2SetDRCBuffer(globals.drcScanBuffer, globals.drcScanBufferSize, globals.drcRenderMode, globals.drcSurfaceFormat, GX2_BUFFERING_MODE_DOUBLE);

	// reallocate all buffers
	for (const MEM1Buffer& buf : gfxMEM1Buffers) {
		if (buf.buffer) {
			*buf.buffer = GfxHeapAllocMEM1(buf.size, buf.alignment);
		}
	}

	return 0;
}

static uint32_t
GfxProcCallbackReleased(void *context)
{
	GX2DrawDone();

	gfxInForeground = false;

	if (globals.tvScanBuffer)
	{
		GfxHeapFreeForeground(globals.tvScanBuffer);
		globals.tvScanBuffer = NULL;
	}

	if (globals.drcScanBuffer)
	{
		GfxHeapFreeForeground(globals.drcScanBuffer);
		globals.drcScanBuffer = NULL;
	}

	// free all buffers in MEM1
	for (const MEM1Buffer& buf : gfxMEM1Buffers) {
		if (buf.buffer) {
			GfxHeapFreeMEM1(*buf.buffer);
			*buf.buffer = nil;
		}
	}

	GfxHeapDestroyMEM1();
	GfxHeapDestroyForeground();
	return 0;
}

static int
openGX2(void)
{
	// TODO make use of tvwidth and height
	uint32_t tvWidth, tvHeight;
	uint32_t unk;

	globals.commandBufferPool = GfxHeapAllocMEM2(0x400000, GX2_COMMAND_BUFFER_ALIGNMENT);

	if (!globals.commandBufferPool)
	{
		WHBLogPrintf("%s: failed to allocate command buffer pool", __FUNCTION__);
		return FALSE;
	}

	uint32_t initAttribs[] = {
		GX2_INIT_CMD_BUF_BASE, (uintptr_t)globals.commandBufferPool,
		GX2_INIT_CMD_BUF_POOL_SIZE, 0x400000,
		GX2_INIT_ARGC, 0,
		GX2_INIT_ARGV, 0,
		GX2_INIT_END
	};
	GX2Init(initAttribs);

	globals.drcRenderMode = GX2GetSystemDRCScanMode();
	globals.tvSurfaceFormat = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
	globals.drcSurfaceFormat = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;

	switch(GX2GetSystemTVScanMode())
	{
	case GX2_TV_SCAN_MODE_480I:
	case GX2_TV_SCAN_MODE_480P:
		globals.tvRenderMode = GX2_TV_RENDER_MODE_WIDE_480P;
		tvWidth = 854;
		tvHeight = 480;
		break;
	case GX2_TV_SCAN_MODE_1080I:
	case GX2_TV_SCAN_MODE_1080P:
		globals.tvRenderMode = GX2_TV_RENDER_MODE_WIDE_1080P;
		tvWidth = 1920;
		tvHeight = 1080;
		break;
	case GX2_TV_SCAN_MODE_720P:
	default:
		globals.tvRenderMode = GX2_TV_RENDER_MODE_WIDE_720P;
		tvWidth = 1280;
		tvHeight = 720;
		break;
	}

	// Setup TV and DRC buffers - they will be allocated in GfxProcCallbackAcquired.
	GX2CalcTVSize(globals.tvRenderMode, globals.tvSurfaceFormat, GX2_BUFFERING_MODE_DOUBLE, &globals.tvScanBufferSize, &unk);
	GX2CalcDRCSize(globals.drcRenderMode, globals.drcSurfaceFormat, GX2_BUFFERING_MODE_DOUBLE, &globals.drcScanBufferSize, &unk);

	GX2RSetAllocator(&GfxGX2RAlloc, &GfxGX2RFree);
	ProcUIRegisterCallback(PROCUI_CALLBACK_ACQUIRE, GfxProcCallbackAcquired, NULL, 100);
	ProcUIRegisterCallback(PROCUI_CALLBACK_RELEASE, GfxProcCallbackReleased, NULL, 100);

	GfxProcCallbackAcquired(nil);

	// Initialise TV context state.
	globals.contextState = (GX2ContextState*) GfxHeapAllocMEM2(sizeof(GX2ContextState), GX2_CONTEXT_STATE_ALIGNMENT);
	if (!globals.contextState)
	{
		WHBLogPrintf("%s: failed to allocate sTvContextState", __FUNCTION__);
		goto error;
	}

	GX2SetupContextStateEx(globals.contextState, TRUE);
	GX2SetContextState(globals.contextState);

	// Wii U complains about no vsync
	GX2SetSwapInterval(1);

	return TRUE;

error:
	if (globals.commandBufferPool)
	{
		GfxHeapFreeMEM2(globals.commandBufferPool);
		globals.commandBufferPool = NULL;
	}

	if (globals.contextState)
	{
		GfxHeapFreeMEM2(globals.contextState);
		globals.contextState = NULL;
	}
	return FALSE;
}

static int
closeGX2(void)
{
	if (gfxInForeground) {
		GfxProcCallbackReleased(nil);
	}

	GX2RSetAllocator(NULL, NULL);
	GX2Shutdown();

	if (globals.contextState)
	{
		GfxHeapFreeMEM2(globals.contextState);
		globals.contextState = NULL;
	}

	if (globals.commandBufferPool)
	{
		GfxHeapFreeMEM2(globals.commandBufferPool);
		globals.commandBufferPool = NULL;
	}
	return 1;
}

static void
createWhiteTexture(void)
{
	whitetex.surface.width = 1;
	whitetex.surface.height = 1;
	whitetex.surface.depth = 1;
	whitetex.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
	whitetex.surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
	whitetex.surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
	whitetex.viewNumSlices = 1;
	whitetex.compMap = GX2_COMP_MAP(GX2_SQ_SEL_R, GX2_SQ_SEL_G, GX2_SQ_SEL_B, GX2_SQ_SEL_A);
	GX2CalcSurfaceSizeAndAlignment(&whitetex.surface);
	GX2InitTextureRegs(&whitetex);

	whitetex.surface.image = memalign(whitetex.surface.alignment, whitetex.surface.imageSize);
	memset(whitetex.surface.image, 0xFF, whitetex.surface.imageSize);
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, whitetex.surface.image, whitetex.surface.imageSize);

	GX2InitSampler(&whitesamp, GX2_TEX_CLAMP_MODE_WRAP, GX2_TEX_XY_FILTER_MODE_POINT);
}

static int
initGX2()
{
	lastShaderUploaded = nil;

	u_fogData = registerUniform("u_fogData", UNIFORM_VEC4);
	u_fogColor = registerUniform("u_fogColor", UNIFORM_VEC4);
	u_proj = registerUniform("u_proj", UNIFORM_MAT4);
	u_view = registerUniform("u_view", UNIFORM_MAT4);
	u_world = registerUniform("u_world", UNIFORM_MAT4);
	u_ambLight = registerUniform("u_ambLight", UNIFORM_VEC4);
	u_lightParams = registerUniform("u_lightParams", UNIFORM_VEC4, MAX_LIGHTS);
	u_lightPosition = registerUniform("u_lightPosition", UNIFORM_VEC4, MAX_LIGHTS);
	u_lightDirection = registerUniform("u_lightDirection", UNIFORM_VEC4, MAX_LIGHTS);
	u_lightColor = registerUniform("u_lightColor", UNIFORM_VEC4, MAX_LIGHTS);

	u_matColor = registerUniform("u_matColor", UNIFORM_VEC4);
	u_surfProps = registerUniform("u_surfProps", UNIFORM_VEC4);

	// for im2d
	registerUniform("u_xform", UNIFORM_VEC4);

	createWhiteTexture();

	resetRenderState();

	defaultShader = Shader::create(default_gsh, GX2_SHADER_MODE_UNIFORM_REGISTER);
	defaultShader->initAttribute("in_pos", 0, GX2_ATTRIB_FORMAT_FLOAT_32_32_32);
	defaultShader->initAttribute("in_normal", 12, GX2_ATTRIB_FORMAT_FLOAT_32_32_32);
	defaultShader->initAttribute("in_color", 24, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32);
	defaultShader->initAttribute("in_tex0", 40, GX2_ATTRIB_FORMAT_FLOAT_32_32);

	defaultShader->init();
	defaultShader->use();

	defaultShader->samplerLocation = GX2GetPixelSamplerVarLocation(defaultShader->group.pixelShader, "tex0");

	openIm2D();
	openIm3D();

	setTexAndSampler(&whitetex, &whitesamp, 0);

	gfxUtilsInit();
	GX2SetContextState(globals.contextState);

	return 1;
}

static int
termGX2()
{
	gfxUtilsShutdown();

	defaultShader->destroy();

	closeIm3D();
	closeIm2D();

	free(whitetex.surface.image);

	return 1;
}

static int
deviceSystemGX2(DeviceReq req, void *arg, int32 n)
{
	VideoMode *rwmode;

	switch(req){
	case DEVICEOPEN:
		return openGX2();
	case DEVICECLOSE:
		return closeGX2();

	case DEVICEINIT:
		return initGX2();
	case DEVICETERM:
		return termGX2();

	case DEVICEFINALIZE:
		return 1;

	case DEVICEGETNUMSUBSYSTEMS:
		return 1;

	case DEVICEGETCURRENTSUBSYSTEM:
		return 0;

	case DEVICESETSUBSYSTEM:
		if (n >= 1)
			return 0;
		return 1;

	case DEVICEGETSUBSSYSTEMINFO:
		if (n >= 1)
			return 0;
		strncpy(((SubSystemInfo*)arg)->name, "Wii U", sizeof(SubSystemInfo::name));
		return 1;

	case DEVICEGETNUMVIDEOMODES:
		return 1;

	case DEVICEGETCURRENTVIDEOMODE:
		return 0;

	case DEVICESETVIDEOMODE:
		if (n >= 1)
			return 0;
		return 1;

	case DEVICEGETVIDEOMODEINFO:
		if (n >= 1)
			return 0;
		// TODO: maybe use the actual tv size here?
		rwmode = (VideoMode*)arg;
		rwmode->width = 1280;
		rwmode->height = 720;
		rwmode->depth = 32;
		rwmode->flags = VIDEOMODEEXCLUSIVE;
		return 1;
	case DEVICEGETMAXMULTISAMPLINGLEVELS:
		return 1;
	case DEVICEGETMULTISAMPLINGLEVELS:
		return 1;
	case DEVICESETMULTISAMPLINGLEVELS:
		return 1;
	default:
		WHBLogPrintf("not implemented");
		assert(0 && "not implemented");
		return 0;
	}
	return 1;
}

Device renderdevice = {
	-1.0f, 1.0f,
	gx2::beginUpdate,
	gx2::endUpdate,
	gx2::clearCamera,
	gx2::showRaster,
	gx2::rasterRenderFast,
	gx2::setRenderState,
	gx2::getRenderState,

	gx2::im2DRenderLine,
	gx2::im2DRenderTriangle,
	gx2::im2DRenderPrimitive,
	gx2::im2DRenderIndexedPrimitive,

	gx2::im3DTransform,
	gx2::im3DRenderPrimitive,
	gx2::im3DRenderIndexedPrimitive,
	gx2::im3DEnd,

	gx2::deviceSystemGX2
};

}
}

#endif

