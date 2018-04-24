#version 450

// Normal from model
in vec4 fNormal;
// This is always white
in vec3 fRGB;
in vec3 fPos;
in vec3 fEye;
// I actually think these should be STs in OpenGL parlance?
// UV = pixel space
// ST = normalized image space
// Not that important
in vec2 fUV;
// Color output
out vec4 gColor;

// We need this for light and normal transforms
uniform mat4 uView;

// Light struct and scene buffer
// I'm using vec4's to be explicit; everything's aligned to 16 bytes anyway
struct Light
{
	vec4 pos;
	vec4 color;
};

layout(std430, location=1) buffer SceneBuffer
{
	Light lights[16];
	int lightCount;
} scene;

// All of our textures. I could have made a texture array, but this was simpler
uniform sampler2D uDiffuse;
uniform sampler2D uNormal;
uniform sampler2D uPbr;
uniform sampler2D uEmissive;

// f0 is base specular
// Product could be NdV or HdV depending on technique
// We use the latter with Cook-Torrance
vec3 fresnelFactor(vec3 f0, float product)
{
	return mix(f0, vec3(1.0), pow(1.0 - product, 5.0));
}

// Code here borrowed from UE4

// I went with this distribution function... it sounds cool?
float D_GGX(float roughness, float NdH)
{
	float m = roughness * roughness;
	float m2 = m * m;
	float d = (NdH * m2 - NdH) * NdH + 1.0;
	return m2 / (3.1415926 * d * d);
}

// G is the geometry shading term
float G_schlick(float roughness, float NdV, float NdL)
{
	float k = roughness * roughness * 0.5;
	float V = NdV * (1.0 - k) + k;
	float L = NdL * (1.0 - k) + k;
	return 0.25 / (V * L);
}

float G_schlickGGX(float roughness, float NdV)
{
	float r = roughness + 1;
	float k = (r * r) / 8.0;
	return NdV / (NdV * (1.0 - k) + k);
}

float G_smith(float roughness, float NdV, float NdL)
{
	float ggx2 = G_schlickGGX(roughness, NdV);
	float ggx1 = G_schlickGGX(roughness, NdL);
	return ggx1 * ggx2;
}

// 
vec3 specularCookTorrance(float NdL, float NdV, float NdH, 
		vec3 F, float rough)
{
	float D = D_GGX(rough, NdH);
	float G = G_smith(rough, NdV, NdL);
	return F * G * D / max(4 * NdL * NdV, 0.001);
}


