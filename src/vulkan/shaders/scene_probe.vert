#version 450

layout(location = 0) out vec3 ProbeColor;

void main()
{
	vec2 positions[3] = vec2[](
		vec2(0.25, -0.45),
		vec2(0.95, -0.45),
		vec2(0.95,  0.25)
	);
	vec3 colors[3] = vec3[](
		vec3(1.0, 0.0, 1.0),
		vec3(0.0, 1.0, 1.0),
		vec3(1.0, 1.0, 0.0)
	);

	gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
	ProbeColor = colors[gl_VertexIndex];
}
