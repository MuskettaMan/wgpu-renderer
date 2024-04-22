#pragma once
#include <webgpu/webgpu_cpp.h>
#include <vec3.hpp>

#include "aliases.hpp"

class Renderer;

struct Material{
    glm::vec3 albedoFactor;
    float metallicFactor;
    float roughnessFactor;
    float aoFactor;
    float normalFactor;
    float emissiveFactor;
};

struct Mesh
{
    wgpu::Buffer vertBuf;
    wgpu::Buffer indexBuf;
    wgpu::IndexFormat indexFormat;
    uint32_t indexCount;

    wgpu::Buffer materialBuf;

    wgpu::Sampler sampler;
    wgpu::BindGroup bindGroup;

    wgpu::Texture albedoTexture;
    wgpu::TextureView albedoTextureView;

    wgpu::Texture normalTexture;
    wgpu::TextureView normalTextureView;

    wgpu::Texture metallicTexture;
    wgpu::TextureView metallicTextureView;

    wgpu::Texture roughnessTexture;
    wgpu::TextureView roughnessTextureView;

    wgpu::Texture aoTexture;
    wgpu::TextureView aoTextureView;

    wgpu::Texture emissiveTexture;
    wgpu::TextureView emissiveTextureView;

    static std::optional<Mesh> CreateMesh(const std::string& path, Renderer& renderer);
};


