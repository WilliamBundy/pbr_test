#version 330

in vec2 fPos;
in vec4 fColor;

out vec4 gColor;
void main()
{
	vec4 color = fColor;
	float dist2 = dot(fPos, fPos) - 0.25; //0.5 ^ 2
	if(dist2 > 0) discard;
	gColor = color;
}
