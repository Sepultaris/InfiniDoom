#version 450

layout(location = 0) in vec3 InPosition;
layout(location = 1) in vec3 InColor;
layout(location = 0) out vec3 ProbeColor;

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
	ProbeColor = InColor;
}
