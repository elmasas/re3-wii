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

#ifdef __WII__



#include "rwgx.h"

namespace rw {
namespace gx {

// the GP fetches these by DMA, so they are 32 byte aligned and flushed
#define GXARRAY_ALIGNMENT 32

static uint32
padSize(uint32 size)
{
	return (size + GXARRAY_ALIGNMENT-1) & ~(GXARRAY_ALIGNMENT-1);
}

static void*
allocArray(uint32 size)
{
	return memalign(GXARRAY_ALIGNMENT, padSize(size));
}

static void
flushArray(void *data, uint32 size)
{
	if(data)
		DCFlushRange(data, padSize(size));
}

void
freeInstanceData(Geometry *geometry)
{
	if(geometry->instData == nil ||
	   geometry->instData->platform != PLATFORM_GX)
		return;
	InstanceDataHeader *header = (InstanceDataHeader*)geometry->instData;
	geometry->instData = nil;
	invalidateBoundArrays();
	// they outlive the frame they were bound in
	deferFree(header->indexBuffer);
	deferFree(header->positions);
	deferFree(header->normals);
	deferFree(header->colors);
	deferFree(header->texCoords);
	rwFree(header->inst);
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
	header->platform = PLATFORM_GX;

	header->serialNumber = meshh->serialNum;
	header->numMeshes = meshh->numMeshes;
	header->primType = meshh->flags == 1 ? GX_TRIANGLESTRIP : GX_TRIANGLES;
	header->totalNumVertex = geo->numVertices;
	header->totalNumIndex = meshh->totalIndices;
	header->texCoordFrac = 0;
	header->hasNormals = 0;
	header->hasTexCoords = 0;
	header->positions = nil;
	header->normals = nil;
	header->colors = nil;
	header->texCoords = nil;
	header->inst = rwNewT(InstanceData, header->numMeshes, MEMDUR_EVENT | ID_GEOMETRY);

	header->indexBuffer = (uint16*)allocArray(header->totalNumIndex * 2);

	InstanceData *inst = header->inst;
	Mesh *mesh = meshh->getMeshes();
	uint32 offset = 0;
	for(uint32 i = 0; i < header->numMeshes; i++){
		findMinVertAndNumVertices(mesh->indices, mesh->numIndices,
		                          &inst->minVert, (int32*)&inst->numVertices);
		assert(inst->minVert != 0xFFFFFFFF);
		inst->numIndex = mesh->numIndices;
		inst->material = mesh->material;
		inst->vertexAlpha = 0;
		inst->offset = offset;
		memcpy(header->indexBuffer + offset, mesh->indices, inst->numIndex*2);
		offset += inst->numIndex;
		mesh++;
		inst++;
	}

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
		assert(header->platform == PLATFORM_GX);
		if(header->serialNumber != geo->meshHeader->serialNum)
			freeInstanceData(geo);
	}

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
	if(geo->instData == nil || geo->instData->platform != PLATFORM_GX)
		return;
	InstanceDataHeader *header = (InstanceDataHeader*)geo->instData;
	if(header->numMeshes == 0 || pipe->renderCB == nil)
		return;
	pipe->renderCB(atomic, header);
}

void
ObjPipeline::init(void)
{
	this->rw::ObjPipeline::init(PLATFORM_GX);
	this->impl.instance = gx::instance;
	this->impl.uninstance = gx::uninstance;
	this->impl.render = gx::render;
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

// the biggest exponent that keeps this geometry's widest UV inside an S16
static uint8
pickTexCoordFrac(TexCoords *texCoords, uint32 numVertices)
{
	float32 widest = 1.0f;
	for(uint32 i = 0; i < numVertices; i++){
		float32 u = texCoords[i].u < 0.0f ? -texCoords[i].u : texCoords[i].u;
		float32 v = texCoords[i].v < 0.0f ? -texCoords[i].v : texCoords[i].v;
		if(u > widest) widest = u;
		if(v > widest) widest = v;
	}
	uint8 frac = 15;
	while(frac > 0 && widest * (float32)(1 << frac) > 32767.0f)
		frac--;
	return frac;
}

void
defaultInstanceCB(Geometry *geo, InstanceDataHeader *header, bool32 reinstance)
{
	bool isPrelit = !!(geo->flags & Geometry::PRELIT);
	bool hasNormals = !!(geo->flags & Geometry::NORMALS);

	// the binding points at the old arrays
	invalidateBoundArrays();

	// Positions
	if(!reinstance || geo->lockedSinceInst&Geometry::LOCKVERTICES){
		if(header->positions == nil)
			header->positions = (V3d*)allocArray(header->totalNumVertex*sizeof(V3d));
		memcpy(header->positions, geo->morphTargets[0].vertices,
			header->totalNumVertex*sizeof(V3d));
		flushArray(header->positions, header->totalNumVertex*sizeof(V3d));
	}

	// Normals
	if(!reinstance || geo->lockedSinceInst&Geometry::LOCKNORMALS){
		if(hasNormals && geo->morphTargets[0].normals){
			header->hasNormals = 1;
			if(header->normals == nil)
				header->normals = (int8*)allocArray(header->totalNumVertex*3);
			V3d *src = geo->morphTargets[0].normals;
			int8 *dst = header->normals;
			float32 scale = (float32)(1 << NORMAL_FRAC);
			for(uint32 i = 0; i < header->totalNumVertex; i++){
				int32 x = (int32)(src[i].x*scale);
				int32 y = (int32)(src[i].y*scale);
				int32 z = (int32)(src[i].z*scale);
				dst[0] = (int8)(x < -128 ? -128 : x > 127 ? 127 : x);
				dst[1] = (int8)(y < -128 ? -128 : y > 127 ? 127 : y);
				dst[2] = (int8)(z < -128 ? -128 : z > 127 ? 127 : z);
				dst += 3;
			}
			flushArray(header->normals, header->totalNumVertex*3);
		}
	}

	// Prelighting
	if(!reinstance || geo->lockedSinceInst&Geometry::LOCKPRELIGHT){
		if(header->colors == nil)
			header->colors = (RGBA*)allocArray(header->totalNumVertex*sizeof(RGBA));
		if(isPrelit && geo->colors)
			memcpy(header->colors, geo->colors, header->totalNumVertex*sizeof(RGBA));
		else{
			// no prelight means the lights supply all of it
			RGBA black = { 0, 0, 0, 255 };
			for(uint32 i = 0; i < header->totalNumVertex; i++)
				header->colors[i] = black;
		}

		InstanceData *inst = header->inst;
		for(uint32 i = 0; i < header->numMeshes; i++, inst++){
			inst->vertexAlpha = 0;
			for(uint32 j = 0; j < inst->numVertices; j++)
				if(header->colors[inst->minVert + j].alpha != 255){
					inst->vertexAlpha = 1;
					break;
				}
		}
		flushArray(header->colors, header->totalNumVertex*sizeof(RGBA));
	}

	// Texture coordinates
	if(!reinstance || geo->lockedSinceInst&Geometry::LOCKTEXCOORDS){
		if(geo->numTexCoordSets > 0 && geo->texCoords[0]){
			header->hasTexCoords = 1;
			if(header->texCoords == nil)
				header->texCoords = (int16*)allocArray(header->totalNumVertex*2*sizeof(int16));
			TexCoords *src = geo->texCoords[0];
			header->texCoordFrac = pickTexCoordFrac(src, header->totalNumVertex);
			float32 scale = (float32)(1 << header->texCoordFrac);
			int16 *dst = header->texCoords;
			for(uint32 i = 0; i < header->totalNumVertex; i++){
				int32 u = (int32)(src[i].u*scale);
				int32 v = (int32)(src[i].v*scale);
				dst[0] = (int16)(u < -32768 ? -32768 : u > 32767 ? 32767 : u);
				dst[1] = (int16)(v < -32768 ? -32768 : v > 32767 ? 32767 : v);
				dst += 2;
			}
			flushArray(header->texCoords, header->totalNumVertex*2*sizeof(int16));
		}
	}
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
