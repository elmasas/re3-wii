#ifdef __WIIU__
#include <gx2/surface.h>
#include <gx2/texture.h>
#include <gx2/event.h>
#include <gx2/draw.h>
#include <gx2/mem.h>
#include <gx2/registers.h>
#include <gx2/state.h>
#include <gx2/swap.h>
#include <gx2/clear.h>
#include <gx2/context.h>
#include <gx2/display.h>
#include <gx2/shaders.h>
#include <gx2/temp.h>
#include <gx2/utils.h>
#include <gx2r/mem.h>
#include <gx2r/buffer.h>
#include <gx2r/draw.h>
#include <gx2r/surface.h>
#endif

namespace rw {

#ifdef __WIIU__
struct EngineOpenParams
{
 	void **window;
	int width, height;
	const char *windowtitle;
};
#endif

namespace gx2 {

void registerPlatformPlugins(void);

extern Device renderdevice;

struct AttribDesc
{
	uint32 index;
	int32  size;
	uint32 stride;
	uint32 offset;
};

enum AttribIndices
{
	ATTRIB_POS = 0,
	ATTRIB_NORMAL,
	ATTRIB_COLOR,
	ATTRIB_TEXCOORDS0,
	ATTRIB_WEIGHTS,
	ATTRIB_INDICES
};

// default uniform indices
extern int32 u_matColor;
extern int32 u_surfProps;

struct InstanceData
{
	uint32    numIndex;
	uint32    minVert;
	uint32    numVertices;
	Material *material;
	bool32    vertexAlpha;
	uint32    offset;
};

#ifdef __WIIU__
struct InstanceDataHeader : rw::InstanceDataHeader
{
	uint32  serialNumber;
	uint32  numMeshes;
	uint16* indexBuffer;
	GX2PrimitiveMode  primType;
	float* vertexBuffer;
	uint32  totalNumIndex;
	uint32  totalNumVertex;
	uint32  numAttribs;
	AttribDesc* attribDesc;

	InstanceData *inst;
};
#endif

#ifdef __WIIU__

inline float _floatswap32(float f)
{
	uint32_t* tmp = (uint32_t*)&f;
	uint32_t _swapval = __builtin_bswap32(*tmp);
	float* tmp2 = (float*)&_swapval;
	return *tmp2;
}

struct Shader;

extern Shader *defaultShader;

struct Im3DVertex
{
	V3d     position;
	uint8   r, g, b, a;
	float32 u, v;

	void setX(float32 x) { this->position.x = x; }
	void setY(float32 y) { this->position.y = y; }
	void setZ(float32 z) { this->position.z = z; }
	void setColor(uint8 r, uint8 g, uint8 b, uint8 a) {
		this->r = r; this->g = g; this->b = b; this->a = a; }
	void setU(float32 u) { this->u = u; }
	void setV(float32 v) { this->v = v; }

	float getX(void) { return this->position.x; }
	float getY(void) { return this->position.y; }
	float getZ(void) { return this->position.z; }
	RGBA getColor(void) { return makeRGBA(this->r, this->g, this->b, this->a); }
	float getU(void) { return this->u; }
	float getV(void) { return this->v; }
};

struct Im2DVertex
{
	float32 x, y, z, w;
	uint8   r, g, b, a;
	float32 u, v;

	void setScreenX(float32 x) { this->x = x; }
	void setScreenY(float32 y) { this->y = y; }
	void setScreenZ(float32 z) { this->z = z; }
	void setCameraZ(float32 z) { this->w = z; }
	void setRecipCameraZ(float32 recipz) { this->w = 1.0f/recipz; }
	void setColor(uint8 r, uint8 g, uint8 b, uint8 a) {
		this->r = r; this->g = g; this->b = b; this->a = a; }
	void setU(float32 u, float recipz) { this->u = u; }
	void setV(float32 v, float recipz) { this->v = v; }

