#pragma once
#include "graphics/render_pass.hpp"

class HDRPass : public RenderPass
{
public:
    HDRPass(Renderer& renderer);
    virtual void Render(const wgpu::CommandEncoder& encoder, const wgpu::TextureView& renderTarget, std::shared_ptr<const wgpu::TextureView> resolveTarget = nullptr) override;

    void UpdateHDRView(const wgpu::TextureView& hdrView);

private:
    wgpu::Sampler _hdrSampler;
    wgpu::RenderPipeline _renderPipeline;
    wgpu::BindGroupLayout _hdrBindGroupLayout;
    wgpu::BindGroup _hdrBindGroup;
};
