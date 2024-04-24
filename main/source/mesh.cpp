#include "mesh.hpp"

#include <iostream>
#include <optional>
#include <tiny_gltf.h>

#include "renderer.hpp"
#include "graphics/pbr_pass.hpp"
#include <utils.hpp>
#include "texture_loader.hpp"   

uint32_t CalculateStride(const tinygltf::Accessor& accessor)
{
    uint32_t elementSize = 0;
    switch (accessor.componentType)
    {
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
    case TINYGLTF_COMPONENT_TYPE_BYTE:
        elementSize = 1;
        break;
    case TINYGLTF_COMPONENT_TYPE_SHORT:
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        elementSize = 2;
        break;
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
    case TINYGLTF_COMPONENT_TYPE_INT:
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        elementSize = 4;
        break;
    default:
        assert(false);
    }

    switch (accessor.type)
    {
    case TINYGLTF_TYPE_MAT2:
        return 4 * elementSize;
    case TINYGLTF_TYPE_MAT3:
        return 9 * elementSize;
    case TINYGLTF_TYPE_MAT4:
        return 16 * elementSize;
    case TINYGLTF_TYPE_SCALAR:
        return elementSize;
    case TINYGLTF_TYPE_VEC2:
        return 2 * elementSize;
    case TINYGLTF_TYPE_VEC3:
        return 3 * elementSize;
    case TINYGLTF_TYPE_VEC4:
        return 4 * elementSize;
    default:
        assert(false);
    }

    return 0;
}

std::vector<float> ExtractAttribute(
    const tinygltf::Model& document, const tinygltf::Primitive& primitive, const std::string& attribute_name,
    glm::vec3* min = nullptr, glm::vec3* max = nullptr
)
{

    std::vector<float> data;
    if (primitive.attributes.count(attribute_name))
    {

        const auto& accessor = document.accessors[primitive.attributes.find(attribute_name)->second];
        const auto& view = document.bufferViews[accessor.bufferView];
        const auto& buffer = document.buffers[view.buffer];

        data.resize(accessor.count * CalculateStride(accessor) / 4);
        memcpy(data.data(), &buffer.data.at(view.byteOffset + accessor.byteOffset), accessor.count * CalculateStride(accessor));

        if (min)
        {
            min->x = (float)accessor.minValues[0]; min->y = (float)accessor.minValues[1]; min->z = (float)accessor.minValues[2];
        }

        if (max)
        {
            max->x = (float)accessor.maxValues[0]; max->y = (float)accessor.maxValues[1]; max->z = (float)accessor.maxValues[2];
        }
    }

    return data;
}

