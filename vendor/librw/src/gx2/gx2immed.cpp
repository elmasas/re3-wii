#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <malloc.h>

#include "../rwbase.h"
#include "../rwerror.h"
#include "../rwplg.h"
#include "../rwrender.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "../rwengine.h"

#ifdef __WIIU__
#include "rwgx2.h"
#include "rwgx2impl.h"
#include "rwgx2shader.h"

#include "shaders/im2d.h"
#include "shaders/im3d.h"

#include <vector>

namespace rw {
namespace gx2 {

static GX2PrimitiveMode primTypeMap[] = {
	GX2_PRIMITIVE_MODE_POINTS, // invalid
	GX2_PRIMITIVE_MODE_LINES,
	GX2_PRIMITIVE_MODE_LINE_STRIP,
	GX2_PRIMITIVE_MODE_TRIANGLES,
	GX2_PRIMITIVE_MODE_TRIANGLE_STRIP,
	GX2_PRIMITIVE_MODE_TRIANGLE_FAN,
	GX2_PRIMITIVE_MODE_POINTS
};

// Im2D

static Shader *im2DShader;
Shader *im2DOverrideShader;

static int32 u_xform;

void
openIm2D(void)
{
	u_xform = registerUniform("u_xform", UNIFORM_VEC4);

	im2DShader = Shader::create(im2d_gsh, GX2_SHADER_MODE_UNIFORM_REGISTER);
	assert(im2DShader);

	im2DShader->initAttribute("in_pos", 0, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32);
	im2DShader->initAttribute("in_color", 16, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32);
	im2DShader->initAttribute("in_tex0", 32, GX2_ATTRIB_FORMAT_FLOAT_32_32);

	im2DShader->init();
	im2DShader->use();

	im2DShader->samplerLocation = GX2GetPixelSamplerVarLocation(im2DShader->group.pixelShader, "tex0");
}

void
closeIm2D(void)
{
	im2DShader->destroy();
	im2DShader = nil;
}

static Im2DVertex tmpprimbuf[3];

void
im2DRenderLine(void *vertices, int32 numVertices, int32 vert1, int32 vert2)
{
	Im2DVertex *verts = (Im2DVertex*)vertices;
	tmpprimbuf[0] = verts[vert1];
	tmpprimbuf[1] = verts[vert2];
	im2DRenderPrimitive(PRIMTYPELINELIST, tmpprimbuf, 2);
}

void
im2DRenderTriangle(void *vertices, int32 numVertices, int32 vert1, int32 vert2, int32 vert3)
{
	Im2DVertex *verts = (Im2DVertex*)vertices;
	tmpprimbuf[0] = verts[vert1];
	tmpprimbuf[1] = verts[vert2];
	tmpprimbuf[2] = verts[vert3];
	im2DRenderPrimitive(PRIMTYPETRILIST, tmpprimbuf, 3);
}

void
im2DSetXform(void)
{
	float xform[4];
	Camera *cam;
	cam = (Camera*)engine->currentCamera;
	xform[0] = 2.0f/cam->frameBuffer->width;
	xform[1] = -2.0f/cam->frameBuffer->height;
	xform[2] = -1.0f;
	xform[3] = 1.0f;
	setUniform(u_xform, xform);
}

void
im2DRenderPrimitive(PrimitiveType primType, void *vertices, int32 numVertices)
{
	uint32 vbo_lenght = numVertices * 10 * sizeof(float);
	float* vbo_verts = (float*) memalign(GX2_VERTEX_BUFFER_ALIGNMENT, vbo_lenght);
	addToVertexFreeBuffer(vbo_verts);

	int v = 0;
	for (int i = 0; i < numVertices; i++)
	{
		Im2DVertex vert = ((Im2DVertex*)vertices)[i];
		// pos
		vbo_verts[v++] = vert.x;
		vbo_verts[v++] = vert.y;
		vbo_verts[v++] = vert.z;
		vbo_verts[v++] = vert.w;
		// color
		vbo_verts[v++] = vert.r/255.0f;
		vbo_verts[v++] = vert.g/255.0f;
		vbo_verts[v++] = vert.b/255.0f;
		vbo_verts[v++] = vert.a/255.0f;
		// tex coord
		vbo_verts[v++] = vert.u;
		vbo_verts[v++] = vert.v;
	}

	GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, vbo_verts, vbo_lenght);

	if (im2DOverrideShader)
		im2DOverrideShader->use();
	else
		im2DShader->use();
	
	im2DSetXform();

