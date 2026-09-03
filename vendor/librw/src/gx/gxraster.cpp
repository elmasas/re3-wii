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

#define PLUGIN_ID ID_RASTERGX

#ifdef __WII__


#include "rwgx.h"


namespace rw {
namespace gx {

int32 nativeRasterOffset;

#define TILE_W 4
#define TILE_H 4

static uint32
padUp(uint32 v, uint32 to)
{
	return (v + to-1) & ~(to-1);
}

uint32
texelsSizeForLevel(int32 width, int32 height, uint8 format)
{
	if(format == GX_TF_CMPR)
		return (padUp(width, 8)/8) * (padUp(height, 8)/8) * 32;
	return (padUp(width, TILE_W)/TILE_W) * (padUp(height, TILE_H)/TILE_H) * 32;
}

uint32
texelsSize(int32 width, int32 height, uint8 format, int32 numLevels)
{
	uint32 total = 0;
	for(int32 i = 0; i < numLevels; i++){
		total += texelsSizeForLevel(width, height, format);
		width = width > 1 ? width/2 : 1;
		height = height > 1 ? height/2 : 1;
	}
	return padUp(total, 32);
}

static uint16
to565(uint8 r, uint8 g, uint8 b)
{
	return (uint16)(((r>>3)<<11) | ((g>>2)<<5) | (b>>3));
}

// one DXT1 block: bounding box endpoints, then nearest of the 4 ramp colours
static void
compressBlock(uint8 *dst, const uint8 *src, int32 stride, int32 bw, int32 bh)
{
	uint8 lo[3] = { 255, 255, 255 };
	uint8 hi[3] = { 0, 0, 0 };
	bool32 hasAlpha = 0;
	int32 x, y, c;

	for(y = 0; y < bh; y++){
		const uint8 *px = src + y*stride;
		for(x = 0; x < bw; x++, px += 4){
			if(px[3] < 0x80){
				hasAlpha = 1;
				continue;
			}
			for(c = 0; c < 3; c++){
				if(px[c] < lo[c]) lo[c] = px[c];
				if(px[c] > hi[c]) hi[c] = px[c];
			}
		}
	}
	if(lo[0] > hi[0]){	// every pixel was transparent
		lo[0] = lo[1] = lo[2] = 0;
		hi[0] = hi[1] = hi[2] = 0;
	}

	uint16 c0 = to565(hi[0], hi[1], hi[2]);
	uint16 c1 = to565(lo[0], lo[1], lo[2]);

	// c0 > c1 selects the opaque 4 colour ramp, c0 <= c1 the 3 colour + transparent one
	if(hasAlpha){
		if(c0 > c1){ uint16 t = c0; c0 = c1; c1 = t; }
	}else{
		if(c0 <= c1){
			if(c1 == 0xFFFF) c0 = 0xFFFE;
			else { uint16 t = c0; c0 = c1 + 1; c1 = t; }
			if(c0 <= c1) c0 = c1 + 1;
		}
	}

	uint8 base[2][3];
	base[0][0] = ((c0>>11)&0x1F)<<3; base[0][1] = ((c0>>5)&0x3F)<<2; base[0][2] = (c0&0x1F)<<3;
	base[1][0] = ((c1>>11)&0x1F)<<3; base[1][1] = ((c1>>5)&0x3F)<<2; base[1][2] = (c1&0x1F)<<3;

	dst[0] = c0>>8; dst[1] = c0&0xFF;
	dst[2] = c1>>8; dst[3] = c1&0xFF;

	// compare the projection against fixed thresholds, no divide and no 64 bit math
	int32 ar = base[0][0] - base[1][0];
	int32 ag = base[0][1] - base[1][1];
	int32 ab = base[0][2] - base[1][2];
	int32 len2 = ar*ar + ag*ag + ab*ab;
	bool32 threeCol = c0 <= c1;
	int32 t1, t2, t3;
	if(threeCol){
		t1 = len2/4; t2 = (len2*3)/4; t3 = 0;
	}else{
		t1 = len2/6; t2 = len2/2; t3 = (len2*5)/6;
	}

	for(y = 0; y < 4; y++){
		uint8 bits = 0;
		for(x = 0; x < 4; x++){
			int32 sel = 0;
			if(x < bw && y < bh){
				const uint8 *px = src + y*stride + x*4;
				if(threeCol && px[3] < 0x80)
					sel = 3;
				else if(len2 == 0)
					sel = 0;
				else{
					int32 dot = ((int32)px[0] - base[1][0])*ar +
					            ((int32)px[1] - base[1][1])*ag +
					            ((int32)px[2] - base[1][2])*ab;
					if(threeCol)
						sel = dot < t1 ? 1 : dot < t2 ? 2 : 0;
					else
						sel = dot < t1 ? 1 : dot < t2 ? 3 : dot < t3 ? 2 : 0;
				}
			}
			bits |= sel << (6 - x*2);
		}
		dst[4+y] = bits;
	}
}

// CMPR: 8x8 tiles built from four 4x4 DXT1 blocks
static void
tileCMPR(const uint8 *src, int32 stride, int32 width, int32 height, uint8 *dst)
{
	static const int32 subX[4] = { 0, 4, 0, 4 };
	static const int32 subY[4] = { 0, 0, 4, 4 };
	int32 tilesPerRow = padUp(width, 8)/8;
	int32 tileRows = padUp(height, 8)/8;

	for(int32 ty = 0; ty < tileRows; ty++)
		for(int32 tx = 0; tx < tilesPerRow; tx++){
			uint8 *tile = dst + (ty*tilesPerRow + tx)*32;
			for(int32 s = 0; s < 4; s++){
				int32 px = tx*8 + subX[s];
				int32 py = ty*8 + subY[s];
				int32 bw = width - px;
				int32 bh = height - py;
				if(bw > 4) bw = 4;
				if(bh > 4) bh = 4;
				if(bw <= 0 || bh <= 0){
					memset(tile + s*8, 0, 8);
					continue;
				}
				compressBlock(tile + s*8, src + py*stride + px*4,
					stride, bw, bh);
			}
		}
}

// RGB5A3: top bit set means RGB555, clear means ARGB3444
static void
tileRGB5A3(const uint8 *src, int32 stride, int32 width, int32 height, uint8 *dst)
{
	int32 tilesPerRow = padUp(width, TILE_W)/TILE_W;
	int32 tileRows = padUp(height, TILE_H)/TILE_H;

	for(int32 ty = 0; ty < tileRows; ty++)
		for(int32 tx = 0; tx < tilesPerRow; tx++){
			uint8 *tile = dst + (ty*tilesPerRow + tx)*32;
			for(int32 y = 0; y < TILE_H; y++)
				for(int32 x = 0; x < TILE_W; x++){
					int32 px = tx*TILE_W + x;
					int32 py = ty*TILE_H + y;
					uint16 v = 0;
					if(px < width && py < height){
						const uint8 *pix = src + py*stride + px*4;
						if(pix[3] >= 0xE0)
							v = 0x8000 | ((pix[0]>>3)<<10) | ((pix[1]>>3)<<5) | (pix[2]>>3);
						else
							v = ((pix[3]>>5)<<12) | ((pix[0]>>4)<<8) | ((pix[1]>>4)<<4) | (pix[2]>>4);
					}
					uint8 *p = tile + (y*TILE_W + x)*2;
					p[0] = (uint8)(v >> 8);
					p[1] = (uint8)v;
				}
		}
}

void
tileTexels(const uint8 *src, int32 stride, int32 width, int32 height,
	uint8 *dst, uint8 format)
{
	if(format == GX_TF_CMPR)
		tileCMPR(src, stride, width, height, dst);
	else
		tileRGB5A3(src, stride, width, height, dst);
}

// CMPR is DXT1 with big endian endpoints and the index bits of each row reversed
static uint8 dxtIndexSwap[256];
static bool32 dxtIndexSwapBuilt;

static void
buildDxtIndexSwap(void)
{
	for(int32 i = 0; i < 256; i++)
		dxtIndexSwap[i] = (uint8)(((i&3)<<6) | (((i>>2)&3)<<4) |
			(((i>>4)&3)<<2) | ((i>>6)&3));
	dxtIndexSwapBuilt = 1;
}

static void
convertDxt1Block(uint8 *dst, const uint8 *src)
{
	dst[0] = src[1];
	dst[1] = src[0];
	dst[2] = src[3];
	dst[3] = src[2];
	dst[4] = dxtIndexSwap[src[4]];
	dst[5] = dxtIndexSwap[src[5]];
	dst[6] = dxtIndexSwap[src[6]];
	dst[7] = dxtIndexSwap[src[7]];
}

bool32
rasterFromDxt1(Raster *raster, const uint8 *src, uint32 srcSize, bool32 hasAlpha)
{
	static const int32 subX[4] = { 0, 4, 0, 4 };
	static const int32 subY[4] = { 0, 0, 4, 4 };

	if(!dxtIndexSwapBuilt)
		buildDxtIndexSwap();

	int32 blocksPerRow = (raster->width + 3)/4;
	int32 blockRows = (raster->height + 3)/4;
	if(srcSize < (uint32)(blocksPerRow*blockRows*8))
		return 0;

	GXRaster *natras = PLUGINOFFSET(GXRaster, raster, nativeRasterOffset);
	uint32 size = texelsSize(raster->width, raster->height, GX_TF_CMPR, 1);
	uint8 *texels = (uint8*)memalign(32, size);
	if(texels == nil)
		return 0;

	int32 tilesPerRow = padUp(raster->width, 8)/8;
	int32 tileRows = padUp(raster->height, 8)/8;
	for(int32 ty = 0; ty < tileRows; ty++)
		for(int32 tx = 0; tx < tilesPerRow; tx++){
			uint8 *tile = texels + (ty*tilesPerRow + tx)*32;
			for(int32 s = 0; s < 4; s++){
				int32 bx = tx*2 + subX[s]/4;
				int32 by = ty*2 + subY[s]/4;
				if(bx >= blocksPerRow || by >= blockRows){
					memset(tile + s*8, 0, 8);
					continue;
				}
				convertDxt1Block(tile + s*8,
					src + (by*blocksPerRow + bx)*8);
			}
		}

	if(natras->texels){
		deferFree(natras->texels);
	}
	natras->texels = texels;
	natras->texelsSize = size;
	natras->format = GX_TF_CMPR;
	natras->hasAlpha = hasAlpha;
	natras->numLevels = 1;

	raster->depth = 32;
	raster->stride = raster->width*4;
	DCFlushRange(texels, size);
	GX_InitTexObj(&natras->tex, texels, (uint16)raster->width,
		(uint16)raster->height, GX_TF_CMPR, GX_REPEAT, GX_REPEAT, GX_FALSE);
	natras->ready = 1;
	updateSampler(natras);
	return 1;
}

void
updateSampler(GXRaster *natras)
{
	if(!natras->ready)
		return;
	uint8 wrapS = natras->addressU == Texture::CLAMP ? GX_CLAMP :
		natras->addressU == Texture::MIRROR ? GX_MIRROR : GX_REPEAT;
	uint8 wrapT = natras->addressV == Texture::CLAMP ? GX_CLAMP :
		natras->addressV == Texture::MIRROR ? GX_MIRROR : GX_REPEAT;
	GX_InitTexObjWrapMode(&natras->tex, wrapS, wrapT);

	uint8 minf, magf;
	switch(natras->filterMode){
	case Texture::NEAREST:		minf = GX_NEAR; magf = GX_NEAR; break;
	case Texture::MIPNEAREST:	minf = GX_NEAR_MIP_NEAR; magf = GX_NEAR; break;
	case Texture::LINEARMIPNEAREST:	minf = GX_LIN_MIP_NEAR; magf = GX_LINEAR; break;
	case Texture::MIPLINEAR:	minf = GX_NEAR_MIP_LIN; magf = GX_NEAR; break;
	case Texture::LINEARMIPLINEAR:	minf = GX_LIN_MIP_LIN; magf = GX_LINEAR; break;
	default:			minf = GX_LINEAR; magf = GX_LINEAR; break;
	}
	GX_InitTexObjFilterMode(&natras->tex, minf, magf);
}

// CMPR only carries a 1 bit alpha, anything softer needs RGB5A3
static void
scanAlpha(const uint8 *pixels, int32 stride, int32 width, int32 height,
	bool32 *hasAlpha, bool32 *needsRGB5A3)
{
	*hasAlpha = 0;
	*needsRGB5A3 = 0;
	for(int32 y = 0; y < height; y++){
		const uint8 *px = pixels + y*stride;
		for(int32 x = 0; x < width; x++, px += 4){
			uint8 a = px[3];
			if(a == 0xFF)
				continue;
			*hasAlpha = 1;
			if(a > 0x10 && a < 0xE0){
				*needsRGB5A3 = 1;
				return;
			}
		}
	}
}

// the format needs the actual pixels to decide, so the texels land here
static void
uploadTexelsFrom(Raster *raster, GXRaster *natras, const uint8 *pixels, int32 stride)
{
	if(pixels == nil)
		return;

	bool32 hasAlpha, soft;
	scanAlpha(pixels, stride, raster->width, raster->height,
		&hasAlpha, &soft);
	natras->hasAlpha = hasAlpha;
	uint8 format = soft ? GX_TF_RGB5A3 : GX_TF_CMPR;

	uint32 size = texelsSize(raster->width, raster->height, format, 1);
	if(natras->texels == nil || natras->texelsSize != size ||
	   natras->format != format){
		if(natras->texels){
			deferFree(natras->texels);
		}
		natras->texels = (uint8*)memalign(32, size);
		natras->texelsSize = size;
		natras->format = format;
		if(natras->texels == nil){
			natras->texelsSize = 0;
			natras->ready = 0;
			return;
		}
	}

	tileTexels(pixels, stride, raster->width, raster->height,
		natras->texels, format);
	DCFlushRange(natras->texels, natras->texelsSize);

	GX_InitTexObj(&natras->tex, natras->texels, (uint16)raster->width,
		(uint16)raster->height, format, GX_REPEAT, GX_REPEAT, GX_FALSE);
	natras->ready = 1;
	updateSampler(natras);
}

static void
uploadTexels(Raster *raster, GXRaster *natras)
{
	uploadTexelsFrom(raster, natras, natras->pixels, raster->stride);
}

Raster*
rasterCreate(Raster *raster)
{
	GXRaster *natras = PLUGINOFFSET(GXRaster, raster, nativeRasterOffset);

	if(raster->type != Raster::TEXTURE && raster->type != Raster::CAMERATEXTURE){
		raster->flags |= Raster::DONTALLOCATE;
		raster->stride = 0;
		raster->pixels = nil;
		return raster;
	}
	if(raster->flags & Raster::DONTALLOCATE)
		return raster;
	if(raster->width == 0 || raster->height == 0){
		raster->flags |= Raster::DONTALLOCATE;
		return raster;
	}

	raster->depth = 32;
	raster->stride = raster->width*4;
	raster->pixels = nil;
	natras->numLevels = 1;
	natras->hasAlpha = raster->format & Raster::C8888 ? 1 : 0;
	return raster;
}

uint8*
rasterLock(Raster *raster, int32 level, int32 lockMode)
{
	GXRaster *natras = PLUGINOFFSET(GXRaster, raster, nativeRasterOffset);
	if(level != 0)
		return nil;
	if(natras->pixels == nil)
		natras->pixels = (uint8*)rwNewT(uint8, raster->stride*raster->height,
			MEMDUR_EVENT | ID_RASTERGX);
	raster->pixels = natras->pixels;
	raster->privateFlags = lockMode;
	return natras->pixels;
}

void
rasterUnlock(Raster *raster, int32 level)
{
	GXRaster *natras = PLUGINOFFSET(GXRaster, raster, nativeRasterOffset);
	if(natras->pixels == nil)
		return;
	if(raster->privateFlags & Raster::LOCKWRITE)
		uploadTexels(raster, natras);
	// the staging copy is only needed while locked
	rwFree(natras->pixels);
	natras->pixels = nil;
	raster->pixels = nil;
	raster->privateFlags = 0;
}

uint8*
rasterLockPalette(Raster *raster, int32 lockMode)
{
	return nil;
}

void
rasterUnlockPalette(Raster *raster)
{
}

int32
rasterNumLevels(Raster *raster)
{
	GXRaster *natras = PLUGINOFFSET(GXRaster, raster, nativeRasterOffset);
	return natras->numLevels;
}

bool32
imageFindRasterFormat(Image *image, int32 type, int32 *pWidth, int32 *pHeight,
	int32 *pDepth, int32 *pFormat)
{
	assert(type == Raster::TEXTURE);
	*pWidth = image->width;
	*pHeight = image->height;
	*pDepth = 32;
	*pFormat = (image->hasAlpha() ? Raster::C8888 : Raster::C888) | type;
	return 1;
}

bool32
rasterFromImage(Raster *raster, Image *image)
{
	if(image->width != raster->width || image->height != raster->height)
		return 0;

	if(image->depth != 32)
		image->convertTo32();

	GXRaster *natras = PLUGINOFFSET(GXRaster, raster, nativeRasterOffset);
	// the caller may have staged a buffer, the image is the source either way
	if(natras->pixels){
		rwFree(natras->pixels);
		natras->pixels = nil;
		raster->pixels = nil;
	}
	uploadTexelsFrom(raster, natras, image->pixels, image->stride);
	raster->privateFlags = 0;
	return 1;
}

Image*
rasterToImage(Raster *raster)
{
	Image *image = Image::create(raster->width, raster->height, 32);
	image->allocate();
	memset(image->pixels, 0, image->stride*image->height);
	return image;
}

// texels are stored already tiled, so loading one back costs nothing but the read
#define GXNATIVE_HEADER 96

uint32
getSizeNativeTexture(Texture *tex)
{
	GXRaster *natras = PLUGINOFFSET(GXRaster, tex->raster, nativeRasterOffset);
	return GXNATIVE_HEADER + natras->texelsSize;
}

void
writeNativeTexture(Texture *tex, Stream *stream)
{
	Raster *raster = tex->raster;
	GXRaster *natras = PLUGINOFFSET(GXRaster, raster, nativeRasterOffset);

	writeChunkHeader(stream, ID_STRUCT, getSizeNativeTexture(tex)-12);
	stream->writeU32(PLATFORM_GX);

	stream->writeU32(tex->filterAddressing);
	stream->write8(tex->name, 32);
	stream->write8(tex->mask, 32);

	stream->writeU16((uint16)raster->width);
	stream->writeU16((uint16)raster->height);
	stream->writeU8(natras->format);
	stream->writeU8((uint8)natras->hasAlpha);
	stream->writeU8((uint8)natras->numLevels);
	stream->writeU8((uint8)raster->type);
	stream->writeU32(natras->texelsSize);
	stream->write8(natras->texels, natras->texelsSize);
}

Texture*
readNativeTexture(Stream *stream)
{
	if(!findChunk(stream, ID_STRUCT, nil, nil)){
		RWERROR((ERR_CHUNK, "STRUCT"));
		return nil;
	}
	if(stream->readU32() != PLATFORM_GX){
		RWERROR((ERR_PLATFORM, PLATFORM_GX));
		return nil;
	}
	Texture *tex = Texture::create(nil);
	if(tex == nil)
		return nil;

	tex->filterAddressing = stream->readU32();
	stream->read8(tex->name, 32);
	stream->read8(tex->mask, 32);

	int32 width = stream->readU16();
	int32 height = stream->readU16();
	uint8 format = stream->readU8();
	uint8 hasAlpha = stream->readU8();
	uint8 numLevels = stream->readU8();
	uint8 type = stream->readU8();
	uint32 size = stream->readU32();

	Raster *raster = Raster::create(width, height, 32,
		type | Raster::DONTALLOCATE, PLATFORM_GX);
	tex->raster = raster;

	GXRaster *natras = PLUGINOFFSET(GXRaster, raster, nativeRasterOffset);
	natras->texels = (uint8*)memalign(32, size);
	if(natras->texels == nil){
		stream->seek(size);
		return tex;
	}
	stream->read8(natras->texels, size);

	natras->texelsSize = size;
	natras->format = format;
	natras->hasAlpha = hasAlpha;
	natras->numLevels = numLevels;

	raster->depth = 32;
	raster->stride = width*4;
	DCFlushRange(natras->texels, size);
	GX_InitTexObj(&natras->tex, natras->texels, (uint16)width, (uint16)height,
		format, GX_REPEAT, GX_REPEAT, GX_FALSE);
	natras->ready = 1;
	updateSampler(natras);
	return tex;
}

static void*
createNativeRaster(void *object, int32 offset, int32)
{
	GXRaster *natras = PLUGINOFFSET(GXRaster, object, offset);
	memset(natras, 0, sizeof(GXRaster));
	natras->filterMode = Texture::LINEAR;
	natras->addressU = Texture::WRAP;
	natras->addressV = Texture::WRAP;
	natras->numLevels = 1;
	return object;
}

static void*
destroyNativeRaster(void *object, int32 offset, int32)
{
	GXRaster *natras = PLUGINOFFSET(GXRaster, object, offset);
	evictRaster((Raster*)object);
	if(natras->texels){
		deferFree(natras->texels);
		natras->texels = nil;
		natras->texelsSize = 0;
	}
	if(natras->pixels){
		rwFree(natras->pixels);
		natras->pixels = nil;
	}
	natras->ready = 0;
	return object;
}

static void*
copyNativeRaster(void *dst, void *, int32 offset, int32)
{
	GXRaster *natras = PLUGINOFFSET(GXRaster, dst, offset);
	memset(natras, 0, sizeof(GXRaster));
	natras->filterMode = Texture::LINEAR;
	natras->addressU = Texture::WRAP;
	natras->addressV = Texture::WRAP;
	natras->numLevels = 1;
	return dst;
}

void
registerNativeRaster(void)
{
	nativeRasterOffset = Raster::registerPlugin(sizeof(GXRaster),
		ID_RASTERGX, createNativeRaster, destroyNativeRaster, copyNativeRaster);
}

}
}

#endif