glm::mat3x3 ComputeTBN(const PBRPass::Vertex corners[3], const glm::vec3& expectedNormal)
{
    glm::vec3 ePos1 = corners[1].position - corners[0].position;
    glm::vec3 ePos2 = corners[2].position - corners[0].position;

    glm::vec2 eUV1 = corners[1].uv - corners[0].uv;
    glm::vec2 eUV2 = corners[2].uv - corners[0].uv;

    glm::vec3 tangent = glm::normalize(ePos1 * eUV2.y - ePos2 * eUV1.y);
    glm::vec3 bitangent = glm::normalize(ePos2 * eUV1.x - ePos1 * eUV2.x);
    glm::vec3 normal = glm::cross(tangent, bitangent);

    if (glm::dot(normal, expectedNormal) < 0.0f)
    {
        tangent = -tangent;
        bitangent = -bitangent;
        normal = -normal;
    }

    normal = expectedNormal;
    tangent = glm::normalize(tangent - normal * glm::dot(tangent, normal));
    bitangent = glm::cross(normal, tangent);

    return glm::mat3x3(tangent, bitangent, normal);
}

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
    std::vector<glm::vec2> uvs;
    std::vector<PBRPass::Vertex> vertices{};
    std::vector<uint32_t> indices32{};
    std::vector<uint16_t> indices16{};

    int32_t albedoIndex;
    int32_t normalIndex;
    int32_t metallicIndex;
    int32_t roughnessIndex;
    int32_t aoIndex;
    int32_t emissiveIndex;
    Material material{};

    for(size_t i = 0; i < model.meshes.size(); ++i)
    {
        tinygltf::Primitive primitive = model.meshes[i].primitives[0];

        tinygltf::Material gltfMaterial = model.materials[primitive.material];
        albedoIndex = gltfMaterial.pbrMetallicRoughness.baseColorTexture.index;
        normalIndex = gltfMaterial.normalTexture.index;
        metallicIndex = gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index;
        roughnessIndex = gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index;
        aoIndex = gltfMaterial.occlusionTexture.index;
        emissiveIndex = gltfMaterial.emissiveTexture.index;
        material = {
            glm::vec3{ gltfMaterial.pbrMetallicRoughness.baseColorFactor[0], gltfMaterial.pbrMetallicRoughness.baseColorFactor[1], gltfMaterial.pbrMetallicRoughness.baseColorFactor[2] },
            static_cast<float>(gltfMaterial.pbrMetallicRoughness.metallicFactor),
            static_cast<float>(gltfMaterial.pbrMetallicRoughness.roughnessFactor),
            1.0f,
            static_cast<float>(gltfMaterial.emissiveFactor[0])
        };

        // Get positions.
        { 
            auto values = ExtractAttribute(model, primitive, "POSITION", nullptr, nullptr);
            positions.assign(reinterpret_cast<glm::vec3*>(values.data()), reinterpret_cast<glm::vec3*>(values.data()) + values.size() / 3);
        }

        // Get normals.
        {
            auto values = ExtractAttribute(model, primitive, "NORMAL", nullptr, nullptr);
            normals.assign(reinterpret_cast<glm::vec3*>(values.data()), reinterpret_cast<glm::vec3*>(values.data()) + values.size() / 3);
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

        // Get uv.
        {
            glm::vec3 min, max;
            auto values = ExtractAttribute(model, primitive, "TEXCOORD_0", &min, &max);
            uvs.assign(reinterpret_cast<glm::vec2*>(values.data()), reinterpret_cast<glm::vec2*>(values.data()) + values.size() / 2);

            for(auto& uv : uvs)
            {
                uv.x = (uv.x - min.x) / (max.x - min.x);
                uv.y = (uv.y - min.y) / (max.y - min.y);

            }
        }
    }

    assert(positions.size() == uvs.size() && "Vertex attributes don't match in length");

    for(size_t i = 0; i < positions.size(); ++i)  
    {   
        vertices.emplace_back(positions[i], normals.empty() ? glm::vec3{ 0.0f } : normals[i], glm::vec3{ 0.0f }, glm::vec3{ 0.0f }, uvs[i]);
    }

    uint32_t indexCount = indices32.empty() ? indices16.size() : indices32.size();
    // add tangents and bitangents
    for(size_t i = 0; i < indexCount; i += 3)
    {
        PBRPass::Vertex corners[3] = { 
            vertices[indices32.empty() ? indices16[i] : indices32[i]], 
            vertices[indices32.empty() ? indices16[i + 1] : indices32[i + 1]], 
            vertices[indices32.empty() ? indices16[i + 2] : indices32[i + 2]] 
        };
        glm::mat3x3 TBN = ComputeTBN(corners, corners[0].normal); 

        for(size_t j = 0; j < 3; ++j)
        {
            vertices[indices32.empty() ? indices16[i + j] : indices32[i + j]].tangent = TBN[0];
            vertices[indices32.empty() ? indices16[i + j] : indices32[i + j]].bitangent = TBN[1];
        }
    }

    uint32_t indexBufferSize = (indices32.empty() ? sizeof(uint16_t) : sizeof(uint32_t)) * indexCount;
    void* indexData = nullptr;
    if (indices32.empty())
        indexData = indices16.data();
    else
        indexData = indices32.data();

    Mesh mesh{}; 
    mesh.vertBuf = renderer.CreateBuffer(vertices.data(), sizeof(PBRPass::Vertex) * vertices.size(), wgpu::BufferUsage::Vertex, "Vertex buffer");
    mesh.indexBuf = renderer.CreateBuffer(indexData, indexBufferSize, wgpu::BufferUsage::Index, "Index buffer");
    mesh.indexFormat = indices32.empty() ? wgpu::IndexFormat::Uint16 : wgpu::IndexFormat::Uint32;
    mesh.indexCount = indexCount;

    mesh.materialBuf = renderer.CreateBuffer(&material, sizeof(Material), wgpu::BufferUsage::Uniform, "Material buffer");

    const auto& albedoImage = model.images[model.textures[albedoIndex].source];
    const uint32_t mipLevelCount = bitWidth(std::max(albedoImage.width, albedoImage.height));
    mesh.albedoTexture = renderer.GetTextureLoader().LoadTexture(albedoImage.image, albedoImage.width, albedoImage.height, wgpu::TextureFormat::BGRA8Unorm, mipLevelCount, albedoImage.name.c_str());

    const auto& normalImage = model.images[model.textures[normalIndex].source];
    mesh.normalTexture = renderer.GetTextureLoader().LoadTexture(normalImage.image, normalImage.width, normalImage.height, wgpu::TextureFormat::BGRA8Unorm, mipLevelCount, normalImage.name.c_str());

    const auto& metallicImage = model.images[model.textures[metallicIndex].source];
    mesh.metallicTexture = renderer.GetTextureLoader().LoadTexture(metallicImage.image, metallicImage.width, metallicImage.height, wgpu::TextureFormat::BGRA8Unorm, mipLevelCount, metallicImage.name.c_str());

    const auto& roughnessImage = model.images[model.textures[roughnessIndex].source];
    mesh.roughnessTexture = renderer.GetTextureLoader().LoadTexture(roughnessImage.image, roughnessImage.width, roughnessImage.height, wgpu::TextureFormat::BGRA8Unorm, mipLevelCount, roughnessImage.name.c_str());

    const auto& aoImage = model.images[model.textures[aoIndex].source];
    mesh.aoTexture = renderer.GetTextureLoader().LoadTexture(aoImage.image, aoImage.width, aoImage.height, wgpu::TextureFormat::BGRA8Unorm, mipLevelCount, aoImage.name.c_str());

    const auto& emissiveImage = model.images[model.textures[emissiveIndex].source];
    mesh.emissiveTexture = renderer.GetTextureLoader().LoadTexture(emissiveImage.image, emissiveImage.width, emissiveImage.height, wgpu::TextureFormat::BGRA8Unorm, mipLevelCount, emissiveImage.name.c_str());


    wgpu::TextureViewDescriptor texViewDesc{};
    texViewDesc.dimension = wgpu::TextureViewDimension::e2D;
    texViewDesc.format = wgpu::TextureFormat::RGBA8Unorm;
    texViewDesc.baseArrayLayer = 0;
    texViewDesc.arrayLayerCount = 1;
    texViewDesc.mipLevelCount = mipLevelCount;
    texViewDesc.baseMipLevel = 0; 
    texViewDesc.aspect = wgpu::TextureAspect::All;
    mesh.albedoTextureView = mesh.albedoTexture.CreateView(&texViewDesc);
    mesh.emissiveTextureView = mesh.emissiveTexture.CreateView(&texViewDesc);
    mesh.normalTextureView = mesh.normalTexture.CreateView(&texViewDesc);
    mesh.metallicTextureView = mesh.metallicTexture.CreateView(&texViewDesc);
    mesh.roughnessTextureView = mesh.roughnessTexture.CreateView(&texViewDesc);
    mesh.aoTextureView = mesh.aoTexture.CreateView(&texViewDesc);

    wgpu::SamplerDescriptor samplerDesc{};
    samplerDesc.addressModeU = wgpu::AddressMode::ClampToEdge;
    samplerDesc.addressModeV = wgpu::AddressMode::ClampToEdge;
    samplerDesc.addressModeW = wgpu::AddressMode::ClampToEdge;
    samplerDesc.minFilter = wgpu::FilterMode::Linear;
    samplerDesc.magFilter = wgpu::FilterMode::Linear;
    samplerDesc.mipmapFilter = wgpu::MipmapFilterMode::Linear;
    samplerDesc.lodMinClamp = 0.0f;
    samplerDesc.lodMaxClamp = static_cast<float>(mipLevelCount);
    samplerDesc.compare = wgpu::CompareFunction::Undefined;
    samplerDesc.maxAnisotropy = 1;
    mesh.sampler = renderer.Device().CreateSampler(&samplerDesc);

    std::array<wgpu::BindGroupEntry, 8> bgEntries{};
    bgEntries[0].binding = 0;
    bgEntries[0].buffer = mesh.materialBuf;
    
    bgEntries[1].binding = 1;
    bgEntries[1].sampler = mesh.sampler;

    bgEntries[2].binding = 2;
    bgEntries[2].textureView = mesh.albedoTextureView;

    bgEntries[3].binding = 3;
    bgEntries[3].textureView = mesh.normalTextureView;

    bgEntries[4].binding = 4;
    bgEntries[4].textureView = mesh.metallicTextureView;

    bgEntries[5].binding = 5;
    bgEntries[5].textureView = mesh.roughnessTextureView;

    bgEntries[6].binding = 6;
    bgEntries[6].textureView = mesh.aoTextureView;

    bgEntries[7].binding = 7;
    bgEntries[7].textureView = mesh.emissiveTextureView;


    wgpu::BindGroupDescriptor bgDesc{};
    bgDesc.layout = renderer.PBRRenderPass().PBRBindGroupLayout();
    bgDesc.entryCount = bgEntries.size();
    bgDesc.entries = bgEntries.data();
    mesh.bindGroup = renderer.Device().CreateBindGroup(&bgDesc);

    return std::optional<Mesh>(mesh);
}
