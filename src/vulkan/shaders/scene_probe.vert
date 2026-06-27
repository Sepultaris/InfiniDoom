#version 450

layout(location = 0) out vec3 ProbeColor;

void main()
{
	vec2 positions[3] = vec2[](
		vec2(-0.85, -0.75),
		vec2(-0.35, -0.75),
		vec2(-0.60, -0.25)
	);
	vec3 colors[3] = vec3[](
		vec3(1.0, 0.15, 0.05),
		vec3(0.05, 0.9, 0.2),
		vec3(0.1, 0.35, 1.0)
	);

	gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
	ProbeColor = colors[gl_VertexIndex];
}
