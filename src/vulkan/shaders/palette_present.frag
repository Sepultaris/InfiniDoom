#version 450

layout(set = 0, binding = 0) uniform sampler2D SourceImage;
layout(set = 0, binding = 1) uniform sampler2D PaletteImage;

layout(push_constant) uniform PresentConstants
{
	vec2 uvOffset;
	vec2 uvScale;
	vec4 borderColor;
} pc;

layout(location = 0) in vec2 TexCoord;
layout(location = 0) out vec4 FragColor;

void main()
{
	vec2 sourceCoord = (TexCoord - pc.uvOffset) * pc.uvScale;
	if (sourceCoord.x < 0.0 || sourceCoord.y < 0.0 || sourceCoord.x > 1.0 || sourceCoord.y > 1.0)
	{
		FragColor = pc.borderColor;
		return;
	}

	float source = texture(SourceImage, sourceCoord).r;
	int index = int(source * 255.0 + 0.5);
	FragColor = texelFetch(PaletteImage, ivec2(index, 0), 0);
}
