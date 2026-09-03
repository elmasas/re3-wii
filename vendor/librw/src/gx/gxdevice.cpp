#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <malloc.h>

#include "../rwbase.h"
#include "../rwerror.h"
#include "../rwplg.h"
#include "../rwrender.h"
#include "../rwengine.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"

#ifdef __WII__

#include <ogc/gu.h>

#include "rwgx.h"

namespace rw {
namespace gx {

#define FIFO_SIZE (256*1024)
#define MAX_LIGHTS 8
#define MAXNUMSTAGES 8

static struct {
	GXRModeObj *rmode;
	void *frameBuffer[2];
	int32 activeBuffer;
	void *fifo;
	uint32 width, height;
} globals;

struct RwRasterStateCache
{
	Raster *raster;
	Texture::Addressing addressingU;
	Texture::Addressing addressingV;
	Texture::FilterMode filter;
};

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
	uint32 fogEnable;
	float32 fogStart;
	float32 fogEnd;
	uint32 fogColor;

	// emulation of PS2 GS
	bool32 gsalpha;
	uint32 gsalpharef;

	RwRasterStateCache texstage[MAXNUMSTAGES];
};
static RwStateCache rwStateCache;

enum
{
	RWGX_BLEND,
	RWGX_SRCBLEND,
	RWGX_DESTBLEND,
	RWGX_DEPTHTEST,
	RWGX_DEPTHFUNC,
	RWGX_DEPTHMASK,
	RWGX_CULLMODE,
	RWGX_ALPHATEST,
	RWGX_ALPHAFUNC,
	RWGX_ALPHAREF,
	RWGX_FOG,

	RWGX_NUM_STATES
};

struct GXState {
	bool32 blendEnable;
	uint8 srcblend, destblend;

	bool32 depthTestEnable;
	uint8 depthFunc;
	uint32 depthMask;

	uint8 cullMode;

	bool32 alphaTestEnable;
	uint8 alphaFunc;
	uint32 alphaRef;

	uint8 fogMode;
	float32 fogStart, fogEnd, fogNear, fogFar;
	uint32 fogColor;

