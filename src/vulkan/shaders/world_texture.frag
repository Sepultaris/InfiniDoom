#version 450

layout(set = 0, binding = 0) uniform sampler2D WallAtlas;

layout(location = 0) in vec2 TexCoord;
layout(location = 1) in vec3 LightColor;
layout(location = 0) out vec4 FragColor;

void main()
{
	vec4 texel = texture(WallAtlas, TexCoord);
	FragColor = vec4(texel.rgb * LightColor, texel.a);
}