// The big idea with PBR is to model materials based on our physical
// understanding of light. It turns out that we actually don't need
// that much data to do this:
// 		- albedo is the diffuse light/base color
// 		- metallic models how much metal is in a material,
// 		which directly affects how it reflects light
// 		- roughness models the microscopic bumps on a surface;
// 		the rougher it is, the more diffuse the reflected light
// 		- normals aren't technically PBR, but they help generate
// 		bigger details on a surface that reflect light differently.
//
// From here, there's a few things I could do to make it better:
// 		- I never actually tested it with some impostored spheres, 
// 		just the model. If Nsight actually worked, doing this would
// 		be a great way to debug it.
// 		- Image Based Lighting: as it is, the scene is very dark, 
// 		because (at least, I think this is the case), when no light
// 		is entering a fragment from a point light, the fragment 
// 		'reflects' the darkness. IBL more-or-less substitutes reflecting
// 		that with reflecting a cubemap around it.
// 		- All of the physical lighting ideas could be implemented
// 		in my raytracer. A lot of these come from raytracing originally;
// 		it's essentially pushing more data through the algorithm.
// 		- You can kind of see lights through the models?
// 		- Shadow mapping (with attention given to self-shadowing)
void main()
{
	vec4 color = texture(uDiffuse, fUV) * vec4(fRGB, 1);
	vec4 normal = texture(uNormal, fUV);
	vec4 pbr = texture(uPbr, fUV);
	vec4 emissive = texture(uEmissive, fUV);
	
	// Apply ambient occlusion to albedo map
	color *= pbr.z;

	vec3 vertexNormal = fNormal.xyz;
	//create TBN matrix
	vec3 posDx = dFdx(fPos);
	vec3 posDy = dFdy(fPos);
	vec2 texDx = dFdx(fUV);
	vec2 texDy = dFdy(fUV);
	vec3 tangent = normalize(texDy.y * posDx - texDx.y * posDy);
	vec3 binormal = normalize(texDy.x * posDx - texDx.x * posDy);
	//rebuild tangent from existing normal
	vec3 xAxis = cross(vertexNormal, tangent);
	tangent = normalize(cross(xAxis, vertexNormal));
	//now bi-tangent
	xAxis = cross(binormal, vertexNormal);
	binormal = normalize(cross(vertexNormal, xAxis));
	mat3 tbn = mat3(tangent, binormal, vertexNormal);

	//transform normal map into real space
	vec3 N = normalize(tbn * (normal.xyz * 2.0 - 1.0));

	// grab some PBR terms
	float roughness = pbr.y;
	float metallic = pbr.x;

	// 0.04 here is a base specular for non-metallic materials
	// Also called F0 for Frenel equation
	// F0 = lerp(0.4, albedo, metallic)
	vec3 specular = mix(vec3(0.04), color.xyz, metallic);
	
	// output color in linear space
	vec3 lightSum = vec3(0);
	

	for(int i = 0; i < scene.lightCount; ++i) {
		// Light position in view space
		vec3 localLight = (uView * vec4(scene.lights[i].pos.xyz, 1)).xyz;
		
		// The constant here is an artistic choice
		// Values >1 are effectively a multiplier on brightness
		float attenuation = 4.0 / dot(localLight - fPos, localLight - fPos);
		vec3 lightDirection = normalize(localLight - fPos);

		vec3 V = fEye;
		vec3 L = lightDirection;
		// If you think of fViewPos as the position in view space
		// it's like a line from the camera to the fragment
		// So the opposite of it is a line from the fragment to the camera
		//vec3 viewDirection = normalize(-fEyePos);
		
		// Not sure how to motivate this term?
		// I think it ends up being the reflection direction
		vec3 H = normalize(lightDirection + fEye);


		// Doing all of these dot products together rather
		// than spreading them out over the various equations
		// We put bounds on them to help keep the numbers sane
		float NdL = max(0.0,   dot(N, L));
		float NdV = max(0.001, dot(N, V));
		float NdH = max(0.001, dot(N, H));
		float HdV = max(0.001, dot(H, V));
		float LdV = max(0.001, dot(L, V));

		// This is the 'magic'
		// The BRDF lets us understand the light entering
		// and exiting the fragment, so we can choose whether 
		// to make it bright or soft
		vec3 specFresnel = fresnelFactor(specular, HdV);
		//specular reflectance
		vec3 specRef = specularCookTorrance(
				NdL, NdV, NdH,
				specFresnel,
				roughness);

		specRef *= vec3(NdL);

		// constant here is the phong/lambertian diffuse term, ie 1.0 / PI
		vec3 diffuseRef = (vec3(1.0) - specFresnel) * 0.3183098 * NdL;

		// Just... go ahead and assemble light from what we've got.
		vec3 reflectedLight = vec3(0);
		vec3 diffuseLight = vec3(0);

		vec3 radiance = scene.lights[i].color.rgb * attenuation;

		reflectedLight += specRef * radiance;
		diffuseLight += diffuseRef * radiance;

		diffuseLight += emissive.rgb;
		reflectedLight += emissive.rgb;

		//...and here's where we'd do IBL lighting with a cubemap

		// Apparently the surface we have here is almost completely metallic, 
		// which means that the diffuse light terms are almost completely 
		// cancelled out (on the spaceship model)
		vec3 result = diffuseLight * mix(color.xyz, vec3(0), metallic) + reflectedLight;
		
		// This is technically squaring the NdL term
		// ...but I think it looks better, so I left it in.
		lightSum += result;
	}


	// Gamma correction
	lightSum = lightSum / (lightSum + vec3(1.0));
	lightSum = pow(lightSum, vec3(1.0/2.2));
	gColor = vec4(lightSum, 1);
}
