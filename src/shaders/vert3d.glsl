#version 330
layout(location=0) in vec4 vPos;
layout(location=1) in vec4 vNormal;
layout(location=2) in vec2 vUV;

out vec4 fNormal;
out vec3 fRGB;
out vec3 fPos;
out vec3 fEye;
out vec2 fUV;

uniform vec3 uOffset;
uniform mat4 uProjection;
uniform mat4 uView;
uniform vec2 uTextureSize;

void main()
{
	vec4 localPos = uView * vec4(uOffset + vPos.xyz, 1);
	gl_Position = uProjection * localPos; 
	fPos = localPos.xyz;
	fEye = normalize(-fPos);
	fRGB = vec3(1.0, 1.0, 1.0);
	fUV = vUV;
	fNormal = transpose(inverse(uView)) * vNormal;
}