	int32 viewportX, viewportY, viewportW, viewportH;
};
static GXState curGXState, oldGXState;
static bool32 stateDirty = 1;
static bool32 viewportDirty = 1;

static int32 tevTextured = ~0;
static GXColor lightAmbient = { 0, 0, 0, 255 };
static bool32 lightingOn;

static GXLightObj loadedLights[MAX_LIGHTS];
static int32 loadedLightCount = -1;
static uint32 loadedAmbient = ~0u;
int32 activeLightCount;

static uint8 blendMap[] = {
	GX_BL_ZERO,		// actually invalid
	GX_BL_ZERO,
	GX_BL_ONE,
	GX_BL_SRCCLR,
	GX_BL_INVSRCCLR,
	GX_BL_SRCALPHA,
	GX_BL_INVSRCALPHA,
	GX_BL_DSTALPHA,
	GX_BL_INVDSTALPHA,
	GX_BL_DSTCLR,
	GX_BL_INVDSTCLR,
	GX_BL_SRCALPHA		// SRCALPHASAT
};

static uint8 alphafuncMap[] = {
	GX_ALWAYS,
	GX_GEQUAL,
	GX_LESS
};

static void
setGXRenderState(int32 state, uint32 value)
{
	switch(state){
	case RWGX_BLEND:	curGXState.blendEnable = value; break;
	case RWGX_SRCBLEND:	curGXState.srcblend = (uint8)value; break;
	case RWGX_DESTBLEND:	curGXState.destblend = (uint8)value; break;
	case RWGX_DEPTHTEST:	curGXState.depthTestEnable = value; break;
	case RWGX_DEPTHFUNC:	curGXState.depthFunc = (uint8)value; break;
	case RWGX_DEPTHMASK:	curGXState.depthMask = value; break;
	case RWGX_CULLMODE:	curGXState.cullMode = (uint8)value; break;
	case RWGX_ALPHATEST:	curGXState.alphaTestEnable = value; break;
	case RWGX_ALPHAFUNC:	curGXState.alphaFunc = (uint8)value; break;
	case RWGX_ALPHAREF:	curGXState.alphaRef = value; break;
	}
	stateDirty = 1;
}

static void
flushGXState(void)
{
	if(!stateDirty && !viewportDirty)
		return;

	if(viewportDirty){
		GX_SetViewport((f32)curGXState.viewportX, (f32)curGXState.viewportY,
			(f32)curGXState.viewportW, (f32)curGXState.viewportH, 0.0f, 1.0f);
		GX_SetScissor(curGXState.viewportX, curGXState.viewportY,
			curGXState.viewportW, curGXState.viewportH);
		oldGXState.viewportX = curGXState.viewportX;
		oldGXState.viewportY = curGXState.viewportY;
		oldGXState.viewportW = curGXState.viewportW;
		oldGXState.viewportH = curGXState.viewportH;
		viewportDirty = 0;
	}

	if(oldGXState.blendEnable != curGXState.blendEnable ||
	   oldGXState.srcblend != curGXState.srcblend ||
	   oldGXState.destblend != curGXState.destblend){
		oldGXState.blendEnable = curGXState.blendEnable;
		oldGXState.srcblend = curGXState.srcblend;
		oldGXState.destblend = curGXState.destblend;
		GX_SetBlendMode(oldGXState.blendEnable ? GX_BM_BLEND : GX_BM_NONE,
			oldGXState.srcblend, oldGXState.destblend, GX_LO_CLEAR);
	}

	if(oldGXState.depthTestEnable != curGXState.depthTestEnable ||
	   oldGXState.depthFunc != curGXState.depthFunc ||
	   oldGXState.depthMask != curGXState.depthMask){
		oldGXState.depthTestEnable = curGXState.depthTestEnable;
		oldGXState.depthFunc = curGXState.depthFunc;
		oldGXState.depthMask = curGXState.depthMask;
		GX_SetZMode(oldGXState.depthTestEnable ? GX_TRUE : GX_FALSE,
			oldGXState.depthFunc,
			oldGXState.depthMask ? GX_TRUE : GX_FALSE);
	}

	if(oldGXState.cullMode != curGXState.cullMode){
		oldGXState.cullMode = curGXState.cullMode;
		GX_SetCullMode(oldGXState.cullMode);
	}

	if(oldGXState.alphaTestEnable != curGXState.alphaTestEnable ||
	   oldGXState.alphaFunc != curGXState.alphaFunc ||
	   oldGXState.alphaRef != curGXState.alphaRef){
		oldGXState.alphaTestEnable = curGXState.alphaTestEnable;
		oldGXState.alphaFunc = curGXState.alphaFunc;
		oldGXState.alphaRef = curGXState.alphaRef;
		uint8 fn = oldGXState.alphaTestEnable ? oldGXState.alphaFunc : GX_ALWAYS;
		// early Z would let a discarded pixel still write depth
		GX_SetZCompLoc(fn == GX_ALWAYS ? GX_ENABLE : GX_DISABLE);
		GX_SetAlphaCompare(fn, oldGXState.alphaRef, GX_AOP_AND, GX_ALWAYS, 0);
	}

	// the near and far planes rebuild the eye z, stale ones fog the whole scene
	if(oldGXState.fogMode != curGXState.fogMode ||
	   oldGXState.fogStart != curGXState.fogStart ||
	   oldGXState.fogEnd != curGXState.fogEnd ||
	   oldGXState.fogNear != curGXState.fogNear ||
	   oldGXState.fogFar != curGXState.fogFar ||
	   oldGXState.fogColor != curGXState.fogColor){
		oldGXState.fogMode = curGXState.fogMode;
		oldGXState.fogStart = curGXState.fogStart;
		oldGXState.fogEnd = curGXState.fogEnd;
		oldGXState.fogNear = curGXState.fogNear;
		oldGXState.fogFar = curGXState.fogFar;
		oldGXState.fogColor = curGXState.fogColor;
		GXColor c = {
			(uint8)(oldGXState.fogColor),
			(uint8)(oldGXState.fogColor >> 8),
			(uint8)(oldGXState.fogColor >> 16),
			(uint8)(oldGXState.fogColor >> 24)
		};
		uint8 mode = oldGXState.fogMode;
		if(oldGXState.fogStart >= oldGXState.fogEnd ||
		   oldGXState.fogNear >= oldGXState.fogFar)
			mode = GX_FOG_NONE;
		GX_SetFog(mode, oldGXState.fogStart, oldGXState.fogEnd,
			oldGXState.fogNear, oldGXState.fogFar, c);
	}

	stateDirty = 0;
}

void
setAlphaBlend(bool32 enable)
{
	if(rwStateCache.blendEnable != enable){
		rwStateCache.blendEnable = enable;
		setGXRenderState(RWGX_BLEND, enable);
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
			// GX drops the write when the compare is off, so compare always
			setGXRenderState(RWGX_DEPTHTEST, true);
			setGXRenderState(RWGX_DEPTHFUNC, GX_ALWAYS);
		}else{
			setGXRenderState(RWGX_DEPTHTEST, rwStateCache.ztest);
			setGXRenderState(RWGX_DEPTHFUNC, GX_LEQUAL);
		}
	}
}

static void
setDepthWrite(bool32 enable)
{
	enable = enable ? 1 : 0;
	if(rwStateCache.zwrite != (uint32)enable){
		rwStateCache.zwrite = enable;
		if(enable && !rwStateCache.ztest){
			setGXRenderState(RWGX_DEPTHTEST, true);
			setGXRenderState(RWGX_DEPTHFUNC, GX_ALWAYS);
		}
		setGXRenderState(RWGX_DEPTHMASK, rwStateCache.zwrite);
	}
}

static void
setAlphaTest(bool32 enable)
{
	if(rwStateCache.alphaTestEnable != (uint32)enable){
		rwStateCache.alphaTestEnable = enable;
		setGXRenderState(RWGX_ALPHATEST, enable);
	}
}

