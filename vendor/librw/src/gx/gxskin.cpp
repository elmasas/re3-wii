#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <malloc.h>

#include "../rwbase.h"
#include "../rwerror.h"
#include "../rwplg.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "../rwengine.h"
#include "../rwrender.h"
#include "../rwanim.h"
#include "../rwplugins.h"

#ifdef __WII__

#include "rwgx.h"

namespace rw {
namespace gx {

#define MAXBONES 64

static Matrix skinMatrices[MAXBONES];

static void
buildSkinMatrices(Atomic *atomic, Skin *skin)
{
	HAnimHierarchy *hier = Skin::getHierarchy(atomic);
	int32 numBones = skin->numBones < MAXBONES ? skin->numBones : MAXBONES;

	if(hier == nil){
		for(int32 i = 0; i < numBones; i++)
			skinMatrices[i].setIdentity();
		return;
	}

	Matrix *invMats = (Matrix*)skin->inverseMatrices;
	if(numBones > hier->numNodes)
		numBones = hier->numNodes;

	if(hier->flags & HAnimHierarchy::LOCALSPACEMATRICES){
		for(int32 i = 0; i < numBones; i++){
			invMats[i].flags = 0;
			Matrix::mult(&skinMatrices[i], &invMats[i], &hier->matrices[i]);
		}
	}else{
		Matrix invAtmMat, tmp;
		Matrix::invert(&invAtmMat, atomic->getFrame()->getLTM());
		for(int32 i = 0; i < numBones; i++){
			invMats[i].flags = 0;
			Matrix::mult(&tmp, &hier->matrices[i], &invAtmMat);
			Matrix::mult(&skinMatrices[i], &invMats[i], &tmp);
		}
	}
}

// GX has no weight blending, so it is done here
static void
skinVertex(V3d *dst, const V3d *v, const uint8 *indices, const float32 *weights)
{
	float32 total = 0.0f;
	dst->x = dst->y = dst->z = 0.0f;
	for(int32 w = 0; w < 4; w++){
		float32 weight = weights[w];
		if(weight == 0.0f || indices[w] >= MAXBONES)
			continue;
		Matrix *m = &skinMatrices[indices[w]];
		dst->x += weight*(v->x*m->right.x + v->y*m->up.x + v->z*m->at.x + m->pos.x);
		dst->y += weight*(v->x*m->right.y + v->y*m->up.y + v->z*m->at.y + m->pos.y);
		dst->z += weight*(v->x*m->right.z + v->y*m->up.z + v->z*m->at.z + m->pos.z);
		total += weight;
	}
	if(total == 0.0f)
		*dst = *v;
}

static void
skinNormal(V3d *dst, const V3d *n, const uint8 *indices, const float32 *weights)
{
	float32 total = 0.0f;
	dst->x = dst->y = dst->z = 0.0f;
	for(int32 w = 0; w < 4; w++){
		float32 weight = weights[w];
		if(weight == 0.0f || indices[w] >= MAXBONES)
			continue;
		Matrix *m = &skinMatrices[indices[w]];
		dst->x += weight*(n->x*m->right.x + n->y*m->up.x + n->z*m->at.x);
		dst->y += weight*(n->x*m->right.y + n->y*m->up.y + n->z*m->at.y);
		dst->z += weight*(n->x*m->right.z + n->y*m->up.z + n->z*m->at.z);
		total += weight;
	}
	if(total == 0.0f)
		*dst = *n;
}

// direct submission, a shared array would race the GP's fetches
static void
setupSkinVertexFormat(InstanceDataHeader *header)
{
	invalidateBoundArrays();
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	if(header->hasNormals){
		GX_SetVtxDesc(GX_VA_NRM, GX_DIRECT);
		GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0);
	}
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	if(header->hasTexCoords){
		GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
		GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_S16,
			header->texCoordFrac);
	}
}

static void
skinRenderCB(Atomic *atomic, InstanceDataHeader *header)
{
	Geometry *geo = atomic->geometry;
	Skin *skin = Skin::get(geo);
	if(skin == nil){
		defaultRenderCB(atomic, header);
		return;
	}

	V3d *srcPos = geo->morphTargets[0].vertices;
	if(srcPos == nil){
		defaultRenderCB(atomic, header);
		return;
	}
	V3d *srcNrm = geo->morphTargets[0].normals;
	bool32 useNormals = header->hasNormals && srcNrm != nil;

	buildSkinMatrices(atomic, skin);

	uint32 flags = geo->flags;
	lightingCB(atomic);
	setWorldMatrix(atomic->getFrame()->getLTM());
	setupSkinVertexFormat(header);

	InstanceData *inst = header->inst;
	int32 n = header->numMeshes;
	while(n--){
		Material *m = inst->material;
		setMaterial(flags, m->color, m->surfaceProps);
		setTexture(0, m->texture);
		rw::SetRenderState(VERTEXALPHA, inst->vertexAlpha || m->color.alpha != 0xFF);
		flushCache();

		uint16 *idx = header->indexBuffer + inst->offset;
		GX_Begin(header->primType, GX_VTXFMT0, (uint16)inst->numIndex);
		for(uint32 i = 0; i < inst->numIndex; i++){
			uint16 v = idx[i];
			V3d sv;
			skinVertex(&sv, &srcPos[v], &skin->indices[v*4], &skin->weights[v*4]);
			GX_Position3f32(sv.x, sv.y, sv.z);
			if(header->hasNormals){
				V3d sn = { 0.0f, 0.0f, 1.0f };
				if(useNormals)
					skinNormal(&sn, &srcNrm[v], &skin->indices[v*4],
						&skin->weights[v*4]);
				GX_Normal3f32(sn.x, sn.y, sn.z);
			}
			RGBA *c = &header->colors[v];
			GX_Color4u8(c->red, c->green, c->blue, c->alpha);
			if(header->hasTexCoords)
				GX_TexCoord2s16(header->texCoords[v*2], header->texCoords[v*2+1]);
		}
		GX_End();
		inst++;
	}
}

ObjPipeline*
makeSkinPipeline(void)
{
	ObjPipeline *pipe = ObjPipeline::create();
	pipe->instanceCB = defaultInstanceCB;
	pipe->uninstanceCB = defaultUninstanceCB;
	pipe->renderCB = skinRenderCB;
	pipe->pluginID = ID_SKIN;
	pipe->pluginData = 1;
	return pipe;
}

static void*
skinOpen(void *o, int32, int32)
{
	skinGlobals.pipelines[PLATFORM_GX] = makeSkinPipeline();
	return o;
}

static void*
skinClose(void *o, int32, int32)
{
	skinGlobals.pipelines[PLATFORM_GX] = nil;
	return o;
}

void
initSkin(void)
{
	Driver::registerPlugin(PLATFORM_GX, 0, ID_SKIN, skinOpen, skinClose);
}

}
}

#endif
