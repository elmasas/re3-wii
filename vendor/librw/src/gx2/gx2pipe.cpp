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

#ifdef __WIIU__

#include "rwgx2.h"
#include "rwgx2shader.h"

namespace rw {
namespace gx2 {


void
freeInstanceData(Geometry *geometry)
{
	if(geometry->instData == nil ||
	   geometry->instData->platform != PLATFORM_GX2)
		return;
	InstanceDataHeader *header = (InstanceDataHeader*)geometry->instData;
	geometry->instData = nil;
	free(header->indexBuffer);
	free(header->vertexBuffer);
	rwFree(header->inst);
	rwFree(header->attribDesc);
	rwFree(header);
}

void*
destroyNativeData(void *object, int32, int32)
{
	freeInstanceData((Geometry*)object);
	return object;
}

static InstanceDataHeader*
instanceMesh(rw::ObjPipeline *rwpipe, Geometry *geo)
{
	InstanceDataHeader *header = rwNewT(InstanceDataHeader, 1, MEMDUR_EVENT | ID_GEOMETRY);
	MeshHeader *meshh = geo->meshHeader;
	geo->instData = header;
	header->platform = PLATFORM_GX2;

	header->serialNumber = meshh->serialNum;
	header->numMeshes = meshh->numMeshes;
	header->primType = meshh->flags == 1 ? GX2_PRIMITIVE_MODE_TRIANGLE_STRIP : GX2_PRIMITIVE_MODE_TRIANGLES;
	header->totalNumVertex = geo->numVertices;
	header->totalNumIndex = meshh->totalIndices;
	header->inst = rwNewT(InstanceData, header->numMeshes, MEMDUR_EVENT | ID_GEOMETRY);

	header->indexBuffer = (uint16*) memalign(GX2_INDEX_BUFFER_ALIGNMENT, header->totalNumIndex * 2);

	uint16* indices = header->indexBuffer;
	InstanceData *inst = header->inst;
	Mesh *mesh = meshh->getMeshes();
	uint32 offset = 0;
	for(uint32 i = 0; i < header->numMeshes; i++){
		findMinVertAndNumVertices(mesh->indices, mesh->numIndices,
		                          &inst->minVert, (int32*) &inst->numVertices);
		assert(inst->minVert != 0xFFFFFFFF);
		inst->numIndex = mesh->numIndices;
		inst->material = mesh->material;
		inst->vertexAlpha = 0;
		inst->offset = offset;
		memcpy((uint8*)header->indexBuffer + inst->offset,
			mesh->indices, inst->numIndex*2);
		offset += inst->numIndex*2;
		mesh++;
		inst++;
	}
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, header->indexBuffer, header->totalNumIndex * 2);

	header->vertexBuffer = nil;
	header->numAttribs = 0;
	header->attribDesc = nil;

