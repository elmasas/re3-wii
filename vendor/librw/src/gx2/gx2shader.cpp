#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>

#include "../rwbase.h"
#include "../rwerror.h"
#include "../rwplg.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "../rwengine.h"

#ifdef __WIIU__
#include "rwgx2.h"
#include "rwgx2shader.h"

#include <whb/log.h>

namespace rw {
namespace gx2 {

GX2SamplerVar *GX2GetPixelSamplerVar(const GX2PixelShader *shader, const char *name)
{
	for (uint32_t i = 0; i < shader->samplerVarCount; i++)
	{
	   if (strcmp(shader->samplerVars[i].name, name) == 0)
		   return &(shader->samplerVars[i]);
	}

	return NULL;
}

int32 GX2GetPixelSamplerVarLocation(const GX2PixelShader *shader, const char *name)
{
	GX2SamplerVar *sampler = GX2GetPixelSamplerVar(shader, name);
	return sampler ? sampler->location : -1;
}

int32 GX2GetPixelUniformVarOffset(const GX2PixelShader *shader, const char *name)
{
	GX2UniformVar *uniform = GX2GetPixelUniformVar(shader, name);
	return uniform ? uniform->offset : -1;
}

int32 GX2GetVertexUniformVarOffset(const GX2VertexShader *shader, const char *name)
{
	GX2UniformVar *uniform = GX2GetVertexUniformVar(shader, name);
	return uniform ? uniform->offset : -1;
}

UniformRegistry uniformRegistry;
static char nameBuffer[MAX_UNIFORMS * 32];
static uint32 nameBufPtr;
static float uniformData[512 * 4] __attribute__ ((aligned (0x40)));
static uint32 dataPtr;

static int uniformTypesize[] = {
	0, 4, 4, 16
};

static char*
shader_strdup(const char *name)
{
	size_t len = strlen(name)+1;
	char *s = &nameBuffer[nameBufPtr];
	nameBufPtr += len;
	assert(nameBufPtr <= nelem(nameBuffer));
	memcpy(s, name, len);
	return s;
}

int32
registerUniform(const char *name, UniformType type, int32 num)
{
	int i = findUniform(name);
	if(i >= 0){
		Uniform *u = &uniformRegistry.uniforms[i];
		assert(u->type == type);
		assert(u->num == num);
		return i;
	}

	if(uniformRegistry.numUniforms + 1 >= MAX_UNIFORMS)
	{
		assert(0 && "no space for uniform");
		return -1;
	}

	Uniform *u = &uniformRegistry.uniforms[uniformRegistry.numUniforms];
	u->name = shader_strdup(name);
	u->type = type;
	u->serialNum = 0;
	if(type == UNIFORM_NA) {
		u->num = 0;
		u->data = nil;
	}
	else {
		u->num = num;
		u->data = &uniformData[dataPtr];
		dataPtr += uniformTypesize[type] * num;
		assert(dataPtr <= nelem(uniformData));
	}
	return uniformRegistry.numUniforms++;
}

int32
findUniform(const char *name)
{
	for(int i = 0; i < uniformRegistry.numUniforms; i++)
	{
		if(strcmp(name, uniformRegistry.uniforms[i].name) == 0)
			return i;
	}
	return -1;
}

void 
setUniform(int32 id, const void* data)
{
	Uniform *u = &uniformRegistry.uniforms[id];
	assert(u->type != UNIFORM_NA);
	if(memcmp(u->data, data, uniformTypesize[u->type] * u->num * sizeof(float)) != 0){
		memcpy(u->data, data, uniformTypesize[u->type] * u->num * sizeof(float));
		u->serialNum++;
	}
}

void
flushUniforms(void)
{
	for(int i = 0; i < uniformRegistry.numUniforms; i++) {
		if(i >= currentShader->numUniforms) {
			WHBLogPrintf("Trying to set uniform %d %s that doesn't exist!", i, uniformRegistry.uniforms[i].name);
			continue;
		}

		UniformLocation loc = currentShader->uniformLocations[i];

		Uniform* u = &uniformRegistry.uniforms[i];
		if(currentShader->serialNums[i] != u->serialNum) {
			if (u->type != UNIFORM_NA) {
				if (loc.pixelLocation != -1)
					GX2SetPixelUniformReg(loc.pixelLocation, uniformTypesize[u->type] * u->num, u->data);
				if (loc.vertexLocation != -1)
					GX2SetVertexUniformReg(loc.vertexLocation, uniformTypesize[u->type] * u->num, u->data);
			}
		}
		currentShader->serialNums[i] = u->serialNum;
	}
}

Shader *currentShader;

Shader*
Shader::create(const void *data, GX2ShaderMode m)
{
	Shader *sh = rwNewT(Shader, 1, MEMDUR_EVENT | ID_DRIVER);
	sh->mode = m;
	sh->samplerLocation = -1;
	sh->sampler2Location = -1;
	sh->numUniforms = uniformRegistry.numUniforms;

	if (!WHBGfxLoadGFDShaderGroup(&sh->group, 0, data))
	{
		rwFree(sh);
		return nil;
	}

	return sh;
}

bool Shader::initAttribute(const char* name, uint32 offset, GX2AttribFormat format)
{
	if (!WHBGfxInitShaderAttribute(&group, name, 0, offset, format))
		return false;

	return true;
}

bool Shader::init(void)
{
	if (!WHBGfxInitFetchShader(&group))
		return false;

	uniformLocations = rwNewT(UniformLocation, uniformRegistry.numUniforms, MEMDUR_EVENT | ID_DRIVER);
	serialNums = rwNewT(uint32, uniformRegistry.numUniforms, MEMDUR_EVENT | ID_DRIVER);
	for(int i = 0; i < uniformRegistry.numUniforms; i++)
	{
		uniformLocations[i].pixelLocation = GX2GetPixelUniformVarOffset(group.pixelShader, uniformRegistry.uniforms[i].name);
		uniformLocations[i].vertexLocation = GX2GetVertexUniformVarOffset(group.vertexShader, uniformRegistry.uniforms[i].name);
		serialNums[i] = ~0;
	}

	return true;
}

void
Shader::use(void)
{
	if(currentShader != this)
	{
		GX2SetShaderMode(mode);
		GX2SetFetchShader(&group.fetchShader);
		GX2SetVertexShader(group.vertexShader);
		GX2SetPixelShader(group.pixelShader);
		currentShader = this;

		// invalidate uniforms
		for(int i = 0; i < uniformRegistry.numUniforms; i++)
		{
			serialNums[i] = ~0;
		}
	}
}

void
Shader::destroy(void)
{
	WHBGfxFreeShaderGroup(&group);
	rwFree(this->uniformLocations);
	rwFree(this->serialNums);
	rwFree(this);
}

}
}

#endif
