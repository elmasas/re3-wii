#ifdef __WIIU__

#include <whb/gfx.h>

namespace rw {
namespace gx2 {

GX2SamplerVar *GX2GetPixelSamplerVar(const GX2PixelShader *shader, const char *name);
int32 GX2GetPixelSamplerVarLocation(const GX2PixelShader *shader, const char *name);
int32 GX2GetPixelUniformVarOffset(const GX2PixelShader *shader, const char *name);
int32 GX2GetVertexUniformVarOffset(const GX2VertexShader *shader, const char *name);

struct UniformLocation
{
	int32 pixelLocation;
	int32 vertexLocation;
};

#define MAX_UNIFORMS 40

enum UniformType
{
	UNIFORM_NA,	// managed by the user
	UNIFORM_VEC4,
	UNIFORM_IVEC4,
	UNIFORM_MAT4
};

struct Uniform
{
	char* name;
	UniformType type;
	uint32 serialNum;
	int32 num;
	void* data;
};

struct UniformRegistry
{
	int32 numUniforms;
	Uniform uniforms[MAX_UNIFORMS];
};

int32 registerUniform(const char *name, UniformType type = UNIFORM_NA, int32 num = 1);
int32 findUniform(const char *name);
void setUniform(int32 id, const void* data);
void flushUniforms(void);

struct Shader
{
	WHBGfxShaderGroup group;
	GX2ShaderMode mode;

	int32 samplerLocation;
	// if a shader has a second sampler this can be used
	int32 sampler2Location;

	UniformLocation* uniformLocations;
	uint32* serialNums;

	int32 numUniforms;

	static Shader *create(const void* data, GX2ShaderMode m);
	bool initAttribute(const char* name, uint32 offset, GX2AttribFormat format);
	bool init(void);
	void use(void);
	void destroy(void);
};

extern Shader *currentShader;

}
}

#endif
