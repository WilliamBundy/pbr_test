#define Math_DegToRad 0.017453292519943295f 
#define CameraDefaultUp v3(0, 1, 0)

typedef struct Texture Texture;
typedef struct Shader Shader;
typedef struct Camera Camera;
typedef struct vec3 vec3;

struct vec3
{
	f32 x, y, z;
};

struct Texture
{
	u32 id;
	i32 w, h;
	u8* pixels;
	string filename;
};

struct Shader 
{
	u32 program, vert, frag;
	string vertSrc, fragSrc;
};

struct Camera
{
	vec3 pos, target, up;
	vec3 xaxis, yaxis, zaxis, translate;
};

// Absolutely minimal vector math
static inline
vec3 v3(f32 x, f32 y, f32 z)
{
	vec3 v;
	v.x = x;
	v.y = y;
	v.z = z;
	return v;
}

static inline
vec3 v3Sub(vec3 a, vec3 b)
{
	return v3(a.x - b.x, a.y - b.y, a.z - b.z);
}

static inline
f32 v3Dot(vec3 a, vec3 b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline
vec3 v3Cross(vec3 a, vec3 b)
{
	vec3 z;
	z.x = a.y * b.z - a.z * b.y;
	z.y = a.z * b.x - a.x * b.z;
	z.z = a.x * b.y - a.y * b.x;
	return z;
}

static inline
vec3 v3Normalize(vec3 v)
{
	f32 mag2 = v3Dot(v, v);
	if(mag2 == 0) {
		return v3(0, 0, 0);
	}

	f32 mag = sqrtf(mag2);
	v.x /= mag;
	v.y /= mag;
	v.z /= mag;
	return v;
}

void printShaderError(u32 shader, string header)
{
	i32 success = 1;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if(!success) {
		GLchar log[4096];
		i32 logSize;
		glGetShaderInfoLog(shader, 4096, &logSize, log);
		fprintf(stderr, "\n%s\n%s\n\n", header, log);
	}
}

void printGLProgramError(u32 program, string header)
{
	i32 success = 1;
	glGetProgramiv(program, GL_LINK_STATUS, &success);

	if(!success) {
		GLchar log[4096];
		i32 logSize;
		glGetProgramInfoLog(program, 4096, &logSize, log);
		fprintf(stderr, "\n%s\n%s\n\n", header, log);
	}
}


void createShader(Shader* shader, string vertSrc, string fragSrc)
{
	shader->vert = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(shader->vert, 1, (const GLchar* const*)&vertSrc, NULL);
	glCompileShader(shader->vert);
	printShaderError(shader->vert, "Vertex Shader Compile Log");

	shader->frag = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(shader->frag, 1, (const GLchar* const*)&fragSrc, NULL);
	glCompileShader(shader->frag);
	printShaderError(shader->frag, "fragex Shader Compile Log");
	
	shader->program = glCreateProgram();
	glAttachShader(shader->program, shader->vert);
	glAttachShader(shader->program, shader->frag);
	glLinkProgram(shader->program);
	printGLProgramError(shader->program, "Program Link Log");
}

static inline
void identityMatrix4(f32* matrix)
{
	for(isize i = 0; i < 16; ++i) {
		matrix[i] = 0.0f;
	}

	matrix[0] = 1.0f;
	matrix[5] = 1.0f;
	matrix[10] = 1.0f;
	matrix[15] = 1.0f;
}

static inline
void clearMatrix4(f32* matrix)
{
	for(isize i = 0; i < 16; ++i) {
		matrix[i] = 0.0f;
	}
}

static inline
void perspectiveMatrix4(f32* matrix, f32 aspect, f32 fov, f32 nearPlane, f32 farPlane)
{
	clearMatrix4(matrix);
	f32 yScale = 1.0f / tanf((fov * Math_DegToRad) / 2);
	f32 xScale = yScale / aspect;
	f32 diff = nearPlane - farPlane;

	matrix[0] = xScale;
	matrix[5] = yScale;
	matrix[10] = farPlane / diff;
	matrix[11] = -1;
	matrix[14] = (2 * nearPlane * farPlane) / diff;
}

static inline
void orthoMatrix4(f32* matrix, f32 w, f32 h)
{
	clearMatrix4(matrix);

	matrix[0] = 2/w;
	matrix[5] = 2/h;
	matrix[10] = 1/(1.0-2048.0);
	matrix[14] = 1/(1.0-2048.0);
	matrix[15] = 1;
}

static inline
void makeCamera(Camera* cam, vec3 pos, vec3 target, vec3 up)
{
	cam->pos = pos;
	cam->target = target;
	cam->up = up;

	cam->zaxis = v3Normalize(v3Sub(pos, target));
	cam->xaxis = v3Normalize(v3Cross(cam->up, cam->zaxis));
	cam->yaxis = v3Cross(cam->zaxis, cam->xaxis);
	cam->translate = v3(
			-v3Dot(cam->xaxis, cam->pos),
			-v3Dot(cam->yaxis, cam->pos),
			-v3Dot(cam->zaxis, cam->pos));
}

static inline
void viewMatrix4(f32* matrix, Camera* cam)
{
	matrix[0] = cam->xaxis.x;
	matrix[1] = cam->yaxis.x;
	matrix[2] = cam->zaxis.x;
	matrix[3] = 0.0f;

	matrix[4] = cam->xaxis.y;
	matrix[5] = cam->yaxis.y;
	matrix[6] = cam->zaxis.y;
	matrix[7] = 0.0f;

	matrix[8] = cam->xaxis.z;
	matrix[9] = cam->yaxis.z;
	matrix[10] = cam->zaxis.z;
	matrix[11] = 0.0f;

	matrix[12] = cam->translate.x;
	matrix[13] = cam->translate.y;
	matrix[14] = cam->translate.z;
	matrix[15] = 1.0f;
}

Texture* loadTexture(string filename)
{
	i32 w, h, bpp;
	u8* data = stbi_load(filename, &w, &h, &bpp, STBI_rgb_alpha);
	if(w == 0 || h == 0) {
		return NULL;
	}

	Texture* t = (Texture*)malloc(sizeof(Texture));
	t->w = w;
	t->h = h;
	t->pixels = data;
	t->id = -1;
	return t; 
}

void uploadTextureToGpu(Texture* texture)
{
	glGenTextures(1, &texture->id);
	glBindTexture(GL_TEXTURE_2D, texture->id);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 
			texture->w, texture->h, 0, 
			GL_RGBA, GL_UNSIGNED_BYTE, 
			texture->pixels);
	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);
}
