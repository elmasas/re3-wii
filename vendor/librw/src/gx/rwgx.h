#ifdef __WII__
#include <gccore.h>
#endif

namespace rw {

#ifdef __WII__
struct EngineOpenParams
{
	void **window;
	int width, height;
	const char *windowtitle;
};
#endif

namespace gx {

void registerPlatformPlugins(void);

extern Device renderdevice;

enum AttribIndices
{
	ATTRIB_POS = 0,
	ATTRIB_NORMAL,
	ATTRIB_COLOR,
	ATTRIB_TEXCOORDS0
};

// normals are GX_S8, unit length uses -64..64
enum { NORMAL_FRAC = 6 };

struct InstanceData
{
	uint32    numIndex;
	uint32    minVert;
	uint32    numVertices;
	Material *material;
	bool32    vertexAlpha;
	uint32    offset;
};

struct InstanceDataHeader : rw::InstanceDataHeader
{
	uint32  serialNumber;
	uint32  numMeshes;
	uint16 *indexBuffer;
	uint8   primType;
	uint32  totalNumIndex;
	uint32  totalNumVertex;

	// GX fetches one array per attribute, not an interleaved buffer
	V3d       *positions;
	int8      *normals;
	RGBA      *colors;
	int16     *texCoords;
	uint8      texCoordFrac;
	bool32     hasNormals;
	bool32     hasTexCoords;

	InstanceData *inst;
};

#ifdef __WII__

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

// per Scene
void setProjectionMatrix(Camera *camera);
void setViewMatrix(Camera *camera);

// per Object
void setWorldMatrix(Matrix *world);
int32 setLights(WorldLights *lightData);
extern int32 activeLightCount;
void invalidateLightCache(void);
void setLightingEnabled(bool32 enable);

// per Mesh
void setTexture(int32 n, Texture *tex);
void setMaterial(const RGBA &color, const SurfaceProperties &surfaceprops);
inline void setMaterial(uint32 flags, const RGBA &color, const SurfaceProperties &surfaceprops)
{
	static RGBA white = { 255, 255, 255, 255 };
	if(flags & Geometry::MODULATE)
		setMaterial(color, surfaceprops);
	else
		setMaterial(white, surfaceprops);
}

void setAlphaBlend(bool32 enable);
bool32 getAlphaBlend(void);
bool32 getAlphaTest(void);

void flushCache(void);
extern bool32 sceneDirty;

void setRasterStage(int32 stage, Raster *raster);
Raster *getRasterStage(int32 stage);
void evictRaster(Raster *raster);

void deferFree(void *p);


void bindArrays(InstanceDataHeader *header);
void invalidateBoundArrays(void);

void im2DRenderLine(void *vertices, int32 numVertices, int32 vert1, int32 vert2);
void im2DRenderTriangle(void *vertices, int32 numVertices, int32 vert1, int32 vert2, int32 vert3);
void im2DRenderPrimitive(PrimitiveType primType, void *vertices, int32 numVertices);
void im2DRenderIndexedPrimitive(PrimitiveType primType, void *vertices, int32 numVertices, void *indices, int32 numIndices);
void im3DTransform(void *vertices, int32 numVertices, Matrix *world, uint32 flags);
void im3DRenderPrimitive(PrimitiveType primType);
void im3DRenderIndexedPrimitive(PrimitiveType primType, void *indices, int32 numIndices);
void im3DEnd(void);

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
void freeInstanceData(Geometry *geometry);

ObjPipeline *makeDefaultPipeline(void);
ObjPipeline *makeSkinPipeline(void);
ObjPipeline *makeMatFXPipeline(void);
void initSkin(void);
void initMatFX(void);

// Native Texture and Raster

extern int32 nativeRasterOffset;

#ifdef __WII__
struct GXRaster
{
	uint8 *pixels;
	uint8 *texels;
	uint32 texelsSize;
	uint8  format;
	GXTexObj tex;

	bool32 ready;
	bool32 hasAlpha;
	int8   numLevels;

	int32 filterMode;
	int32 addressU;
	int32 addressV;
};

uint32 texelsSizeForLevel(int32 width, int32 height, uint8 format);
uint32 texelsSize(int32 width, int32 height, uint8 format, int32 numLevels);
void tileTexels(const uint8 *src, int32 stride, int32 width, int32 height, uint8 *dst, uint8 format);
void updateSampler(GXRaster *natras);
bool32 rasterFromDxt1(Raster *raster, const uint8 *src, uint32 srcSize, bool32 hasAlpha);
#endif

Texture *readNativeTexture(Stream *stream);
void writeNativeTexture(Texture *tex, Stream *stream);
uint32 getSizeNativeTexture(Texture *tex);

void registerNativeRaster(void);

Raster *rasterCreate(Raster *raster);
uint8 *rasterLock(Raster *raster, int32 level, int32 lockMode);
void rasterUnlock(Raster *raster, int32 level);
uint8 *rasterLockPalette(Raster *raster, int32 lockMode);
void rasterUnlockPalette(Raster *raster);
int32 rasterNumLevels(Raster *raster);
bool32 imageFindRasterFormat(Image *image, int32 type, int32 *width, int32 *height, int32 *depth, int32 *format);
bool32 rasterFromImage(Raster *raster, Image *image);
Image *rasterToImage(Raster *raster);

}
}
