#include "mesh.hpp"

#include <iostream>
#include <optional>
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
    std::vector<glm::vec3> normals;
    std::vector<Renderer::Vertex> vertices{};
    std::vector<uint32_t> indices32{};
    std::vector<uint16_t> indices16{};

    int32_t albedoIndex;

    for(size_t i = 0; i < model.meshes.size(); ++i)
    {
        tinygltf::Primitive primitive = model.meshes[i].primitives[0];

        tinygltf::Material material = model.materials[primitive.material];
        albedoIndex = material.pbrMetallicRoughness.baseColorTexture.index;

        // Get positions.
        {
            tinygltf::Accessor positionAccessor = model.accessors[primitive.attributes["POSITION"]];
            tinygltf::BufferView positionBufferView = model.bufferViews[positionAccessor.bufferView];
            tinygltf::Buffer positionBuffer = model.buffers[positionBufferView.buffer];

            auto firstPositionByte = positionBuffer.data.begin() + positionBufferView.byteOffset + positionAccessor.byteOffset;
            auto lastPositionByte = positionBuffer.data.begin() + positionBufferView.byteOffset + positionBufferView.byteLength;
            positions = std::vector<glm::vec3>(std::distance(firstPositionByte, lastPositionByte) / sizeof(glm::vec3));
            memcpy(positions.data(), &*firstPositionByte, std::distance(firstPositionByte, lastPositionByte));
        }

        // Get normals.
        {
            tinygltf::Accessor normalAccessor = model.accessors[primitive.attributes["NORMAL"]];
            tinygltf::BufferView normalBufferView = model.bufferViews[normalAccessor.bufferView];
            tinygltf::Buffer normalBuffer = model.buffers[normalBufferView.buffer];

            auto firstNormalByte = normalBuffer.data.begin() + normalBufferView.byteOffset + normalAccessor.byteOffset;
            auto lastNormalByte = normalBuffer.data.begin() + normalBufferView.byteOffset + normalBufferView.byteLength;
            normals = std::vector<glm::vec3>(std::distance(firstNormalByte, lastNormalByte) / sizeof(glm::vec3));
            memcpy(normals.data(), &*firstNormalByte, std::distance(firstNormalByte, lastNormalByte));
        }

        // Get indices.
        {
            tinygltf::Accessor indicesAccessor = model.accessors[primitive.indices];
            tinygltf::BufferView indicesBufferView = model.bufferViews[indicesAccessor.bufferView];
            tinygltf::Buffer indicesBuffer = model.buffers[indicesBufferView.buffer];

            auto firstIndexByte = indicesBuffer.data.begin() + indicesBufferView.byteOffset + indicesAccessor.byteOffset;
            auto lastIndexByte = indicesBuffer.data.begin() + indicesBufferView.byteOffset + indicesBufferView.byteLength;

            if(indicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
            {
                indices32 = { firstIndexByte, lastIndexByte };
            }
            else if(indicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
            {
                indices16 = std::vector<uint16_t>(std::distance(firstIndexByte, lastIndexByte) / sizeof(uint16_t));
                memcpy(indices16.data(), &*firstIndexByte, std::distance(firstIndexByte, lastIndexByte));

            }
            else
            {
                std::cout << "Failed parsing index type" << std::endl; 
                return std::nullopt;
            }
        }
    }

    assert(positions.size() == normals.size() && "Vertex attributes don't match in length");

    for(size_t i = 0; i < positions.size(); ++i) 
    {
        vertices.emplace_back(positions[i], normals[i], glm::vec3{ 1.0f });
    }

    uint32_t indexCount = indices32.empty() ? indices16.size() : indices32.size();
    uint32_t indexBufferSize = (indices32.empty() ? sizeof(uint16_t) : sizeof(uint32_t)) * indexCount;
    void* indexData = nullptr;
    if (indices32.empty())
        indexData = indices16.data();
    else
        indexData = indices32.data();

    Mesh mesh{}; 
    mesh.vertBuf = renderer.CreateBuffer(vertices.data(), sizeof(Renderer::Vertex) * vertices.size(), wgpu::BufferUsage::Vertex, "Vertex buffer");
    mesh.indexBuf = renderer.CreateBuffer(indexData, indexBufferSize, wgpu::BufferUsage::Index, "Index buffer");
    mesh.indexFormat = indices32.empty() ? wgpu::IndexFormat::Uint16 : wgpu::IndexFormat::Uint32;
    mesh.indexCount = indexCount;

    const auto& albedoImage = model.images[model.textures[albedoIndex].source];
    mesh.albedoTexture = renderer.CreateTexture(albedoImage, albedoImage.image);

    wgpu::TextureViewDescriptor texViewDesc{};
    texViewDesc.dimension = wgpu::TextureViewDimension::e2D;
    texViewDesc.format = wgpu::TextureFormat::RGBA8Unorm;
    texViewDesc.baseArrayLayer = 0;
    texViewDesc.arrayLayerCount = 1;
    texViewDesc.mipLevelCount = 1;
    texViewDesc.baseMipLevel = 0;
    texViewDesc.aspect = wgpu::TextureAspect::All;
    mesh.albedoTextureView = mesh.albedoTexture.CreateView(&texViewDesc);

    wgpu::BindGroupEntry bgEntry{};
    bgEntry.binding = 0;
    bgEntry.textureView = mesh.albedoTextureView;

    wgpu::BindGroupDescriptor bgDesc{};
    bgDesc.layout = renderer.GetMeshBindGroupLayout();
    bgDesc.entryCount = 1;
    bgDesc.entries = &bgEntry;
    mesh.bindGroup = renderer.GetDevice().CreateBindGroup(&bgDesc);

    return std::optional<Mesh>(mesh);
}
