#pragma once

#include "render_pass.hpp"
#include <webgpu/webgpu_cpp.h>
#include <glm.hpp>

class SkyboxPass : public RenderPass
{
public:
    struct Instance
    {
        glm::mat4 model;
        float exposure{ 3.0f };
        float padding[3];
    };

    SkyboxPass(Renderer& renderer);
    virtual ~SkyboxPass();

    virtual void Render(const wgpu::CommandEncoder& encoder, const wgpu::TextureView& renderTarget, std::shared_ptr<const wgpu::TextureView> resolveTarget = nullptr) override;

    void SetExposure(float exposure) { _instance.exposure = exposure; }
    float GetExposure() const { return _instance.exposure; }

    const wgpu::TextureView& SkyboxView() const { return _skyboxView; }
    const wgpu::TextureView SkyboxView(uint32_t face) const 
    {  
        assert(face < 6 && "Invalid face index");

        wgpu::TextureViewDescriptor viewDesc{};
        viewDesc.dimension = wgpu::TextureViewDimension::e2D;
        viewDesc.baseMipLevel = 0;
        viewDesc.mipLevelCount = 1;
        viewDesc.baseArrayLayer = face;
        viewDesc.arrayLayerCount = 1;
        return _skyboxTexture.CreateView(&viewDesc);
    }

private:
    wgpu::Texture _skyboxTexture;
    wgpu::TextureView _skyboxView;
    wgpu::Sampler _skyboxSampler;
    wgpu::Buffer _vertexBuffer;
    wgpu::BindGroupLayout _skyboxBGL;
    wgpu::BindGroup _skyboxBindGroup;
    wgpu::ShaderModule _skyboxShader;
    wgpu::RenderPipeline _skyboxPipeline;
    wgpu::Buffer _instanceBuffer;
    Instance _instance;
};
