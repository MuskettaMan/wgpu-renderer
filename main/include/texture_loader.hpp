#pragma once
#include <webgpu/webgpu_cpp.h>

class Renderer;

class TextureLoader
{
public:
    TextureLoader(const Renderer& renderer);

    wgpu::Texture LoadTexture(const std::string& path, const char* label = nullptr) const;
    wgpu::Texture LoadTexture(const std::vector<uint8_t>& data, uint32_t width, uint32_t height, wgpu::TextureFormat format, uint32_t mipLevels = 1, const char* label = nullptr) const;

private:
    const Renderer& _renderer;
    wgpu::BindGroupLayout _bgLayout;
    wgpu::ComputePipeline _computePipeline;

};
