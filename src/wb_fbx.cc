
/* wb_fbx.h
 *
 * This isn't really useful software yet, I just packaged 
 * it separately to improve my compile times. 
 * 
 * wb_fbx loads simple FBX models, using fbxsdk.
 * ...and when I mean simple, I mean a subset that:
 * 		- Only has one layer per mesh
 * 		- Doesn't need any rotations
 * 		- You have to provide the material textures
 * 		- Has specific mapping/reference modes
 * 			- UVs must be PolygonVertex and IndexToDirect
 * 			- Normals must be ControlPoint and Direct
 * 		- Doesn't use anything other than Polygons, UVs, and Normals
 *
 * However, it does provide you with a bunch of data that's
 * easy to use with OpenGL, already transformed and scaled, 
 * with a nice-and-simple C API.
 *
 * You probably want to compile this separately, 
 * defining WB_FBX_IMPLEMENTATION with a compiler command.
 *
 */

#include <stddef.h>

typedef struct 
{
	float pos[4];
	float normal[4];
	float uv[2];
} wfbxVertex;

typedef struct
{
	float translation[3];
	float rotation[3];
	float scale[3];
} wfbxTransform;

typedef struct
{
	unsigned int diffuse, normal, pbr, emissive;
	int width, height;
} wfbxMaterialTexture;

typedef struct 
{
	wfbxVertex** meshes;
	ptrdiff_t* meshSizes;

	unsigned int** indices;
	ptrdiff_t* indexCounts;

	wfbxTransform* transforms;
	wfbxMaterialTexture* materials;

	ptrdiff_t count;
} wfbxModel;

#ifdef __cplusplus
extern "C" 
#endif
wfbxModel* wfbxLoadModelFromFile(
		const char* filename, 
		wfbxMaterialTexture* defaultMaterial);

#if WB_FBX_IMPLEMENTATION
//Allocators, overload at compile time
#define wfbxMalloc(size) malloc(size)
#define wfbxFree(ptr) free(ptr)

#define wfbxNew(type) (type*)wfbxMalloc(sizeof(type))
#define wfbxNewArray(type, count) (type*)wfbxMalloc(sizeof(type) * count)

typedef ptrdiff_t isize;
typedef unsigned int u32;
typedef float f32;

#define Fenum(t) FbxLayerElement::EType::e ## t
#include <fbxsdk.h>
#include <intrin.h>

static 
void buildModelFromMeshesRecursively(
		FbxNode* node, 
		isize* meshIndex, 
		wfbxModel* model,
		wfbxMaterialTexture* defaultMaterial);
static
void countMeshesRecursively(FbxNode* node, isize* meshCount);

wfbxModel* wfbxLoadModelFromFile(
		const char* fileName, 
		wfbxMaterialTexture* defaultMaterial)
{
	FbxManager* sdkManager = FbxManager::Create();
	FbxIOSettings* ios = FbxIOSettings::Create(sdkManager, IOSROOT);
	FbxImporter* importer = FbxImporter::Create(sdkManager, "");
	if(!importer->Initialize(fileName, -1, sdkManager->GetIOSettings())) {
		return NULL;
	}
	FbxScene* scene = FbxScene::Create(sdkManager, "defaultScene");
	importer->Import(scene);
	importer->Destroy();


	FbxNode* root = scene->GetRootNode();
	if(!root) { return NULL; }

	wfbxModel* model = wfbxNew(wfbxModel);
	isize meshCount = 0;
	countMeshesRecursively(root, &meshCount);

	model->meshes = wfbxNewArray(wfbxVertex*, meshCount);
	model->meshSizes = wfbxNewArray(isize, meshCount);
	model->indices = wfbxNewArray(u32*, meshCount);
	model->indexCounts = wfbxNewArray(isize, meshCount);
	model->transforms = wfbxNewArray(wfbxTransform, meshCount);
	model->materials = wfbxNewArray(wfbxMaterialTexture, meshCount);
	model->count = meshCount;
	isize meshIndex = 0;

	buildModelFromMeshesRecursively(root, &meshIndex, model, defaultMaterial);
	return model;
}

