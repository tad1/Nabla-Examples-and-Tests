#version 450

layout(location = 0) out vec3 fragColor;

layout( push_constant ) uniform constants
{
	float phase;
} pc;

vec2 positions[3] = vec2[](
 vec2(0.0+(1-pc.phase), -0.5+pc.phase),
 vec2(0.5-pc.phase, 0.5+(1-pc.phase)),
 vec2(-0.5-(1-pc.phase), 0.5-pc.phase)
);

vec3 colors[3] = vec3[](
 vec3(1.0-pc.phase, 0.0+pc.phase, 0.0),
 vec3(0.0, 1.0-pc.phase, 0.0+pc.phase),
 vec3(0.0+pc.phase, 0.0, 1.0-pc.phase)
);

void main() {
 gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
 fragColor = colors[gl_VertexIndex];
}