#include "mesh.hpp"

#include <iostream>
#include <tiny_gltf.h>

#include "renderer.hpp"

std::optional<Mesh> Mesh::CreateMesh(const std::string& path, Renderer& renderer)
{
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err;
    std::string warn;


    bool success = loader.LoadASCIIFromFile(&model, &err, &warn, path);
    if(!warn.empty())
        std::cout << warn << std::endl;
    if (!err.empty())
        std::cout << err << std::endl;
    if(!success)
    {
        std::cout << "Failed parsing GLtf" << std::endl;
        return std::nullopt;
    }

    std::vector<glm::vec3> positions;
    std::vector<Renderer::Vertex> vertices{};
    std::vector<uint32_t> indices{};

    for(size_t i = 0; i < model.meshes.size(); ++i)
    {
        tinygltf::Primitive primitive = model.meshes[i].primitives[0];
        
        tinygltf::Accessor positionAccessor = model.accessors[primitive.attributes["POSITION"]];
        tinygltf::BufferView positionBufferView = model.bufferViews[positionAccessor.bufferView];
        tinygltf::Buffer positionBuffer = model.buffers[positionBufferView.buffer];

        auto firstPositionByte = positionBuffer.data.begin() + positionBufferView.byteOffset + positionAccessor.byteOffset;
        auto lastPositionByte = positionBuffer.data.begin() + positionBufferView.byteOffset + positionBufferView.byteLength;

        if(positionBufferView.byteStride == 0)
        {
            positions = { firstPositionByte, lastPositionByte };
        }



        tinygltf::Accessor indicesAccessor = model.accessors[primitive.indices];
        tinygltf::BufferView indicesBufferView = model.bufferViews[indicesAccessor.bufferView];
        tinygltf::Buffer indicesBuffer = model.buffers[indicesBufferView.buffer];

        if(indicesAccessor.componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
        {
            std::cout << "Failed parsing index type" << std::endl;
            return std::nullopt;
        }


        auto firstIndexByte = indicesBuffer.data.begin() + indicesBufferView.byteOffset + indicesAccessor.byteOffset;
        auto lastIndexByte = indicesBuffer.data.begin() + indicesBufferView.byteOffset + indicesBufferView.byteLength;
        if (indicesBufferView.byteStride == 0)
        {
            indices = { firstIndexByte, lastIndexByte };
        }
    }

    Mesh mesh{};
    mesh.vertBuf = renderer.CreateBuffer(vertices.data(), sizeof(Renderer::Vertex) * vertices.size(), wgpu::BufferUsage::Vertex, "Vertex buffer");
    mesh.indexBuf = renderer.CreateBuffer(indices.data(), sizeof(uint32_t) * indices.size(), wgpu::BufferUsage::Index, "Index buffer");
    mesh.indexFormat = wgpu::IndexFormat::Uint32;
    mesh.indexCount = indices.size();

    return mesh;
}
