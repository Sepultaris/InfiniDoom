#version 450

layout(set = 0, binding = 0) uniform sampler2D SourceImage;
layout(set = 0, binding = 1) uniform sampler2D PaletteImage;

layout(location = 0) in vec2 TexCoord;
layout(location = 0) out vec4 FragColor;

void main()
{
	float source = texture(SourceImage, TexCoord).r;
	int index = int(source * 255.0 + 0.5);
	FragColor = texelFetch(PaletteImage, ivec2(index, 0), 0);
}
