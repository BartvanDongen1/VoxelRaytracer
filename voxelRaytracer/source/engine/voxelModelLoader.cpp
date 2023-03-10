#include "engine/voxelModelLoader.h"
#include "engine\logger.h"

#include "engine\meshModel.h"

#include <unordered_map>
#include <iostream>
#include <rapidobj\rapidobj.hpp>

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

void loadModel(const char* aFileName);
void voxelizeMesh(const char* aFileName, int aResolution);
void voxelizeMesh2(const char* aFileName, int aResolution);
void ReportError(const rapidobj::Error& error);

VoxelModel* VoxelModelLoader::getModel(const char* aFileName, int aResolution)
{
	if (!modelMeshes.count(aFileName))
	{
		loadModel(aFileName);
	}

	if (!voxelizedModels.count({ aFileName, aResolution }))
	{
        voxelizeMesh2(aFileName, aResolution);
	}

    return &voxelizedModels[{ aFileName, aResolution }];
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

        myVoxelModel.setVoxel(myVoxelX, myVoxelY, myVoxelZ, 1);
    }

    voxelizedModels.insert({ { aFileName, aResolution }, myVoxelModel });
}

void voxelizeMesh2(const char* aFileName, int aResolution)
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

