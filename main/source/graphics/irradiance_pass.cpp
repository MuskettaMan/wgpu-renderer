#include "graphics/irradiance_pass.hpp"
#include "stb_image.h"
#include "renderer.hpp"
#include <utils.hpp>

IrradiancePass::IrradiancePass(Renderer& renderer, const wgpu::TextureView& skyboxView) : 
    RenderPass(renderer, wgpu::TextureFormat::RGBA16Float), 
    _uniformStride(ceilToNextMultiple(sizeof(_currentFace), 256)), 
    _skyboxView(skyboxView)
{
    std::array<wgpu::BindGroupLayoutEntry, 3> bgLayoutEntries{};
    bgLayoutEntries[0].binding = 0;
    bgLayoutEntries[0].visibility = wgpu::ShaderStage::Fragment;
    bgLayoutEntries[0].texture.sampleType = wgpu::TextureSampleType::Float;
    bgLayoutEntries[0].texture.viewDimension = wgpu::TextureViewDimension::Cube;
    bgLayoutEntries[0].texture.multisampled = false;

    bgLayoutEntries[1].binding = 1;
    bgLayoutEntries[1].visibility = wgpu::ShaderStage::Fragment;
    bgLayoutEntries[1].sampler.type = wgpu::SamplerBindingType::Filtering;

    bgLayoutEntries[2].binding = 2;
    bgLayoutEntries[2].visibility = wgpu::ShaderStage::Fragment;
    bgLayoutEntries[2].buffer.type = wgpu::BufferBindingType::Uniform;
    bgLayoutEntries[2].buffer.minBindingSize = sizeof(_currentFace);
    bgLayoutEntries[2].buffer.hasDynamicOffset = true;


    wgpu::BindGroupLayoutDescriptor bgLayoutDesc{};
    bgLayoutDesc.label = "Irradiance pass bind group layout";
    bgLayoutDesc.entryCount = bgLayoutEntries.size();
    bgLayoutDesc.entries = bgLayoutEntries.data();

    _bindGroupLayout = _renderer.Device().CreateBindGroupLayout(&bgLayoutDesc);

    wgpu::PipelineLayoutDescriptor bgPipelineLayoutDesc{};
    bgPipelineLayoutDesc.bindGroupLayoutCount = 1;
    bgPipelineLayoutDesc.bindGroupLayouts = &_bindGroupLayout;

    _pipelineLayout = _renderer.Device().CreatePipelineLayout(&bgPipelineLayoutDesc);

    wgpu::ShaderModule shader = _renderer.CreateShader("assets/shaders/irradiance-convolution.wgsl", "Irradiance convolution shader module");
    
    wgpu::RenderPipelineDescriptor renderPipelineDesc{};
    renderPipelineDesc.label = "Irradiance convolution pipeline";
    renderPipelineDesc.layout = _pipelineLayout;
    renderPipelineDesc.vertex.module = shader;
    renderPipelineDesc.vertex.entryPoint = "vs_main";
    wgpu::ColorTargetState colorTarget{};
    colorTarget.format = _renderFormat;
    colorTarget.blend = nullptr;
    colorTarget.writeMask = wgpu::ColorWriteMask::All;

    wgpu::FragmentState fragmentState{};
    fragmentState.module = shader;
    fragmentState.entryPoint = "fs_main";
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    renderPipelineDesc.fragment = &fragmentState;
    renderPipelineDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;

    _renderPipeline = _renderer.Device().CreateRenderPipeline(&renderPipelineDesc);

    _faceUniformBuffer = _renderer.CreateBuffer(&_faceUniformBuffer, _uniformStride * 6, wgpu::BufferUsage::Uniform, "Irradiance convolution face uniform buffer");

    wgpu::SamplerDescriptor hdrSamplerDesc{};
    hdrSamplerDesc.label = "HDR sampler";
    hdrSamplerDesc.addressModeU = wgpu::AddressMode::ClampToEdge;
    hdrSamplerDesc.addressModeV = wgpu::AddressMode::ClampToEdge;
    hdrSamplerDesc.addressModeW = wgpu::AddressMode::ClampToEdge;
    hdrSamplerDesc.minFilter = wgpu::FilterMode::Linear;
    hdrSamplerDesc.magFilter = wgpu::FilterMode::Linear;
    hdrSamplerDesc.mipmapFilter = wgpu::MipmapFilterMode::Linear;
    _cubemapSampler = _renderer.Device().CreateSampler(&hdrSamplerDesc);
}

IrradiancePass::~IrradiancePass()
{
    _faceUniformBuffer.Destroy();
}

void IrradiancePass::Render(const wgpu::CommandEncoder& encoder, const wgpu::TextureView& renderTarget, std::shared_ptr<const wgpu::TextureView> resolveTarget)
{
    std::array<wgpu::BindGroupEntry, 3> bgEntriesHDR{};
    bgEntriesHDR[0].binding = 0;
    bgEntriesHDR[0].textureView = _skyboxView;
    bgEntriesHDR[1].binding = 1;
    bgEntriesHDR[1].sampler = _cubemapSampler;
    bgEntriesHDR[2].binding = 2;
    bgEntriesHDR[2].buffer = _faceUniformBuffer;
    bgEntriesHDR[2].size = sizeof(_currentFace);

    wgpu::BindGroupDescriptor hdrBindgroupDesc{};
    hdrBindgroupDesc.layout = _bindGroupLayout;
    hdrBindgroupDesc.entryCount = bgEntriesHDR.size();
    hdrBindgroupDesc.entries = bgEntriesHDR.data();

    _bindGroup = _renderer.Device().CreateBindGroup(&hdrBindgroupDesc);

    wgpu::RenderPassColorAttachment colorDescTonemap{};
    colorDescTonemap.view = renderTarget;
    colorDescTonemap.loadOp = wgpu::LoadOp::Clear;
    colorDescTonemap.storeOp = wgpu::StoreOp::Store;
    colorDescTonemap.resolveTarget = nullptr;

    wgpu::RenderPassDescriptor hdrPassDesc{};
    hdrPassDesc.label = "HDR to cubemap render pass";
    hdrPassDesc.colorAttachmentCount = 1;
    hdrPassDesc.colorAttachments = &colorDescTonemap;
    hdrPassDesc.depthStencilAttachment = nullptr;

    wgpu::RenderPassEncoder hdrPass = encoder.BeginRenderPass(&hdrPassDesc);

    hdrPass.SetPipeline(_renderPipeline);

    uint32_t dynamicOffset{ _currentFace * _uniformStride };
    _renderer.Queue().WriteBuffer(_faceUniformBuffer, dynamicOffset, &_currentFace, sizeof(_currentFace));

    hdrPass.SetBindGroup(0, _bindGroup, 1, &dynamicOffset);
    hdrPass.Draw(3, 1, 0, 0);

    hdrPass.End();
}
