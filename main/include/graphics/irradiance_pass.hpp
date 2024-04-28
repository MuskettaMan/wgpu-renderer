#pragma once

#include "render_pass.hpp"

class IrradiancePass : public RenderPass
{
public:
    IrradiancePass(Renderer& renderer, const wgpu::TextureView& skyboxView);
    virtual ~IrradiancePass() override;

    virtual void Render(const wgpu::CommandEncoder& encoder, const wgpu::TextureView& renderTarget, std::shared_ptr<const wgpu::TextureView> resolveTarget = nullptr) override;

    void SetFace(uint32_t face) { _currentFace = face; }

private:
    wgpu::PipelineLayout _pipelineLayout;
    wgpu::RenderPipeline _renderPipeline;
    wgpu::BindGroupLayout _bindGroupLayout;
    wgpu::BindGroup _bindGroup;
    wgpu::Sampler _cubemapSampler;
    wgpu::Buffer _faceUniformBuffer;

    const wgpu::TextureView& _skyboxView;

    uint32_t _uniformStride;
    uint32_t _currentFace{ 0 };
};
