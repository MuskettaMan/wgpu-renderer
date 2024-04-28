#pragma once

#include "render_pass.hpp"

class HDRIConversionPass : public RenderPass
{
public:
    HDRIConversionPass(Renderer& renderer);
    virtual ~HDRIConversionPass() override;

    virtual void Render(const wgpu::CommandEncoder& encoder, const wgpu::TextureView& renderTarget, std::shared_ptr<const wgpu::TextureView> resolveTarget = nullptr) override;

    void SetFace(uint32_t face) { _currentFace = face; }

private:
    int32_t _hdrWidth;
    int32_t _hdrHeight;
    int32_t _hdrChannels;
    float* _hdriData;
    wgpu::PipelineLayout _pipelineLayout;
    wgpu::RenderPipeline _renderPipeline;
    wgpu::BindGroupLayout _bindGroupLayout;
    wgpu::BindGroup _hdrBindGroup;
    wgpu::Texture _hdrTexture;
    wgpu::TextureView _cubemapView;
    wgpu::Sampler _hdrSampler;
    wgpu::Buffer _faceUniformBuffer;
    wgpu::TextureView _hdrView;
    uint32_t _uniformStride;
    uint32_t _currentFace{ 0 };
};
