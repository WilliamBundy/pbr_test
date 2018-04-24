// 

// stddef gets me ptrdiff_t, which I use a lot,
// stdint gets me nice integers types,
// and stdio gets me printf. 
// If it weren't for fbxsdk, I probably wouldn't
// compile with the CRT.
#include <stddef.h>
#include <stdint.h>
#include <stdio.h> 

// I'm only using SDL2 for window and context creation
// I'm not sure what platforms this will have to use, 
// so I'm leaving the SDL route as the default. I have 
// a win32 platform layer effectively ready to go, 
// but SDL2 is my go-to for cross-platform code.
#include <SDL2/SDL.h>

// I have a little tool that embeds shaders in a C file
// for C++ I usually use here-strings, ie R"shader( ... )"
// but in C we don't have those
#include "shaders.h"

// Other than the CRT and fbxsdk, stb_image is the only library I'm using.
// PNG decoding is involved even if you have a deflate decoder handy, 
// so this ends up being the lightest implementation around.
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_ONLY_PNG
#include "stb_image.h"

// I put this compact loader together a while ago, partly because 
// I wanted the lightest thing possible (it's close), and partly because 
// the other one I've used a lot (glad) comes in a bunch of 
// files and uses library functions.
#define WB_GL_SDL
#define WB_GL_LOADER_IMPLEMENTATION
#define WB_GL_USE_ALL_VERSIONS
#include "wb_gl_loader.h"

// nice numerical types
typedef int32_t i32;
typedef uint8_t u8;
typedef uint32_t u32;
typedef float f32;
typedef ptrdiff_t isize;

// personal substitute; I never use std::string
// exclusively means "string literal" in my code
typedef const char* string;

// I compile the fbx loading code separately 
// because huge libraries in C++ take a long
// time to compile
#include "wb_fbx.cc"

// I pulled a lot of the stuff that goes into 
// rendering into its own file to help keep this one
// more organized.
//
// Sticking to one translation unit keeps file
// counts and compile times low, while making
// building very simple.
#include "render_util.c"

// render_utils.c prototypes
//
// The program doesn't need these to run
// Hopefully self-explanatory
Texture* loadTexture(string filename);
void uploadTextureToGpu(Texture* texture);
void createShader(Shader* shader, string vertSrc, string fragSrc);

// Standard look-at camera setup
void makeCamera(Camera* cam, vec3 pos, vec3 target, vec3 up);

// Some simple matrix functions, using raw arrays.
// If C allowed me to do `typedef f32[16] Matrix4;` 
// I would, but I don't think it's particularly 
// unclear what these types could mean.
void viewMatrix4(f32* matrix, Camera* cam);
void identityMatrix4(f32* matrix);
void clearMatrix4(f32* matrix);
void perspectiveMatrix4(f32* matrix, 
		f32 aspect, f32 fov,
		f32 nearPlane, f32 farPlane);
void orthoMatrix4(f32* matrix, f32 w, f32 h);

// Some convenience structure for creating lighting
typedef struct
{
	float pos[4];
	float color[4];
} Light;

struct {
	Light lights[16];
	int lightCount;
} scene;

void addLight(f32 x, f32 y, f32 z, f32 r, f32 g, f32 b)
{
	if(scene.lightCount >= 16) return;
	Light* l = scene.lights + scene.lightCount++;
	l->pos[0] = x;
	l->pos[1] = y;
	l->pos[2] = z;
	l->pos[3] = 0;

	l->color[0] = r;
	l->color[1] = g;
	l->color[2] = b;
	l->color[3] = 1;
}

