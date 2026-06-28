#version 450

layout(set = 0, binding = 0) uniform sampler2D WallAtlas;

layout(location = 0) in vec2 TexCoord;
layout(location = 1) in vec3 LightColor;
layout(location = 2) in vec4 AtlasRect;
layout(location = 0) out vec4 FragColor;

void main()
{
	vec2 repeatedCoord = fract(TexCoord);
	vec2 atlasCoord = mix(AtlasRect.xy, AtlasRect.zw, repeatedCoord);
	vec4 texel = texture(WallAtlas, atlasCoord);
	FragColor = vec4(texel.rgb * LightColor, texel.a);
}
