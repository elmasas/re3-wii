#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static uint8 primTypeMap[] = {
	GX_POINTS,
	GX_LINES,
	GX_LINESTRIP,
	GX_TRIANGLES,
	GX_TRIANGLESTRIP,
	GX_TRIANGLEFAN,
	GX_POINTS
};

static Im3DVertex *im3dVertices;
static int32 im3dNumVertices;

// immediate submission always reprograms the format, it never inherits one
static bool32
setupImmediateVertexFormat(void)
{
	bool32 textured = getRasterStage(0) != nil;
	invalidateBoundArrays();

	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	if(textured){
		GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
		GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
	}
	return textured;
}

static void
setupIm2DMatrices(void)
{
	Camera *cam = (Camera*)engine->currentCamera;
	float32 w = cam ? (float32)cam->frameBuffer->width : 640.0f;
	float32 h = cam ? (float32)cam->frameBuffer->height : 480.0f;

	// Im2D z is 0..1, GX wants eye space z negative, so it is submitted negated
	Mtx44 proj;
	guOrtho(proj, 0.0f, h, 0.0f, w, 0.0f, 1.0f);
	GX_LoadProjectionMtx(proj, GX_ORTHOGRAPHIC);

	Mtx identity;
	guMtxIdentity(identity);
	GX_LoadPosMtxImm(identity, GX_PNMTX0);
	GX_SetCurrentMtx(GX_PNMTX0);

	sceneDirty = 1;
}

static void
setupImmediate(bool32 is2D)
{
	if(is2D)
		setupIm2DMatrices();
	// immediate colours are final, no ambient must be added on top
	setLightingEnabled(0);
	RGBA white = { 255, 255, 255, 255 };
	SurfaceProperties surf = { 1.0f, 1.0f, 1.0f };
	setMaterial(white, surf);
	flushCache();
}

void
im2DRenderPrimitive(PrimitiveType primType, void *vertices, int32 numVertices)
{
	Im2DVertex *verts = (Im2DVertex*)vertices;
	setupImmediate(1);
	bool32 textured = setupImmediateVertexFormat();

	GX_Begin(primTypeMap[primType], GX_VTXFMT0, (uint16)numVertices);
	for(int32 i = 0; i < numVertices; i++){
		GX_Position3f32(verts[i].x, verts[i].y, -verts[i].z);
		GX_Color4u8(verts[i].r, verts[i].g, verts[i].b, verts[i].a);
		if(textured)
			GX_TexCoord2f32(verts[i].u, verts[i].v);
	}
	GX_End();
}

void
im2DRenderIndexedPrimitive(PrimitiveType primType, void *vertices,
	int32 numVertices, void *indices, int32 numIndices)
{
	Im2DVertex *verts = (Im2DVertex*)vertices;
	uint16 *idx = (uint16*)indices;
	setupImmediate(1);
	bool32 textured = setupImmediateVertexFormat();

	GX_Begin(primTypeMap[primType], GX_VTXFMT0, (uint16)numIndices);
	for(int32 i = 0; i < numIndices; i++){
		Im2DVertex *v = &verts[idx[i]];
		GX_Position3f32(v->x, v->y, -v->z);
		GX_Color4u8(v->r, v->g, v->b, v->a);
		if(textured)
			GX_TexCoord2f32(v->u, v->v);
	}
	GX_End();
}

void
im2DRenderLine(void *vertices, int32 numVertices, int32 vert1, int32 vert2)
{
	Im2DVertex *verts = (Im2DVertex*)vertices;
	Im2DVertex line[2];
	line[0] = verts[vert1];
	line[1] = verts[vert2];
	im2DRenderPrimitive(PRIMTYPELINELIST, line, 2);
}

void
im2DRenderTriangle(void *vertices, int32 numVertices, int32 vert1, int32 vert2,
	int32 vert3)
{
	Im2DVertex *verts = (Im2DVertex*)vertices;
	Im2DVertex tri[3];
	tri[0] = verts[vert1];
	tri[1] = verts[vert2];
	tri[2] = verts[vert3];
	im2DRenderPrimitive(PRIMTYPETRILIST, tri, 3);
}

void
im3DTransform(void *vertices, int32 numVertices, Matrix *world, uint32 flags)
{
	static Matrix identity;
	if(world == nil){
		identity.setIdentity();
		world = &identity;
	}
	setWorldMatrix(world);
	im3dVertices = (Im3DVertex*)vertices;
	im3dNumVertices = numVertices;
}

static void
im3DDraw(uint8 prim, uint16 *indices, int32 numIndices)
{
	if(im3dVertices == nil)
		return;
	setupImmediate(0);
	bool32 textured = setupImmediateVertexFormat();

	GX_Begin(prim, GX_VTXFMT0, (uint16)numIndices);
	for(int32 i = 0; i < numIndices; i++){
		Im3DVertex *v = &im3dVertices[indices ? indices[i] : i];
		GX_Position3f32(v->position.x, v->position.y, v->position.z);
		GX_Color4u8(v->r, v->g, v->b, v->a);
		if(textured)
			GX_TexCoord2f32(v->u, v->v);
	}
	GX_End();
}

void
im3DRenderPrimitive(PrimitiveType primType)
{
	im3DDraw(primTypeMap[primType], nil, im3dNumVertices);
}

void
im3DRenderIndexedPrimitive(PrimitiveType primType, void *indices,
	int32 numIndices)
{
	im3DDraw(primTypeMap[primType], (uint16*)indices, numIndices);
}

void
im3DEnd(void)
{
	im3dVertices = nil;
	im3dNumVertices = 0;
}

}
}

#endif