	flushCache();
	GX2SetAttribBuffer(0, vbo_lenght, 10 * sizeof(float), vbo_verts);
	GX2DrawEx(primTypeMap[primType], numVertices, 0, 1);
}

void
im2DRenderIndexedPrimitive(PrimitiveType primType,
	void *vertices, int32 numVertices,
	void *indices, int32 numIndices)
{
	uint32 vbo_lenght = numVertices * 10 * sizeof(float);
	float* vbo_verts = (float*) memalign(GX2_VERTEX_BUFFER_ALIGNMENT, vbo_lenght);
	addToVertexFreeBuffer(vbo_verts);

	int v = 0;
	for (int i = 0; i < numVertices; i++)
	{
		Im2DVertex vert = ((Im2DVertex*)vertices)[i];
		// pos
		vbo_verts[v++] = vert.x;
		vbo_verts[v++] = vert.y;
		vbo_verts[v++] = vert.z;
		vbo_verts[v++] = vert.w;
		// color
		vbo_verts[v++] = vert.r/255.0f;
		vbo_verts[v++] = vert.g/255.0f;
		vbo_verts[v++] = vert.b/255.0f;
		vbo_verts[v++] = vert.a/255.0f;
		// tex coord
		vbo_verts[v++] = vert.u;
		vbo_verts[v++] = vert.v;
	}
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, vbo_verts, vbo_lenght);

	if (im2DOverrideShader)
		im2DOverrideShader->use();
	else
		im2DShader->use();
	
	im2DSetXform();

	flushCache();
	GX2SetAttribBuffer(0, vbo_lenght, 10 * sizeof(float), vbo_verts);
	GX2DrawIndexedImmediateEx(primTypeMap[primType], numIndices, GX2_INDEX_TYPE_U16, indices, 0, 1);
}

// Im3D

static Shader *im3DShader;

static int32 num3DVertices;	

void
openIm3D(void)
{
	im3DShader = Shader::create(im3d_gsh, GX2_SHADER_MODE_UNIFORM_REGISTER);
	assert(im3DShader);

	im3DShader->initAttribute("in_pos", 0, GX2_ATTRIB_FORMAT_FLOAT_32_32_32);
	im3DShader->initAttribute("in_color", 12, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32);
	im3DShader->initAttribute("in_tex0", 28, GX2_ATTRIB_FORMAT_FLOAT_32_32);

	im3DShader->init();
	im3DShader->use();

	im3DShader->samplerLocation = GX2GetPixelSamplerVarLocation(im3DShader->group.pixelShader, "tex0");
}

void
closeIm3D(void)
{
	im3DShader->destroy();
	im3DShader = nil;
}

void
im3DTransform(void *vertices, int32 numVertices, Matrix *world, uint32 flags)
{
	if(world == nil)
	{
		Matrix ident;
		ident.setIdentity();
		world = &ident;
	}
	setWorldMatrix(world);

	im3DShader->use();

	uint32 vbo_lenght = numVertices * 9 * sizeof(float);
	float* vbo_verts = (float*) memalign(GX2_VERTEX_BUFFER_ALIGNMENT, vbo_lenght);
	addToVertexFreeBuffer(vbo_verts);

	int v = 0;
	for (int i = 0; i < numVertices; i++)
	{
		Im3DVertex vert = ((Im3DVertex*)vertices)[i];
		// pos
		vbo_verts[v++] = vert.position.x;
		vbo_verts[v++] = vert.position.y;
		vbo_verts[v++] = vert.position.z;
		// color
		vbo_verts[v++] = vert.r/255.0f;
		vbo_verts[v++] = vert.g/255.0f;
		vbo_verts[v++] = vert.b/255.0f;
		vbo_verts[v++] = vert.a/255.0f;
		// tex coord
		vbo_verts[v++] = vert.u;
		vbo_verts[v++] = vert.v;
	}
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, vbo_verts, vbo_lenght);

	GX2SetAttribBuffer(0, vbo_lenght, 9 * sizeof(float), vbo_verts);

	num3DVertices = numVertices;
}

void
im3DRenderPrimitive(PrimitiveType primType)
{
	flushCache();
	GX2DrawEx(primTypeMap[primType], num3DVertices, 0, 1);
}

void
im3DRenderIndexedPrimitive(PrimitiveType primType, void *indices, int32 numIndices)
{
	flushCache();
	GX2DrawIndexedImmediateEx(primTypeMap[primType], numIndices, GX2_INDEX_TYPE_U16, indices, 0, 1);
}

void
im3DEnd(void)
{

}

}
}

#endif
