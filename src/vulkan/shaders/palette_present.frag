#version 450

layout(set = 0, binding = 0) uniform sampler2D SourceImage;
layout(set = 0, binding = 1) uniform sampler2D PaletteImage;

layout(push_constant) uniform PresentConstants
{
	vec2 uvOffset;
	vec2 uvScale;
	vec2 sourceOffset;
	vec2 sourceScale;
	vec4 borderColor;
	vec4 filterParams;
} pc;

layout(location = 0) in vec2 TexCoord;
layout(location = 0) out vec4 FragColor;

vec4 paletteColorAt(ivec2 pixel, ivec2 sourceSize)
{
	ivec2 clampedPixel = clamp(pixel, ivec2(0), sourceSize - ivec2(1));
	float source = texelFetch(SourceImage, clampedPixel, 0).r;
	int index = int(source * 255.0 + 0.5);
	return texelFetch(PaletteImage, ivec2(index, 0), 0);
}

vec4 sharpColorFilter(vec2 sourceCoord)
{
	ivec2 sourceSize = max(ivec2(pc.filterParams.yz + vec2(0.5)), ivec2(1));
	vec2 pixel = sourceCoord * vec2(sourceSize) - vec2(0.5);
	ivec2 basePixel = ivec2(floor(pixel));
	vec2 blend = clamp((fract(pixel) - vec2(0.25)) * 2.0, vec2(0.0), vec2(1.0));

	vec4 c00 = paletteColorAt(basePixel, sourceSize);
	vec4 c10 = paletteColorAt(basePixel + ivec2(1, 0), sourceSize);
	vec4 c01 = paletteColorAt(basePixel + ivec2(0, 1), sourceSize);
	vec4 c11 = paletteColorAt(basePixel + ivec2(1, 1), sourceSize);
	return mix(mix(c00, c10, blend.x), mix(c01, c11, blend.x), blend.y);
}

void main()
{
	vec2 sourceCoord = (TexCoord - pc.uvOffset) * pc.uvScale;
	if (sourceCoord.x < 0.0 || sourceCoord.y < 0.0 || sourceCoord.x > 1.0 || sourceCoord.y > 1.0)
	{
		FragColor = pc.borderColor;
		return;
	}

	sourceCoord = pc.sourceOffset + sourceCoord * pc.sourceScale;
	if (int(pc.filterParams.x + 0.5) == 2)
	{
		FragColor = sharpColorFilter(sourceCoord);
		return;
	}

	float source = texture(SourceImage, sourceCoord).r;
	int index = int(source * 255.0 + 0.5);
	FragColor = texelFetch(PaletteImage, ivec2(index, 0), 0);
}
