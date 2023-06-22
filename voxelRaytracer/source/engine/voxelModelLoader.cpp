#include "engine/voxelModelLoader.h"
#include "engine\logger.h"

#include "engine\meshModel.h"

#include <unordered_map>
#include <iostream>
#include <rapidobj\rapidobj.hpp>

#define STB_IMAGE_IMPLEMENTATION
#pragma warning(push, 0)
//#pragma warning(disable : 4068)
#include <stb/stb_image.h>
#pragma warning(pop)

#define VOXELIZER_IMPLEMENTATION
#include <voxelizer\voxelizer.h>

// A hash function used to hash a pair of any kind
struct hash_pair
{
    template <class T1, class T2>
    size_t operator()(const std::pair<T1, T2>& p) const
    {
        auto hash1 = std::hash<T1>{}(p.first);
        auto hash2 = std::hash<T2>{}(p.second);

        if (hash1 != hash2) {
            return hash1 ^ hash2;
        }

        // If hash1 == hash2, their XOR is zero.
        return hash1;
    }
};

static std::unordered_map<const char*, MeshModel> modelMeshes;
static std::unordered_map<std::pair<const char* , int>, VoxelModel, hash_pair> voxelizedModels;
static std::unordered_map<const char*, Texture> textures;
static std::unordered_map<const char*, Texture> hdrTextures;

void loadModel(const char* aFileName);
void voxelizeMesh(const char* aFileName, int aResolution);
void voxelizeMesh2(const char* aFileName, int aResolution, int aFillVoxelIndex = -1);
void ReportError(const rapidobj::Error& error);

void loadTexture(const char* aFileName);
void loadHdrTexture(const char* aFileName);

VoxelModel* VoxelModelLoader::getModel(const char* aFileName, int aResolution, int aFillVoxelIndex)
{
	if (!modelMeshes.count(aFileName))
	{
		loadModel(aFileName);
	}

	if (!voxelizedModels.count({ aFileName, aResolution }))
	{
        voxelizeMesh2(aFileName, aResolution, aFillVoxelIndex);
	}

    return &voxelizedModels[{ aFileName, aResolution }];
}

Texture* VoxelModelLoader::getTexture(const char* aFileName)
{
    if (!textures.count(aFileName))
    {
        loadTexture(aFileName);
    }

    return &textures[aFileName];
}

Texture* VoxelModelLoader::getHdrTexture(const char* aFileName)
{
    if (!hdrTextures.count(aFileName))
    {
        loadHdrTexture(aFileName);
    }

    return &hdrTextures[aFileName];
}

void loadModel(const char* aFileName)
{
    auto result = rapidobj::ParseFile(aFileName);

    if (result.error) 
    {
        ReportError(result.error);
        return;
    }

    rapidobj::Triangulate(result);

    if (result.error) {
        ReportError(result.error);
        return;
    }

    AABB myAABB;
    myAABB.initInvertedInfinity();

    // Temp Buffer.
    std::vector<float> rawData;
    int indexOffset = 0;

    for (int i = 0; i < result.shapes.size(); i++)
    {
        rapidobj::Shape& myShape = result.shapes[i];

        for (int f = 0; f < myShape.mesh.num_face_vertices.size(); f++)
        {
            int fv = myShape.mesh.num_face_vertices[f];
            for (int v = 0; v < fv; v++)
            {
                rapidobj::Index idx = myShape.mesh.indices[static_cast<size_t>(indexOffset) + v];

                //only make use of the position data for now
                rawData.push_back(result.attributes.positions[3 * static_cast<size_t>(idx.position_index) + 0]);
                rawData.push_back(-result.attributes.positions[3 * static_cast<size_t>(idx.position_index) + 1]);
                rawData.push_back(result.attributes.positions[3 * static_cast<size_t>(idx.position_index) + 2]);

                glm::vec3 myPosition{ rawData[rawData.size() - 3], rawData[rawData.size() - 2], rawData[rawData.size() - 1] };

                myAABB.growToContain(myPosition);

                //rawData.push_back(result.attributes.normals[3 * idx.normal_index + 0]);
                //rawData.push_back(result.attributes.normals[3 * idx.normal_index + 1]);
                //rawData.push_back(result.attributes.normals[3 * idx.normal_index + 2]);
                //rawData.push_back(result.attributes.texcoords[2 * idx.texcoord_index + 0]);
                //rawData.push_back(result.attributes.texcoords[2 * idx.texcoord_index + 1]);
            }
            indexOffset += fv;
        }
    }

    MeshModel myModel;

    //one index per xyz
    myModel.positions = reinterpret_cast<float*>(malloc(rawData.size() * sizeof(float)));
    memcpy(myModel.positions, rawData.data(), rawData.size() * sizeof(float));
    myModel.vertexCount = rawData.size() / 3;

    myModel.aabb = myAABB;

    modelMeshes.insert({ aFileName, myModel });
}