	return header;
}

static void
instance(rw::ObjPipeline *rwpipe, Atomic *atomic)
{
	ObjPipeline *pipe = (ObjPipeline*)rwpipe;
	Geometry *geo = atomic->geometry;
	// don't try to (re)instance native data
	if(geo->flags & Geometry::NATIVE)
		return;

	InstanceDataHeader *header = (InstanceDataHeader*)geo->instData;
	if(geo->instData){
		// Already have instanced data, so check if we have to reinstance
		assert(header->platform == PLATFORM_GX2);
		if(header->serialNumber != geo->meshHeader->serialNum){
			// Mesh changed, so reinstance everything
			freeInstanceData(geo);
		}
	}

	// no instance or complete reinstance
	if(geo->instData == nil){
		geo->instData = instanceMesh(rwpipe, geo);
		pipe->instanceCB(geo, (InstanceDataHeader*)geo->instData, 0);
	}else if(geo->lockedSinceInst)
		pipe->instanceCB(geo, (InstanceDataHeader*)geo->instData, 1);

	geo->lockedSinceInst = 0;
}

static void
uninstance(rw::ObjPipeline *rwpipe, Atomic *atomic)
{
	assert(0 && "can't uninstance");
}

static void
render(rw::ObjPipeline *rwpipe, Atomic *atomic)
{
	ObjPipeline *pipe = (ObjPipeline*)rwpipe;
	Geometry *geo = atomic->geometry;
	pipe->instance(atomic);
	assert(geo->instData != nil);
	assert(geo->instData->platform == PLATFORM_GX2);
	if(pipe->renderCB)
		pipe->renderCB(atomic, (InstanceDataHeader*)geo->instData);
}

void
ObjPipeline::init(void)
{
	this->rw::ObjPipeline::init(PLATFORM_GX2);
	this->impl.instance = gx2::instance;
	this->impl.uninstance = gx2::uninstance;
	this->impl.render = gx2::render;
	this->instanceCB = nil;
	this->uninstanceCB = nil;
	this->renderCB = nil;
}

ObjPipeline*
ObjPipeline::create(void)
{
	ObjPipeline *pipe = rwNewT(ObjPipeline, 1, MEMDUR_GLOBAL);
	pipe->init();
	return pipe;
}

void
defaultInstanceCB(Geometry *geo, InstanceDataHeader *header, bool32 reinstance)
{
	AttribDesc *attribs, *a;

	bool isPrelit = !!(geo->flags & Geometry::PRELIT);
	bool hasNormals = !!(geo->flags & Geometry::NORMALS);

	if(!reinstance)
	{
		AttribDesc tmpAttribs[12];
		uint32 stride;

		a = tmpAttribs;
		stride = 0;

		// pos
		a->index = ATTRIB_POS;
		a->size = 3;
		a->offset = stride;
		stride += a->size*sizeof(float);
		a++;

		// normals
		a->index = ATTRIB_NORMAL;
		a->size = 3;
		a->offset = stride;
		stride += a->size*sizeof(float);
		a++;

		// color
		a->index = ATTRIB_COLOR;
		a->size = 4;
		a->offset = stride;
		stride += a->size*sizeof(float);
		a++;

		// tex coords
		a->index = ATTRIB_TEXCOORDS0;
		a->size = 2;
		a->offset = stride;
		stride += a->size*sizeof(float);
		a++;

		header->numAttribs = a - tmpAttribs;
		for(a = tmpAttribs; a != &tmpAttribs[header->numAttribs]; a++)
			a->stride = stride;
		
		header->attribDesc = rwNewT(AttribDesc, header->numAttribs, MEMDUR_EVENT | ID_GEOMETRY);
		memcpy(header->attribDesc, tmpAttribs, header->numAttribs*sizeof(AttribDesc));

		header->vertexBuffer = (float*) memalign(GX2_VERTEX_BUFFER_ALIGNMENT, header->totalNumVertex * stride);
	}

	attribs = header->attribDesc;

	//
	// Fill vertex buffer
	//

	uint8* verts = (uint8*) header->vertexBuffer;

	// Positions
	if(!reinstance || geo->lockedSinceInst&Geometry::LOCKVERTICES) {
		for(a = attribs; a->index != ATTRIB_POS; a++);

		instV3d(VERT_FLOAT3, verts + a->offset, geo->morphTargets[0].vertices,
			header->totalNumVertex, a->stride);
	}

	// Normals
	if (!reinstance || geo->lockedSinceInst&Geometry::LOCKNORMALS) {
		if(hasNormals) {
			for(a = attribs; a->index != ATTRIB_NORMAL; a++);

			instV3d(VERT_FLOAT3, verts + a->offset, geo->morphTargets[0].normals,
				header->totalNumVertex, a->stride);
		}
		else {
			for(a = attribs; a->index != ATTRIB_NORMAL; a++);

			float* f_verts = (float*)(verts + a->offset);
			for (uint32 i = 0; i < header->totalNumVertex; i++) {
				f_verts[0] = 0.0f;
				f_verts[1] = 0.0f;
				f_verts[2] = 1.0f;
				f_verts += a->stride/sizeof(float);
			}
		}
	}

	// Prelighting
	if (!reinstance || geo->lockedSinceInst&Geometry::LOCKPRELIGHT) {
		if(isPrelit) {
			for(a = attribs; a->index != ATTRIB_COLOR; a++);

			int n = header->numMeshes;
			InstanceData *inst = header->inst;
			while(n--){
				assert(inst->minVert != 0xFFFFFFFF);
				inst->vertexAlpha = instColorFloat(VERT_RGBA,
					verts + a->offset + a->stride*inst->minVert,
					geo->colors + inst->minVert,
					inst->numVertices, a->stride);
				inst++;
			}
		}
		else {
			for(a = attribs; a->index != ATTRIB_COLOR; a++);

			float* f_verts = (float*)(verts + a->offset);
			for (uint32 i = 0; i < header->totalNumVertex; i++) {
				f_verts[0] = 0.0f;
				f_verts[1] = 0.0f;
				f_verts[2] = 0.0f;
				f_verts[3] = 1.0f;
				f_verts += a->stride/sizeof(float);
			}
		}
	}

	// Texture coordinates
	if(!reinstance || geo->lockedSinceInst&(Geometry::LOCKTEXCOORDS)) {
		if (geo->numTexCoordSets > 0) {
			for(a = attribs; a->index != ATTRIB_TEXCOORDS0; a++);

			instTexCoords(VERT_FLOAT2, verts + a->offset, geo->texCoords[0],
				header->totalNumVertex, a->stride);
		}
		else {
			for(a = attribs; a->index != ATTRIB_TEXCOORDS0; a++);

			float* f_verts = (float*)(verts + a->offset);
			for (uint32 i = 0; i < header->totalNumVertex; i++) {
				f_verts[0] = 0.0f;
				f_verts[1] = 1.0f;
				f_verts += a->stride/sizeof(float);
			}
		}
	}

	GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, verts, header->totalNumVertex * attribs->stride);
}

void
defaultUninstanceCB(Geometry *geo, InstanceDataHeader *header)
{
	assert(0 && "can't uninstance");
}

ObjPipeline*
makeDefaultPipeline(void)
{
	ObjPipeline *pipe = ObjPipeline::create();
	pipe->instanceCB = defaultInstanceCB;
	pipe->uninstanceCB = defaultUninstanceCB;
	pipe->renderCB = defaultRenderCB;
	return pipe;
}

}
}

#endif