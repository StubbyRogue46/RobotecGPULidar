#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "gdt/math/box.h"
#include "simple_uid_generator.h"

#include "gdt/utils/CUDABuffer.h"
#include <optix_types.h>

struct TransformMatrix {
    float matrix_flat[12] = { 0 };
} __attribute__((packed));

//indexed triangle mesh
struct TriangleMesh {
    TriangleMesh() { mesh_id = generate_simple_uid(); }
    TriangleMesh(const std::string& id)
        : mesh_id(id)
    {
    }

    bool is_global_coords = true;
    TransformMatrix transform;
    std::string mesh_id;
    std::vector<gdt::vec3f> vertex;
    std::vector<gdt::vec3f> normal;
    std::vector<gdt::vec2f> texcoord;
    std::vector<gdt::vec3i> index;

    // material data:
    gdt::vec3f diffuse;
    int diffuseTextureID { -1 };

    TriangleMesh(const TriangleMesh&) = delete; // non construction-copyable
    TriangleMesh& operator=(const TriangleMesh&) = delete; // non copyable
};

// TODO - non string id used for indexing
struct Texture {
    Texture(int id)
        : texture_id(std::to_string(id))
    {
    }

    std::string texture_id;
    ~Texture()
    {
        if (pixel)
            delete[] pixel;
    }

    uint32_t* pixel { nullptr };
    gdt::vec2i resolution { -1 };

    Texture(const Texture&) = delete; // non construction-copyable
    Texture& operator=(const Texture&) = delete; // non copyable
};

using Meshes = std::vector<std::shared_ptr<TriangleMesh>>;
using Textures = std::vector<std::shared_ptr<Texture>>;

class ModelInstance {
public:
    ModelInstance(std::shared_ptr<TriangleMesh> mesh);
    ~ModelInstance();
    OptixTraversableHandle buildGAS(OptixDeviceContext optixContext);
    OptixInstance buildIAS(unsigned int id);
    void updateMesh(std::shared_ptr<TriangleMesh> mesh);

    //(TODO) make a map since one model can contain multiple meshes
    std::shared_ptr<TriangleMesh> m_triangle_mesh;
    std::shared_ptr<Texture> m_texture;

    bool needs_rebuild { false };

    CUDABuffer m_vertex_buffer;
    CUDABuffer m_normal_buffer;
    CUDABuffer m_texcoord_buffer;
    CUDABuffer m_index_buffer;

private:
    OptixTraversableHandle _GAS_handle;
    OptixBuildInput triangleInput;
    CUDABuffer outputBuffer;
    CUDABuffer asBuffer;
    OptixInstance instance;
    OptixTraversableHandle meshHandle;
    CUdeviceptr d_vertices;
    CUdeviceptr d_indices;
    CUdeviceptr d_transforms;
    uint32_t triangleInputFlags;
};

typedef std::unordered_map<std::string, std::shared_ptr<ModelInstance>> InstancesMap;