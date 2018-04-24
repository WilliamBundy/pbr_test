#version 330

layout(location=0) in vec4 vPos;
layout(location=1) in vec4 vColor;
layout(location=2) in vec4 vPbr;

// normalized position within quad
// this gets interpolated over the fragment shader
out vec2 fPos;
out vec4 fColor;

uniform mat4 uView;
uniform mat4 uProjection;

float[4] corners = float[4](-0.5, -0.5, 0.5, 0.5);

void main()
{
	float size = 2;
	int vx = gl_VertexID & 2;
	int vy = ((gl_VertexID & 1) << 1) ^ 3;
	vec2 vert = size * vec2(corners[vx], corners[vy]);
	vec3 cameraLeft = vec3(uView[0][0], uView[1][0], uView[2][0]);
	vec3 cameraUp = vec3(uView[0][1], uView[1][1], uView[2][1]);
	vec4 lpos = vec4(vPos.xyz + cameraLeft * vert.x + cameraUp * vert.y, 1);
	gl_Position = uProjection * uView * lpos;
	fColor = vColor;
	fPos = vert;
}