	float getScreenX(void) { return this->x; }
	float getScreenY(void) { return this->y; }
	float getScreenZ(void) { return this->z; }
	float getCameraZ(void) { return this->w; }
	float getRecipCameraZ(void) { return 1.0f/this->w; }
	RGBA getColor(void) { return makeRGBA(this->r, this->g, this->b, this->a); }
	float getU(void) { return this->u; }
	float getV(void) { return this->v; }
};

// Render state

// Vertex shader bits
enum
{
	// These should be low so they could be used as indices
	VSLIGHT_DIRECT	= 1,
	VSLIGHT_POINT	= 2,
	VSLIGHT_SPOT	= 4,
	VSLIGHT_MASK	= 7,	// all the above
	// less critical
	VSLIGHT_AMBIENT = 8,
};

extern Shader *im2DOverrideShader;

// per Scene
void setProjectionMatrix(float32*);
void setViewMatrix(float32*);

// per Object
void setWorldMatrix(Matrix*);
int32 setLights(WorldLights *lightData);

// per Mesh
void setTexture(int32 n, Texture *tex);
void setMaterial(const RGBA &color, const SurfaceProperties &surfaceprops, float extraSurfProp = 0.0f);
inline void setMaterial(uint32 flags, const RGBA &color, const SurfaceProperties &surfaceprops, float extraSurfProp = 0.0f)
{
	static RGBA white = { 255, 255, 255, 255 };
	if(flags & Geometry::MODULATE)
		setMaterial(color, surfaceprops, extraSurfProp);
	else
		setMaterial(white, surfaceprops, extraSurfProp);
}

void setAlphaBlend(bool32 enable);
bool32 getAlphaBlend(void);

bool32 getAlphaTest(void);

void flushCache(void);

#endif

class ObjPipeline : public rw::ObjPipeline
{
public:
	void init(void);
	static ObjPipeline *create(void);

	void (*instanceCB)(Geometry *geo, InstanceDataHeader *header, bool32 reinstance);
	void (*uninstanceCB)(Geometry *geo, InstanceDataHeader *header);
	void (*renderCB)(Atomic *atomic, InstanceDataHeader *header);
};

void defaultInstanceCB(Geometry *geo, InstanceDataHeader *header, bool32 reinstance);
void defaultUninstanceCB(Geometry *geo, InstanceDataHeader *header);
void defaultRenderCB(Atomic *atomic, InstanceDataHeader *header);
int32 lightingCB(Atomic *atomic);

void drawInst_simple(InstanceDataHeader *header, InstanceData *inst);
// Emulate PS2 GS alpha test FB_ONLY case: failed alpha writes to frame- but not to depth buffer
void drawInst_GSemu(InstanceDataHeader *header, InstanceData *inst);
// This one switches between the above two depending on render state;
void drawInst(InstanceDataHeader *header, InstanceData *inst);


void *destroyNativeData(void *object, int32, int32);

ObjPipeline *makeDefaultPipeline(void);

// Native Texture and Raster

extern int32 nativeRasterOffset;

extern bool gfxInForeground;
void addToMEM1Buffer(void** buffer, uint32 size, uint32 alignment);
void removeFromMEM1Buffer(void* buffer);

#ifdef __WIIU__
struct GX2Raster
{
	// will be casted to GX2Texture / GX2ColorBuffer / GX2DepthBuffer
	void* texture;
	// colorbuffer for rendering to a camera texture
	void* texture2;
	GX2Sampler* sampler;

	uint32_t bpp;
	bool isCompressed;
	bool hasAlpha;
	bool autogenMipmap;
	int8 numLevels;

	int32 filterMode;
	int32 addressU;
	int32 addressV;
	int32 maxAnisotropy;
};
#endif

Texture *readNativeTexture(Stream *stream);
void writeNativeTexture(Texture *tex, Stream *stream);
uint32 getSizeNativeTexture(Texture *tex);

void registerNativeRaster(void);

// everything added to this buffer will be free'd once a frame has been rendered
void addToVertexFreeBuffer(void* vert);

}
}
