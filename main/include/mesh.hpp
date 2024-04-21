#pragma once
#include <webgpu/webgpu_cpp.h>

#include "aliases.hpp"

class Renderer;

struct Mesh
{
    wgpu::Buffer vertBuf;
    wgpu::Buffer indexBuf;
    wgpu::IndexFormat indexFormat;
    uint32_t indexCount;

    wgpu::Texture albedoTexture;
    wgpu::TextureView albedoTextureView;

    wgpu::Texture normalTexture;
    wgpu::TextureView normalTextureView;

    wgpu::Sampler sampler;
    wgpu::BindGroup bindGroup;

    static std::optional<Mesh> CreateMesh(const std::string& path, Renderer& renderer);
};