static void
setAlphaTestFunction(uint32 function)
{
	if(rwStateCache.alphaFunc != function){
		rwStateCache.alphaFunc = function;
		setGXRenderState(RWGX_ALPHAFUNC, alphafuncMap[function]);
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

// ------------------------------------------------------------ raster stage

static void
setFilterMode(int32 stage, int32 filter)
{
	if(rwStateCache.texstage[stage].filter != (Texture::FilterMode)filter){
		rwStateCache.texstage[stage].filter = (Texture::FilterMode)filter;
		Raster *raster = rwStateCache.texstage[stage].raster;
		if(raster){
			GXRaster *natras = PLUGINOFFSET(GXRaster, raster, nativeRasterOffset);
			if(natras->filterMode != filter){
				natras->filterMode = filter;
				updateSampler(natras);
				if(natras->ready)
					GX_LoadTexObj(&natras->tex, GX_TEXMAP0 + stage);
			}
		}
	}
}

static void
setAddressU(int32 stage, int32 addressing)
{
	if(rwStateCache.texstage[stage].addressingU != (Texture::Addressing)addressing){
		rwStateCache.texstage[stage].addressingU = (Texture::Addressing)addressing;
		Raster *raster = rwStateCache.texstage[stage].raster;
		if(raster){
			GXRaster *natras = PLUGINOFFSET(GXRaster, raster, nativeRasterOffset);
			if(natras->addressU != addressing){
				natras->addressU = addressing;
				updateSampler(natras);
				if(natras->ready)
					GX_LoadTexObj(&natras->tex, GX_TEXMAP0 + stage);
			}
		}
	}
}

static void
setAddressV(int32 stage, int32 addressing)
{
	if(rwStateCache.texstage[stage].addressingV != (Texture::Addressing)addressing){
		rwStateCache.texstage[stage].addressingV = (Texture::Addressing)addressing;
		Raster *raster = rwStateCache.texstage[stage].raster;
		if(raster){
			GXRaster *natras = PLUGINOFFSET(GXRaster, raster, nativeRasterOffset);
			if(natras->addressV != addressing){
				natras->addressV = addressing;
				updateSampler(natras);
				if(natras->ready)
					GX_LoadTexObj(&natras->tex, GX_TEXMAP0 + stage);
			}
		}
	}
}

void
setRasterStage(int32 stage, Raster *raster)
{
	bool32 alpha = 0;
	if(raster != rwStateCache.texstage[stage].raster){
		rwStateCache.texstage[stage].raster = raster;
		if(raster){
			GXRaster *natras = PLUGINOFFSET(GXRaster, raster, nativeRasterOffset);
			if(natras->ready)
				GX_LoadTexObj(&natras->tex, GX_TEXMAP0 + stage);
			alpha = natras->hasAlpha;
			rwStateCache.texstage[stage].filter = (Texture::FilterMode)natras->filterMode;
			rwStateCache.texstage[stage].addressingU = (Texture::Addressing)natras->addressU;
			rwStateCache.texstage[stage].addressingV = (Texture::Addressing)natras->addressV;
		}
		if(stage == 0 && alpha != rwStateCache.textureAlpha){
			rwStateCache.textureAlpha = alpha;
			if(!rwStateCache.vertexAlpha){
				setAlphaBlend(alpha);
				setAlphaTest(alpha);
			}
		}
	}
}

Raster*
getRasterStage(int32 stage)
{
	return rwStateCache.texstage[stage].raster;
}

void
evictRaster(Raster *raster)
{
	for(int32 i = 0; i < MAXNUMSTAGES; i++)
		if(rwStateCache.texstage[i].raster == raster)
			rwStateCache.texstage[i].raster = nil;
}

void
setTexture(int32 stage, Texture *tex)
{
	if(tex == nil || tex->raster == nil){
		setRasterStage(stage, nil);
		return;
	}
	setRasterStage(stage, tex->raster);
	setFilterMode(stage, tex->getFilter());
	setAddressU(stage, tex->getAddressU());
	setAddressV(stage, tex->getAddressV());
}

// ------------------------------------------------------------ matrices

static Matrix viewMatrix;
static Mtx44 projMatrix;
static uint8 projType = GX_PERSPECTIVE;
bool32 sceneDirty = 1;

// RW looks down +Z where GX looks down -Z, and GX clip space is mirrored on X
static void
makeGXMatrix(Mtx dst, const Matrix *m)
{
	dst[0][0] = -m->right.x; dst[0][1] = -m->up.x; dst[0][2] = -m->at.x; dst[0][3] = -m->pos.x;
	dst[1][0] =  m->right.y; dst[1][1] =  m->up.y; dst[1][2] =  m->at.y; dst[1][3] =  m->pos.y;
	dst[2][0] = -m->right.z; dst[2][1] = -m->up.z; dst[2][2] = -m->at.z; dst[2][3] = -m->pos.z;
}

static void
toViewSpace(V3d *dst, const V3d *src, bool32 isPoint)
{
	V3d t;
	t.x = src->x*viewMatrix.right.x + src->y*viewMatrix.up.x + src->z*viewMatrix.at.x;
	t.y = src->x*viewMatrix.right.y + src->y*viewMatrix.up.y + src->z*viewMatrix.at.y;
	t.z = src->x*viewMatrix.right.z + src->y*viewMatrix.up.z + src->z*viewMatrix.at.z;
	if(isPoint){
		t.x += viewMatrix.pos.x;
		t.y += viewMatrix.pos.y;
		t.z += viewMatrix.pos.z;
	}
	dst->x = -t.x;
	dst->y =  t.y;
	dst->z = -t.z;
}

void
setProjectionMatrix(Camera *cam)
{
	float32 n = cam->nearPlane;
	float32 f = cam->farPlane;
	float32 ox = cam->viewOffset.x;
	float32 oy = cam->viewOffset.y;

	if(cam->projection == Camera::PERSPECTIVE){
		guFrustum(projMatrix,
			( cam->viewWindow.y - oy)*n, (-cam->viewWindow.y - oy)*n,
			(-cam->viewWindow.x - ox)*n, ( cam->viewWindow.x - ox)*n, n, f);
		projType = GX_PERSPECTIVE;
	}else{
		guOrtho(projMatrix,
			 cam->viewWindow.y - oy, -cam->viewWindow.y - oy,
			-cam->viewWindow.x - ox,  cam->viewWindow.x - ox, n, f);
		projType = GX_ORTHOGRAPHIC;
	}
	sceneDirty = 1;
}

void
setViewMatrix(Camera *cam)
{
	Matrix::invert(&viewMatrix, cam->getFrame()->getLTM());
	sceneDirty = 1;
}

void
setWorldMatrix(Matrix *world)
{
	if(sceneDirty){
		GX_LoadProjectionMtx(projMatrix, projType);
		sceneDirty = 0;
	}

	Matrix modelView;
	Matrix::mult(&modelView, world, &viewMatrix);

	Mtx m;
	makeGXMatrix(m, &modelView);
	GX_LoadPosMtxImm(m, GX_PNMTX0);

	// the normal matrix only feeds the lighting unit
	if(activeLightCount){
		// an orthonormal matrix is its own inverse transpose
		if((world->flags & Matrix::TYPEMASK) == Matrix::TYPEORTHONORMAL)
			GX_LoadNrmMtxImm(m, GX_PNMTX0);
		else{
			Mtx nrm;
			if(guMtxInvXpose(m, nrm))
				GX_LoadNrmMtxImm(nrm, GX_PNMTX0);
			else
				GX_LoadNrmMtxImm(m, GX_PNMTX0);
		}
	}
	GX_SetCurrentMtx(GX_PNMTX0);
}

// ------------------------------------------------------------ lights

static const uint8 lightIds[MAX_LIGHTS] = {
	GX_LIGHT0, GX_LIGHT1, GX_LIGHT2, GX_LIGHT3,
	GX_LIGHT4, GX_LIGHT5, GX_LIGHT6, GX_LIGHT7
};

static uint8
toByte(float32 v)
{
	int32 i = (int32)(v*255.0f);
	return (uint8)(i < 0 ? 0 : i > 255 ? 255 : i);
}

int32
setLights(WorldLights *lightData)
{
	GXColor amb = {
		toByte(lightData->ambient.red),
		toByte(lightData->ambient.green),
		toByte(lightData->ambient.blue),
		255
	};
	uint32 ambKey = (amb.r<<16) | (amb.g<<8) | amb.b;
	if(ambKey != loadedAmbient){
		loadedAmbient = ambKey;
		lightAmbient = amb;
		tevTextured = ~0;
	}

	GXLightObj objs[MAX_LIGHTS];
	uint32 mask = 0;
	int32 n = 0;

	for(int32 i = 0; i < lightData->numDirectionals && n < MAX_LIGHTS; i++){
		Light *l = lightData->directionals[i];
		V3d dir;
		toViewSpace(&dir, &l->getFrame()->getLTM()->at, 0);

		GXColor c = { toByte(l->color.red), toByte(l->color.green),
			toByte(l->color.blue), 255 };
		// every field reaches the hardware, so nothing may stay uninitialised
		memset(&objs[n], 0, sizeof(GXLightObj));
		// a directional light is a point light pushed far along -dir
		GX_InitLightPos(&objs[n], -dir.x*1048576.0f, -dir.y*1048576.0f, -dir.z*1048576.0f);
		GX_InitLightDir(&objs[n], dir.x, dir.y, dir.z);
		GX_InitLightColor(&objs[n], c);
		GX_InitLightSpot(&objs[n], 0.0f, GX_SP_OFF);
		GX_InitLightDistAttn(&objs[n], 1.0f, 1.0f, GX_DA_OFF);
		mask |= lightIds[n];
		n++;
	}

	for(int32 i = 0; i < lightData->numLocals && n < MAX_LIGHTS; i++){
		Light *l = lightData->locals[i];
		V3d pos;
		toViewSpace(&pos, &l->getFrame()->getLTM()->pos, 1);

		GXColor c = { toByte(l->color.red), toByte(l->color.green),
			toByte(l->color.blue), 255 };
		float32 r = l->radius > 0.0f ? l->radius : 1.0f;
		memset(&objs[n], 0, sizeof(GXLightObj));
		GX_InitLightPos(&objs[n], pos.x, pos.y, pos.z);
		GX_InitLightDir(&objs[n], 0.0f, 0.0f, -1.0f);
		GX_InitLightColor(&objs[n], c);
		GX_InitLightSpot(&objs[n], 0.0f, GX_SP_OFF);
		GX_InitLightDistAttn(&objs[n], r, 0.5f, GX_DA_MEDIUM);
		mask |= lightIds[n];
		n++;
	}

	for(int32 i = 0; i < n; i++)
		if(i >= loadedLightCount ||
		   memcmp(&objs[i], &loadedLights[i], sizeof(GXLightObj)) != 0){
			loadedLights[i] = objs[i];
			GX_LoadLightObj(&objs[i], lightIds[i]);
		}

	// RW adds prelight and lights, so the vertex colour is the ambient input
	if(n != loadedLightCount || !lightingOn){
		loadedLightCount = n;
		GXColor white = { 255, 255, 255, 255 };
		GX_SetNumChans(1);
		GX_SetChanMatColor(GX_COLOR0A0, white);
		GX_SetChanCtrl(GX_COLOR0A0, GX_ENABLE, GX_SRC_VTX, GX_SRC_REG,
			mask, GX_DF_CLAMP, GX_AF_SPOT);
		lightingOn = 1;
		tevTextured = ~0;
	}
	activeLightCount = n;
	return n;
}

void
setLightingEnabled(bool32 enable)
{
	if(lightingOn == enable)
		return;
	lightingOn = enable;
	if(!enable){
		GX_SetNumChans(1);
		GX_SetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_REG, GX_SRC_VTX,
			0, GX_DF_NONE, GX_AF_NONE);
		loadedLightCount = -1;
		activeLightCount = 0;
	}
	tevTextured = ~0;
}

void
invalidateLightCache(void)
{
	loadedLightCount = -1;
	loadedAmbient = ~0u;
}

// ------------------------------------------------------------ material / tev

static RGBA matColor = { 255, 255, 255, 255 };
static bool32 matDirty = 1;

void
setMaterial(const RGBA &color, const SurfaceProperties &surfaceprops)
{
	if(matColor.red != color.red || matColor.green != color.green ||
	   matColor.blue != color.blue || matColor.alpha != color.alpha){
		matColor = color;
		matDirty = 1;
	}
}

static void
setupTev(bool32 textured)
{
	bool32 lit = lightingOn;
	int32 key = (textured ? 1 : 0) | (lit ? 2 : 0);
	if(tevTextured != key){
		tevTextured = key;
		int32 stage = GX_TEVSTAGE0;

		// the ambient is added, not modulated, so it gets its own stage
		if(lit){
			GX_SetTevOrder(stage, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
			GX_SetTevKColorSel(stage, GX_TEV_KCSEL_K1);
			GX_SetTevColorIn(stage, GX_CC_KONST, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
			GX_SetTevAlphaIn(stage, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_RASA);
			GX_SetTevColorOp(stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
			GX_SetTevAlphaOp(stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
			stage++;
		}

		if(textured){
			GX_SetNumTexGens(1);
			GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
			GX_SetTevOrder(stage, GX_TEXCOORD0, GX_TEXMAP0,
				lit ? GX_COLORNULL : GX_COLOR0A0);
			if(lit){
				GX_SetTevColorIn(stage, GX_CC_ZERO, GX_CC_TEXC, GX_CC_CPREV, GX_CC_ZERO);
				GX_SetTevAlphaIn(stage, GX_CA_ZERO, GX_CA_TEXA, GX_CA_APREV, GX_CA_ZERO);
			}else{
				GX_SetTevColorIn(stage, GX_CC_ZERO, GX_CC_TEXC, GX_CC_RASC, GX_CC_ZERO);
				GX_SetTevAlphaIn(stage, GX_CA_ZERO, GX_CA_TEXA, GX_CA_RASA, GX_CA_ZERO);
			}
		}else{
			GX_SetNumTexGens(0);
			GX_SetTevOrder(stage, GX_TEXCOORDNULL, GX_TEXMAP_NULL,
				lit ? GX_COLORNULL : GX_COLOR0A0);
			if(lit){
				GX_SetTevColorIn(stage, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_CPREV);
				GX_SetTevAlphaIn(stage, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_APREV);
			}else{
				GX_SetTevColorIn(stage, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
				GX_SetTevAlphaIn(stage, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_RASA);
			}
		}
		GX_SetTevColorOp(stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
		GX_SetTevAlphaOp(stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
		stage++;

		// the material colour modulates last
		GX_SetTevOrder(stage, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLORNULL);
		GX_SetTevKColorSel(stage, GX_TEV_KCSEL_K0);
		GX_SetTevKAlphaSel(stage, GX_TEV_KASEL_K0_A);
		GX_SetTevColorIn(stage, GX_CC_ZERO, GX_CC_CPREV, GX_CC_KONST, GX_CC_ZERO);
		GX_SetTevAlphaIn(stage, GX_CA_ZERO, GX_CA_APREV, GX_CA_KONST, GX_CA_ZERO);
		GX_SetTevColorOp(stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
		GX_SetTevAlphaOp(stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
		stage++;

		GX_SetNumTevStages(stage);
		GX_SetTevKColor(GX_KCOLOR1, lightAmbient);
	}

	if(matDirty){
		GXColor k = { matColor.red, matColor.green, matColor.blue, matColor.alpha };
		GX_SetTevKColor(GX_KCOLOR0, k);
		matDirty = 0;
	}
}

void
flushCache(void)
{
	// a raster whose texels never made it would otherwise sample the last one bound
	Raster *raster = rwStateCache.texstage[0].raster;
	bool32 textured = 0;
	if(raster){
		GXRaster *natras = PLUGINOFFSET(GXRaster, raster, nativeRasterOffset);
		textured = natras->ready;
	}
	setupTev(textured);
	flushGXState();
}

// ------------------------------------------------------------ device

// freed once the frame the GP was reading them in is done
#define MAX_DEFERRED_FREES 1024
static void *deferredFrees[MAX_DEFERRED_FREES];
static int32 numDeferredFrees;

static void
flushDeferredFrees(void)
{
	for(int32 i = 0; i < numDeferredFrees; i++)
		free(deferredFrees[i]);
	numDeferredFrees = 0;
}

void
deferFree(void *p)
{
	if(p == nil)
		return;
	if(numDeferredFrees >= MAX_DEFERRED_FREES){
		GX_DrawDone();
		flushDeferredFrees();
	}
	deferredFrees[numDeferredFrees++] = p;
}

static void
setViewport(int32 x, int32 y, int32 w, int32 h)
{
	if(curGXState.viewportX != x || curGXState.viewportY != y ||
	   curGXState.viewportW != w || curGXState.viewportH != h){
		curGXState.viewportX = x;
		curGXState.viewportY = y;
		curGXState.viewportW = w;
		curGXState.viewportH = h;
		viewportDirty = 1;
	}
}

static void
getCameraViewport(Camera *cam, int32 *x, int32 *y, int32 *w, int32 *h)
{
	Raster *fb = cam->frameBuffer;
	*x = fb->offsetX;
	*y = fb->offsetY;
	*w = fb->width;
	*h = fb->height;
	if(*x + *w > (int32)globals.width)  *w = globals.width - *x;
	if(*y + *h > (int32)globals.height) *h = globals.height - *y;
	if(*w < 0) *w = 0;
	if(*h < 0) *h = 0;
}

static void
beginUpdate(Camera *cam)
{
	int32 x, y, w, h;
	getCameraViewport(cam, &x, &y, &w, &h);
	setViewport(x, y, w, h);
	setProjectionMatrix(cam);
	setViewMatrix(cam);

	curGXState.fogNear = cam->nearPlane;
	curGXState.fogFar = cam->farPlane;
	curGXState.fogStart = cam->fogPlane;
	curGXState.fogEnd = cam->farPlane;
	curGXState.fogMode = rwStateCache.fogEnable ?
		(cam->projection == Camera::PERSPECTIVE ? GX_FOG_PERSP_LIN : GX_FOG_ORTHO_LIN) :
		GX_FOG_NONE;
	stateDirty = 1;

	GX_InvVtxCache();
	GX_InvalidateTexAll();
}

static void
endUpdate(Camera *cam)
{
}

static void
clearCamera(Camera *cam, RGBA *col, uint32 mode)
{
	bool32 clearColor = (mode & Camera::CLEARIMAGE) != 0;
	bool32 clearDepth = (mode & Camera::CLEARZ) != 0;
	if(!clearColor && !clearDepth)
		return;

	int32 x, y, w, h;
	getCameraViewport(cam, &x, &y, &w, &h);

	Mtx44 proj;
	guOrtho(proj, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f);
	GX_LoadProjectionMtx(proj, GX_ORTHOGRAPHIC);
	Mtx identity;
	guMtxIdentity(identity);
	GX_LoadPosMtxImm(identity, GX_PNMTX0);
	GX_SetCurrentMtx(GX_PNMTX0);

	GX_SetViewport((f32)x, (f32)y, (f32)w, (f32)h, 0.0f, 1.0f);
	GX_SetScissor(x, y, w, h);
	GX_SetColorUpdate(clearColor ? GX_TRUE : GX_FALSE);
	GX_SetZMode(GX_TRUE, GX_ALWAYS, clearDepth ? GX_TRUE : GX_FALSE);
	GX_SetZCompLoc(GX_ENABLE);
	GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
	GX_SetCullMode(GX_CULL_NONE);
	GX_SetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0);
	GX_SetNumTexGens(0);
	GX_SetNumTevStages(1);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
	GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);

	invalidateBoundArrays();
	setLightingEnabled(0);
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);

	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
		GX_Position3f32(0.0f, 0.0f, -1.0f);
		GX_Color4u8(col->red, col->green, col->blue, col->alpha);
		GX_Position3f32(1.0f, 0.0f, -1.0f);
		GX_Color4u8(col->red, col->green, col->blue, col->alpha);
		GX_Position3f32(1.0f, 1.0f, -1.0f);
		GX_Color4u8(col->red, col->green, col->blue, col->alpha);
		GX_Position3f32(0.0f, 1.0f, -1.0f);
		GX_Color4u8(col->red, col->green, col->blue, col->alpha);
	GX_End();

	GX_SetColorUpdate(GX_TRUE);

	// the clear ran on its own state, force everything back on the next draw
	memset(&oldGXState, 0xFF, sizeof(oldGXState));
	tevTextured = ~0;
	matDirty = 1;
	sceneDirty = 1;
	stateDirty = 1;
	viewportDirty = 1;
}

static void
showRaster(Raster *raster, uint32 flags)
{
	if(raster == nil || raster->type != Raster::CAMERA)
		return;
	globals.activeBuffer ^= 1;
	void *fb = globals.frameBuffer[globals.activeBuffer];
	GX_SetColorUpdate(GX_TRUE);
	GX_SetZMode(GX_ENABLE, GX_ALWAYS, GX_ENABLE);
	GX_CopyDisp(fb, GX_TRUE);
	GX_DrawDone();
	flushDeferredFrees();
	VIDEO_SetNextFramebuffer(fb);
	VIDEO_Flush();
	VIDEO_WaitVSync();

	memset(&oldGXState, 0xFF, sizeof(oldGXState));
	stateDirty = 1;
	viewportDirty = 1;
}

static bool32
rasterRenderFast(Raster *raster, int32 x, int32 y)
{
	return 0;
}

static void
setRenderState(int32 state, void *pvalue)
{
	uint32 value = (uint32)(uintptr)pvalue;
	switch(state){
	case TEXTURERASTER:	setRasterStage(0, (Raster*)pvalue); break;
	case TEXTUREADDRESS:	setAddressU(0, value); setAddressV(0, value); break;
	case TEXTUREADDRESSU:	setAddressU(0, value); break;
	case TEXTUREADDRESSV:	setAddressV(0, value); break;
	case TEXTUREFILTER:	setFilterMode(0, value); break;
	case VERTEXALPHA:	setVertexAlpha(value); break;
	case SRCBLEND:
		if(rwStateCache.srcblend != value){
			rwStateCache.srcblend = value;
			setGXRenderState(RWGX_SRCBLEND, blendMap[value]);
		}
		break;
	case DESTBLEND:
		if(rwStateCache.destblend != value){
			rwStateCache.destblend = value;
			setGXRenderState(RWGX_DESTBLEND, blendMap[value]);
		}
		break;
	case ZTESTENABLE:	setDepthTest(value); break;
	case ZWRITEENABLE:	setDepthWrite(value); break;
	case FOGENABLE:
		if(rwStateCache.fogEnable != value){
			rwStateCache.fogEnable = value;
			curGXState.fogMode = value ? GX_FOG_PERSP_LIN : GX_FOG_NONE;
			stateDirty = 1;
		}
		break;
	case FOGCOLOR:
		if(rwStateCache.fogColor != value){
			rwStateCache.fogColor = value;
			curGXState.fogColor = value;
			stateDirty = 1;
		}
		break;
	case CULLMODE:
		if(rwStateCache.cullmode != value){
			rwStateCache.cullmode = value;
			// the X mirror in the modelview flips winding, so front/back swap
			setGXRenderState(RWGX_CULLMODE,
				value == CULLBACK ? GX_CULL_FRONT :
				value == CULLFRONT ? GX_CULL_BACK : GX_CULL_NONE);
		}
		break;
	case ALPHATESTFUNC:	setAlphaTestFunction(value); break;
	case ALPHATESTREF:
		if(rwStateCache.alphaRef != value){
			rwStateCache.alphaRef = value;
			setGXRenderState(RWGX_ALPHAREF, value);
		}
		break;
	case GSALPHATEST:	rwStateCache.gsalpha = value; break;
	case GSALPHATESTREF:	rwStateCache.gsalpharef = value; break;
	}
}

static void*
getRenderState(int32 state)
{
	uint32 value = 0;
	switch(state){
	case TEXTURERASTER:	return rwStateCache.texstage[0].raster;
	case TEXTUREADDRESS:
		if(rwStateCache.texstage[0].addressingU == rwStateCache.texstage[0].addressingV)
			value = rwStateCache.texstage[0].addressingU;
		else
			value = 0;
		break;
	case TEXTUREADDRESSU:	value = rwStateCache.texstage[0].addressingU; break;
	case TEXTUREADDRESSV:	value = rwStateCache.texstage[0].addressingV; break;
	case TEXTUREFILTER:	value = rwStateCache.texstage[0].filter; break;
	case VERTEXALPHA:	value = rwStateCache.vertexAlpha; break;
	case SRCBLEND:		value = rwStateCache.srcblend; break;
	case DESTBLEND:		value = rwStateCache.destblend; break;
	case ZTESTENABLE:	value = rwStateCache.ztest; break;
	case ZWRITEENABLE:	value = rwStateCache.zwrite; break;
	case FOGENABLE:		value = rwStateCache.fogEnable; break;
	case FOGCOLOR:		value = rwStateCache.fogColor; break;
	case CULLMODE:		value = rwStateCache.cullmode; break;
	case ALPHATESTFUNC:	value = rwStateCache.alphaFunc; break;
	case ALPHATESTREF:	value = rwStateCache.alphaRef; break;
	case GSALPHATEST:	value = rwStateCache.gsalpha; break;
	case GSALPHATESTREF:	value = rwStateCache.gsalpharef; break;
	}
	return (void*)(uintptr)value;
}

static void
resetRenderState(void)
{
	memset(&rwStateCache, 0, sizeof(rwStateCache));
	rwStateCache.alphaFunc = ALPHAGREATEREQUAL;
	rwStateCache.alphaRef = 10;
	rwStateCache.gsalpharef = 128;
	rwStateCache.srcblend = BLENDSRCALPHA;
	rwStateCache.destblend = BLENDINVSRCALPHA;
	rwStateCache.zwrite = 1;
	rwStateCache.ztest = 1;
	rwStateCache.cullmode = CULLNONE;
	for(int32 i = 0; i < MAXNUMSTAGES; i++){
		rwStateCache.texstage[i].raster = nil;
		rwStateCache.texstage[i].filter = Texture::LINEAR;
		rwStateCache.texstage[i].addressingU = Texture::WRAP;
		rwStateCache.texstage[i].addressingV = Texture::WRAP;
	}

	memset(&curGXState, 0, sizeof(curGXState));
	curGXState.blendEnable = 0;
	curGXState.srcblend = blendMap[BLENDSRCALPHA];
	curGXState.destblend = blendMap[BLENDINVSRCALPHA];
	curGXState.depthTestEnable = 1;
	curGXState.depthFunc = GX_LEQUAL;
	curGXState.depthMask = 1;
	curGXState.cullMode = GX_CULL_NONE;
	curGXState.alphaTestEnable = 0;
	curGXState.alphaFunc = GX_GEQUAL;
	curGXState.alphaRef = 10;
	curGXState.fogMode = GX_FOG_NONE;
	memset(&oldGXState, 0xFF, sizeof(oldGXState));

	GX_SetNumTevStages(1);
	GX_SetNumIndStages(0);
	GX_SetZTexture(GX_ZT_DISABLE, GX_TF_Z24X8, 0);
	GX_SetTevSwapModeTable(GX_TEV_SWAP0, GX_CH_RED, GX_CH_GREEN, GX_CH_BLUE, GX_CH_ALPHA);
	GX_SetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);
	GX_SetNumChans(1);
	GX_SetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_REG, GX_SRC_VTX,
		0, GX_DF_NONE, GX_AF_NONE);

	lightingOn = 0;
	loadedLightCount = -1;
	loadedAmbient = ~0u;
	activeLightCount = 0;
	tevTextured = ~0;
	matDirty = 1;
	sceneDirty = 1;
	stateDirty = 1;
	viewportDirty = 1;
}

static void
openGX(void)
{
	VIDEO_Init();
	globals.rmode = VIDEO_GetPreferredMode(NULL);

	for(int32 i = 0; i < 2; i++){
		globals.frameBuffer[i] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(globals.rmode));
		// the system menu leaves garbage in these
		VIDEO_ClearFrameBuffer(globals.rmode, globals.frameBuffer[i], COLOR_BLACK);
	}
	globals.activeBuffer = 0;
	globals.width = globals.rmode->fbWidth;
	globals.height = globals.rmode->xfbHeight;

	VIDEO_Configure(globals.rmode);
	VIDEO_SetNextFramebuffer(globals.frameBuffer[0]);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(globals.rmode->viTVMode & VI_NON_INTERLACE)
		VIDEO_WaitVSync();

	globals.fifo = memalign(32, FIFO_SIZE);
	memset(globals.fifo, 0, FIFO_SIZE);
	DCFlushRange(globals.fifo, FIFO_SIZE);
	GX_Init(globals.fifo, FIFO_SIZE);

	GXColor background = { 0, 0, 0, 255 };
	GX_SetCopyClear(background, GX_MAX_Z24);
	GX_SetViewport(0.0f, 0.0f, globals.rmode->fbWidth, globals.rmode->efbHeight, 0.0f, 1.0f);
	GX_SetDispCopyYScale((f32)globals.rmode->xfbHeight/(f32)globals.rmode->efbHeight);
	GX_SetScissor(0, 0, globals.rmode->fbWidth, globals.rmode->efbHeight);
	GX_SetDispCopySrc(0, 0, globals.rmode->fbWidth, globals.rmode->efbHeight);
	GX_SetDispCopyDst(globals.rmode->fbWidth, globals.rmode->xfbHeight);
	GX_SetCopyFilter(globals.rmode->aa, globals.rmode->sample_pattern, GX_TRUE,
		globals.rmode->vfilter);
	GX_SetFieldMode(globals.rmode->field_rendering,
		globals.rmode->viHeight == 2*globals.rmode->xfbHeight ? GX_ENABLE : GX_DISABLE);
	GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
	GX_SetCullMode(GX_CULL_NONE);
	GX_SetDispCopyGamma(GX_GM_1_0);
	GX_CopyDisp(globals.frameBuffer[1], GX_TRUE);
}

static int
deviceSystemGX(DeviceReq req, void *arg, int32 n)
{
	VideoMode *rwmode;

	switch(req){
	case DEVICEOPEN:
		openGX();
		return 1;
	case DEVICECLOSE:
		return 1;
	case DEVICEINIT:
		resetRenderState();
		return 1;
	case DEVICEFINALIZE:
	case DEVICETERM:
		return 1;

	case DEVICEGETNUMSUBSYSTEMS:
		return 1;
	case DEVICEGETCURRENTSUBSYSTEM:
		return 0;
	case DEVICESETSUBSYSTEM:
		return n == 0;
	case DEVICEGETSUBSSYSTEMINFO:
		if(n >= 1 || arg == nil)
			return 0;
		strncpy(((SubSystemInfo*)arg)->name, "Wii", sizeof(SubSystemInfo::name));
		return 1;

	case DEVICEGETNUMVIDEOMODES:
		return 1;
	case DEVICEGETCURRENTVIDEOMODE:
		return 0;
	case DEVICESETVIDEOMODE:
		return n == 0;
	case DEVICEGETVIDEOMODEINFO:
		if(n >= 1 || arg == nil)
			return 0;
		rwmode = (VideoMode*)arg;
		rwmode->width = globals.width ? globals.width : 640;
		rwmode->height = globals.height ? globals.height : 480;
		rwmode->depth = 32;
		rwmode->flags = VIDEOMODEEXCLUSIVE;
		return 1;

	case DEVICEGETMAXMULTISAMPLINGLEVELS:
	case DEVICEGETMULTISAMPLINGLEVELS:
	case DEVICESETMULTISAMPLINGLEVELS:
		return 1;

	default:
		return 0;
	}
}

Device renderdevice = {
	// GX exposes post-projection depth as 0..1, like D3D
	0.0f, 1.0f,
	gx::beginUpdate,
	gx::endUpdate,
	gx::clearCamera,
	gx::showRaster,
	gx::rasterRenderFast,
	gx::setRenderState,
	gx::getRenderState,
	gx::im2DRenderLine,
	gx::im2DRenderTriangle,
	gx::im2DRenderPrimitive,
	gx::im2DRenderIndexedPrimitive,
	gx::im3DTransform,
	gx::im3DRenderPrimitive,
	gx::im3DRenderIndexedPrimitive,
	gx::im3DEnd,
	gx::deviceSystemGX
};

}
}

#endif
