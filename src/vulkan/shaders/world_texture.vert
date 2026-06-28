#version 450

layout(location = 0) in vec3 InPosition;
layout(location = 1) in vec3 InLight;
layout(location = 2) in vec2 InTexCoord;
layout(location = 3) in vec4 InAtlasRect;

layout(location = 0) out vec2 TexCoord;
layout(location = 1) out vec3 LightColor;
layout(location = 2) out vec4 AtlasRect;

layout(push_constant) uniform ProbeConstants
{
	vec4 Row0;
	vec4 Row1;
	vec4 Row2;
	vec4 Row3;
} Probe;

void main()
{
	vec4 world = vec4(InPosition, 1.0);
	gl_Position = vec4(
		dot(Probe.Row0, world),
		dot(Probe.Row1, world),
		dot(Probe.Row2, world),
		dot(Probe.Row3, world));
	TexCoord = InTexCoord;
	LightColor = InLight;
	AtlasRect = InAtlasRect;
}