// I know long functions are generally frowned upon, but in
// the name of simplicity, I think it makes sense for a program
// this small to keep the main program code together and sequential
int main(int argc, char** argv)
{
	stbi_set_flip_vertically_on_load(1);

	// Simplistic way of setting up command line args
	string fileName = NULL;
	string diffuseTextureName = NULL;
	string normalTextureName = NULL;
	string pbrTextureName = NULL;
	string emissiveTextureName = NULL;
	if(argc > 1) {
		fileName = argv[1];
		if(argc > 2) {
			diffuseTextureName = argv[2];
		}
		if(argc > 3) {
			normalTextureName = argv[3];
		}
		if(argc > 4) {
			pbrTextureName = argv[4];
		}
		if(argc > 5) {
			emissiveTextureName = argv[5];
		}
	} else {
		fileName = "model0/enemyFighter.fbx";
		diffuseTextureName = "model0/diffuse.png";
		normalTextureName = "model0/normals.png";
		pbrTextureName = "model0/pbr.png";
		emissiveTextureName = "model0/emissive.png";
	}
	printf("Using these textures and files:\n%s\n%s\n%s\n%s\n%s\n", 
			fileName,
			diffuseTextureName, 
			normalTextureName, 
			pbrTextureName, 
			emissiveTextureName);

	if(!fileName) {
		printf("No model provided, quitting...\n");
		return 0;
	}

	// Initialization and window creation
	SDL_Init(SDL_INIT_EVERYTHING);
#define glattr(attr, val) SDL_GL_SetAttribute(SDL_GL_##attr, val)
	glattr(RED_SIZE, 8);
	glattr(GREEN_SIZE, 8);
	glattr(BLUE_SIZE, 8);
	glattr(ALPHA_SIZE, 8);
	glattr(DEPTH_SIZE, 24);
	glattr(STENCIL_SIZE, 8);

	glattr(DOUBLEBUFFER, 1);
	glattr(FRAMEBUFFER_SRGB_CAPABLE, 1);
	glattr(MULTISAMPLEBUFFERS, 1);
	glattr(MULTISAMPLESAMPLES, 4);

	// I'm using a 4.5 context for SSBOs
	glattr(CONTEXT_MAJOR_VERSION, 4);
	glattr(CONTEXT_MINOR_VERSION, 5);
	glattr(CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	SDL_Window* window = SDL_CreateWindow(
			"3D Test",
			SDL_WINDOWPOS_CENTERED_DISPLAY(1),
			SDL_WINDOWPOS_CENTERED_DISPLAY(1),
			1280, 720,
			SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL
			);

	SDL_GLContext glctx = SDL_GL_CreateContext(window);
	if(!glctx) {
		printf("GL context failed %s\n", SDL_GetError());
		return 1;
	}

	SDL_GL_MakeCurrent(window, glctx);
	struct wbgl_ErrorContext errorCtx;
	if(wbgl_load_all(&errorCtx)) {
		printf("Failed to load %d OpenGL functions \n", errorCtx.error_count);
		// We don't return here, because it's possible
		// we loaded everything we need. 
	}
	
	// Query window size for glViewport
	float windowWidth = 1280, windowHeight = 720;
	{
		int ww, wh;
		SDL_GetWindowSize(window, &ww, &wh);
		windowWidth = (f32)ww;
		windowHeight = (f32)wh;
		glViewport(0, 0, windowWidth, windowHeight);
		glClearColor(0.01, 0.01, 0.01, 1);

		glEnable(GL_DEPTH_TEST);
		glEnable(GL_MULTISAMPLE);
	}

	// Most of our OpenGL state
	u32 vao, vbo, eab, ssbo;
	Shader shader;

	Camera cam;

	Texture *diffuse = NULL, *normals = NULL, *pbr = NULL, *emissive = NULL;
	i32 uViewLoc, uProjLoc, uDiffuse, uNormal, uPbr, uEmissive, uOffset, uDoLightSkip;
	f32 projMatrix[16], viewMatrix[16];
	i32 lightSkip = 1;
	wfbxModel* model = NULL;
	{
		// Load our textures if we got filenames for them
		if(diffuseTextureName) {
			diffuse = loadTexture(diffuseTextureName);
			uploadTextureToGpu(diffuse);
		}

		if(normalTextureName) {
			normals = loadTexture(normalTextureName);
			uploadTextureToGpu(normals);
		}

		if(pbrTextureName) {
			pbr = loadTexture(pbrTextureName);
			uploadTextureToGpu(pbr);
		}

		if(emissiveTextureName) {
			emissive = loadTexture(emissiveTextureName);
			uploadTextureToGpu(emissive);
		}

		// Create our model
		wfbxMaterialTexture defaultTexture = {
			diffuse->id, normals->id, pbr->id, emissive->id,
			diffuse->w, diffuse->h
		};
		model = wfbxLoadModelFromFile(fileName, &defaultTexture);

		// Do all the OpenGL stuff that OpenGL wants
		// vert3d and frag3d are from shaders.h, by the way.
		createShader(&shader, vert3d, frag3d);
		glCullFace(GL_BACK);
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		glGenBuffers(1, &vbo);
		glGenBuffers(1, &eab);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);

		isize i = 0; 
		i32 stride = sizeof(wfbxVertex);
#define voffset(name) (void*)(offsetof(wfbxVertex, name))
		glVertexAttribPointer(i, 4, GL_FLOAT, 0, stride, voffset(pos));
		glEnableVertexAttribArray(i++);
		glVertexAttribPointer(i, 4, GL_FLOAT, 0, stride, voffset(normal));
		glEnableVertexAttribArray(i++);
		glVertexAttribPointer(i, 2, GL_FLOAT, 0, stride, voffset(uv));
		glEnableVertexAttribArray(i++);

		// Buffer static model data
		glBufferData(GL_ARRAY_BUFFER, 
				sizeof(wfbxVertex) * model->meshSizes[0], 
				model->meshes[0],
				GL_STATIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eab);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, 
				sizeof(int) * model->indexCounts[0], 
				model->indices[0],
				GL_STATIC_DRAW);

		glUseProgram(shader.program);

		// Uniform locations
		uProjLoc = glGetUniformLocation(shader.program, "uProjection");
		uViewLoc = glGetUniformLocation(shader.program, "uView");
		uOffset = glGetUniformLocation(shader.program, "uOffset");
		uDoLightSkip = glGetUniformLocation(shader.program, "uDoLightSkip");

		glUniform1i(uDoLightSkip, lightSkip);

		// Map shader texture slots
		uDiffuse = glGetUniformLocation(shader.program, "uDiffuse");
		uNormal = glGetUniformLocation(shader.program, "uNormal");
		uPbr = glGetUniformLocation(shader.program, "uPbr");
		uEmissive = glGetUniformLocation(shader.program, "uEmissive");

		glUniform1i(uDiffuse, 0);
		glUniform1i(uNormal, 1);
		glUniform1i(uPbr, 2);
		glUniform1i(uEmissive, 3);

		// Set up ssbo for lights
		glGenBuffers(1, &ssbo);
		i32 index = glGetProgramResourceIndex(
				shader.program, 
				GL_SHADER_STORAGE_BLOCK,
				"SceneBuffer");
		glShaderStorageBlockBinding(shader.program, index, 1);

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbo);
		glBindVertexArray(0);
	}

	// Setup OpenGL for light circles
	Shader lightShader;
	u32 lightVao, lightVbo;
	i32 uLightProjLoc, uLightViewLoc;
	i32 qq = 0;
	{
		createShader(&lightShader, vertSimple, fragSimple);
		glUseProgram(lightShader.program);

		glGenVertexArrays(1, &lightVao);
		glBindVertexArray(lightVao);
		glGenBuffers(1, &lightVbo);
		glBindBuffer(GL_ARRAY_BUFFER, lightVbo);

		glVertexAttribPointer(0, 4, GL_FLOAT, 0, sizeof(Light), (void*)0);
		glEnableVertexAttribArray(0);
		glVertexAttribDivisor(0, 1);
		glVertexAttribPointer(1, 4, GL_FLOAT, 0, sizeof(Light), (void*)16);
		glEnableVertexAttribArray(1);
		glVertexAttribDivisor(1, 1);


		uLightProjLoc = glGetUniformLocation(lightShader.program, "uProjection");
		uLightViewLoc = glGetUniformLocation(lightShader.program, "uView");

	}

	// Generic timer
	f32 t = 0.0;

	int running = 1;
	SDL_Event event;
	while(running) {
		// Event handling.
		//
		while(SDL_PollEvent(&event)) {
			switch(event.type) {
				case SDL_QUIT:
					running = 0;
					goto MainLoopEnd;
				case SDL_KEYDOWN:
					glUseProgram(shader.program);
					lightSkip = !lightSkip;
					glUniform1i(uDoLightSkip, lightSkip);
					break;
				case SDL_WINDOWEVENT: {
					SDL_WindowEvent win = event.window;
					if(win.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
						windowWidth = (f32)win.data1;
						windowHeight = (f32)win.data2;
						glViewport(0, 0, windowWidth, windowHeight);

					}
				} break;
			}
		}

		// I clear all of these; some vendors don't initialize them to zero
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		// Render our model.
		{
			//Update camera and projection for both shaders
			f32 camDist = 8;
			makeCamera(&cam, 
					v3(sinf(t) * camDist, camDist * 1.5, cosf(t) * camDist), 
					v3(0, 6, 0), CameraDefaultUp);
			t += 0.005f;
			viewMatrix4(viewMatrix, &cam);
			perspectiveMatrix4(projMatrix, 
					windowWidth / windowHeight, 
					90, 0.02f, 1000.0f);
			glUseProgram(shader.program);
			glUniformMatrix4fv(uProjLoc, 1, 0, projMatrix);
			glUniformMatrix4fv(uViewLoc, 1, 0, viewMatrix);

			glUseProgram(lightShader.program);
			glUniformMatrix4fv(uLightProjLoc, 1, 0, projMatrix);
			glUniformMatrix4fv(uLightViewLoc, 1, 0, viewMatrix);
			 
			// move and upload lights every frame
			{
				scene.lightCount = 0;
				addLight(cam.pos.x, cam.pos.y + 1, cam.pos.z, 1, 1, 1);
				addLight(-6, 8, 1, 1, 0.5, 1);
				addLight(10 + cosf(t*4)*2, 5+sinf(t*4)*2, -3, 1, 0.5, 0.5);
				addLight(cosf(t*4), -10, 0, (cosf(t*3)+1)/2.0, 0.5, 1);
				addLight(0, sinf(t*2) * 6 + 4, 4.5, 1, 1, 1);
				addLight(-1, 8, -12, 1, 1, 1);


				glBindVertexArray(vao);
				glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
				glBufferData(
						GL_SHADER_STORAGE_BUFFER,
						sizeof(scene),
						&scene,
						GL_STREAM_DRAW);
				glBindVertexArray(0);
			}

			//Bind textures if available
			if(diffuse) {
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, diffuse->id);
			}

			if(normals) {
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, normals->id);
			}

			if(pbr) {
				glActiveTexture(GL_TEXTURE2);
				glBindTexture(GL_TEXTURE_2D, pbr->id);
			}

			if(emissive) {
				glActiveTexture(GL_TEXTURE3);
				glBindTexture(GL_TEXTURE_2D, emissive->id);
			}

			// Draw a bunch of them all over the place
			{
				glEnable(GL_CULL_FACE);
				glUseProgram(shader.program);
				glBindVertexArray(vao);
				glBindBuffer(GL_ARRAY_BUFFER, vbo);

				glUniform3f(uOffset, 0, -2, 0);
				glDrawElements(GL_TRIANGLES, 
						model->indexCounts[0], 
						GL_UNSIGNED_INT, 0);

				glUniform3f(uOffset, 10, -2, 0);
				glDrawElements(GL_TRIANGLES,
						model->indexCounts[0], 
						GL_UNSIGNED_INT, 0);

				glUniform3f(uOffset, -10, -2, 0);
				glDrawElements(GL_TRIANGLES, 
						model->indexCounts[0],
						GL_UNSIGNED_INT, 0);

				glUniform3f(uOffset, 0, 8, 0);
				glDrawElements(GL_TRIANGLES, 
						model->indexCounts[0],
						GL_UNSIGNED_INT, 0);

				glUniform3f(uOffset, 0, -10, 5);
				glDrawElements(GL_TRIANGLES, 
						model->indexCounts[0],
						GL_UNSIGNED_INT, 0);
				glBindVertexArray(0);
			}

			// Draw some circles to represent our lights
			{
				glDisable(GL_CULL_FACE);
				glUseProgram(lightShader.program);
				glBindVertexArray(lightVao);
				glBindBuffer(GL_ARRAY_BUFFER, lightVbo);
				glBufferData(GL_ARRAY_BUFFER,
						sizeof(Light) * scene.lightCount,
						scene.lights,
						GL_STREAM_DRAW);
				glDrawArraysInstanced(
						GL_TRIANGLE_STRIP,
						0, 4, scene.lightCount);
				glBindVertexArray(0);
			}


			// In case something went wrong...
			i32 e = glGetError();
			if(e) {
				printf("Runtime error: %d\n", e);
				goto MainLoopEnd;
			}
		}


		SDL_GL_SwapWindow(window);
	}
	// The side-effect of long stretches of inline code
	// is that you have to explicity use goto to skip 
	// execution, rather than just returning in the middle
	// of a function
MainLoopEnd:

	SDL_Quit();
	return 0;
}


