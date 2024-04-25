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
    };

    SkyboxPass(Renderer& renderer);
    virtual ~SkyboxPass();

    virtual void Render(const wgpu::CommandEncoder& encoder, const wgpu::TextureView& renderTarget, std::shared_ptr<const wgpu::TextureView> resolveTarget = nullptr) override;

private:
    wgpu::Texture _skyboxTexture;
    wgpu::TextureView _skyboxView;
    wgpu::Sampler _skyboxSampler;
    wgpu::Buffer _vertexBuffer;
    wgpu::BindGroupLayout _skyboxBindGroupLayout;
    wgpu::BindGroup _skyboxBindGroup;
    wgpu::ShaderModule _skyboxShader;
    wgpu::RenderPipeline _skyboxPipeline;
    wgpu::Buffer _instanceBuffer;
    Instance _instance;
};