void voxelizeMesh(const char* aFileName, int aResolution)
{
    MeshModel myModel = modelMeshes[aFileName];
    AABB myAABB = myModel.aabb;

    VoxelModel myVoxelModel{ aResolution, aResolution, aResolution };

	// voxelize mesh
    for (int i = 0; i < myModel.vertexCount; i++)
    {
        glm::vec3 myVertexPosition{ myModel.positions[3 * i], myModel.positions[3 * i + 1], myModel.positions[3 * i + 2] };

        glm::vec3 myNormalizedPosition = myAABB.calculateNormalizedPosition(myVertexPosition);

        int myVoxelX = static_cast<int>((myNormalizedPosition.x - 0.001f) * aResolution);
        int myVoxelY = static_cast<int>((myNormalizedPosition.y - 0.001f) * aResolution);
        int myVoxelZ = static_cast<int>((myNormalizedPosition.z - 0.001f) * aResolution);

        myVoxelModel.setVoxel(myVoxelX, myVoxelY, myVoxelZ, 2);
    }

    voxelizedModels.insert({ { aFileName, aResolution }, myVoxelModel });
}

void voxelizeMesh2(const char* aFileName, int aResolution, int aFillVoxelIndex)
{
    MeshModel myModel = modelMeshes[aFileName];
    
    // initialize mesh in correct format for voxelization

    vx_mesh_t* myMesh;
    myMesh = vx_mesh_alloc(myModel.vertexCount, myModel.vertexCount);

    // init position data
    for (int i = 0; i < myModel.vertexCount; i++)
    {
        vx_vertex_t myVertex;
        myVertex.x = myModel.positions[3 * i + 0];
        myVertex.y = myModel.positions[3 * i + 1];
        myVertex.z = myModel.positions[3 * i + 2];

        myMesh->vertices[i] = myVertex;

        myMesh->indices[i] = i;
    }

    unsigned int* data = vx_voxelize_snap_3dgrid(myMesh, aResolution, aResolution, aResolution);

    VoxelModel myVoxelModel{ aResolution, aResolution, aResolution };
    myVoxelModel.data = data;

    if (aFillVoxelIndex != -1)
    {
        //replace all voxels that are filled in with a specific index that isn't 0
        for (int i = 0; i < myVoxelModel.dataSize; i++)
        {
            myVoxelModel.data[i] = (!(myVoxelModel.data[i] == 0)) * aFillVoxelIndex;
        }
    }

    voxelizedModels.insert({ { aFileName, aResolution }, myVoxelModel });
}

// rapidobj error handling
void ReportError(const rapidobj::Error& error)
{
    LOG_ERROR((error.code.message() + "\n").c_str());
    
    if (!error.line.empty()) 
    {
        LOG_ERROR("On line %i: %s \n", error.line_num, error.line.c_str());
    }
}

void loadTexture(const char* aFileName)
{
    // load image
    int width;
    int height;
    int comp;
    unsigned char* data = nullptr;
    data = stbi_load(aFileName, &width, &height, &comp, 4);

    // assert data is loaded
    assert(data);

    int totalMemSize = sizeof(unsigned char) * comp * width * height;

    Texture myTexture;
    myTexture.textureWidth = width;
    myTexture.textureHeight = height;
    myTexture.bytesPerPixel = comp;
    myTexture.textureData = data;

    textures.insert({ aFileName, myTexture });
}

void loadHdrTexture(const char* aFileName)
{
    // load image
    int width;
    int height;
    int comp;
    float* data = nullptr;
    data = stbi_loadf(aFileName, &width, &height, &comp, 4);

    // assert data is loaded
    assert(data);

    int totalMemSize = sizeof(float) * comp * width * height;

    Texture myTexture;
    myTexture.textureWidth = width;
    myTexture.textureHeight = height;
    myTexture.bytesPerPixel = sizeof(float) * 4;
    myTexture.textureData = data;

    hdrTextures.insert({ aFileName, myTexture });
}