static 
void buildModelFromMeshesRecursively(
		FbxNode* node, 
		isize* meshIndex, 
		wfbxModel* model,
		wfbxMaterialTexture* defaultMaterial)
{
	FbxDouble3 nodeTrans = node->LclTranslation.Get();
	FbxDouble3 nodeRot = node->LclRotation.Get();
	FbxDouble3 nodeScale = node->LclScaling.Get();

	isize attribCount = node->GetNodeAttributeCount();
	for(isize i = 0; i < attribCount; ++i) {
		FbxNodeAttribute* attrib = node->GetNodeAttributeByIndex(i);
		if(attrib->GetAttributeType() == FbxNodeAttribute::eMesh) {
			FbxMesh* mesh = (FbxMesh*)attrib;
			isize count = mesh->GetControlPointsCount();

			wfbxVertex* modelMesh = wfbxNewArray(wfbxVertex, count);
			model->meshes[*meshIndex] = modelMesh;
			model->meshSizes[*meshIndex] = count;

			FbxStatus status;
			FbxDouble4* verts = mesh->GetControlPoints(&status);
			int* indices = mesh->GetPolygonVertices();

			isize indexCount = mesh->GetPolygonVertexCount();
			u32* modelIndices = wfbxNewArray(u32, indexCount);
			model->indices[*meshIndex] = modelIndices;
			model->indexCounts[*meshIndex] = indexCount;

			for(isize i = 0; i < indexCount; ++i) {
				modelIndices[i] = (u32)indices[i];
			}

			double* scale = nodeScale.Buffer();
			double* trans = nodeTrans.Buffer();
			double* rot = nodeRot.Buffer();

			for(isize i = 0; i < 3; ++i) {
				model->transforms[*meshIndex].translation[i] = (f32)trans[i];
				model->transforms[*meshIndex].scale[i] = (f32)scale[i];
				model->transforms[*meshIndex].rotation[i] = (f32)rot[i];
			}

			__m128 vscale = _mm_setr_ps(scale[0], scale[1], scale[2], 1);
			__m128 vtrans = _mm_setr_ps(trans[0], trans[1], trans[2], 0);
			//TODO(will): add rotation support
			//__mm128 vrot= _mm_setr_ps(rot[0],   rot[1],   rot[2],   0);

			//TODO(will): Support for multiple layers
			FbxLayer* l = mesh->GetLayer(0);
			FbxLayerElementUV* uvs = l->GetUVs();

			FbxLayerElementNormal* normals = l->GetNormals();
			auto normalArray = normals->GetDirectArray();
			for(isize i = 0; i < count; ++i) {
				double* vb = verts[i].Buffer();
				__m128 v = _mm_setr_ps(vb[0], vb[1], vb[2], vb[3]);
				v = _mm_mul_ps(v, vscale);
				v = _mm_add_ps(v, vtrans);
				*(__m128*)&modelMesh[i].pos = v;

				vb = normalArray[i].Buffer();
				v = _mm_setr_ps(vb[0], vb[1], vb[2], vb[3]);
				*(__m128*)&modelMesh[i].normal = v;
			}

			auto uvArray = uvs->GetDirectArray();
			auto uvIndices = uvs->GetIndexArray();

			for(isize i = 0; i < indexCount; ++i) {
				isize index = modelIndices[i];
				modelMesh[index].uv[0] = uvArray[uvIndices[i]].Buffer()[0];
				modelMesh[index].uv[1] = uvArray[uvIndices[i]].Buffer()[1];
			}
			model->materials[*meshIndex] = *defaultMaterial;
			*meshIndex = *meshIndex + 1;
		}
	}
	isize childCount = node->GetChildCount();
	for(isize i = 0; i < childCount; ++i) {
		buildModelFromMeshesRecursively(
				node->GetChild(i),
				meshIndex,
				model,
				defaultMaterial);
	}
}

static
void countMeshesRecursively(FbxNode* node, isize* meshCount)
{
	isize attribCount = node->GetNodeAttributeCount();
	for(isize i = 0; i < attribCount; ++i) {
		FbxNodeAttribute* attrib = node->GetNodeAttributeByIndex(i);
		if(attrib->GetAttributeType() == FbxNodeAttribute::eMesh) {
			*meshCount = *meshCount + 1;
		}
	}

	isize childCount = node->GetChildCount();
	for(isize i = 0; i < childCount; ++i) {
		countMeshesRecursively(node->GetChild(i), meshCount);
	}
}

#endif
