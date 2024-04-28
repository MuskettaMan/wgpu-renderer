#include "graphics/hdr_pass.hpp"
#include "renderer.hpp"
#include <iostream>

HDRPass::HDRPass(Renderer& renderer) : RenderPass(renderer, renderer.SwapChainFormat())
{
    wgpu::ColorTargetState colorTargetHDR{};
    colorTargetHDR.format = _renderer.SwapChainFormat();
    colorTargetHDR.blend = nullptr;

    wgpu::ShaderModule hdrShader = _renderer.CreateShader("assets/shaders/hdr.wgsl", "HDR Fragment shader");
    wgpu::FragmentState hdrFragment{};
    hdrFragment.module = hdrShader;
    hdrFragment.entryPoint = "fs_main";
    hdrFragment.targetCount = 1;
    hdrFragment.targets = &colorTargetHDR;
    hdrFragment.constantCount = 0;
    hdrFragment.constants = nullptr;

    wgpu::RenderPipelineDescriptor rpHDRDesc{};
    rpHDRDesc.label = "HDR render pipeline";
    rpHDRDesc.fragment = &hdrFragment;
    rpHDRDesc.vertex.bufferCount = 0;
    rpHDRDesc.vertex.buffers = nullptr;
    rpHDRDesc.vertex.entryPoint = "vs_main";
    rpHDRDesc.vertex.module = hdrShader;

    rpHDRDesc.multisample.count = 1;
    rpHDRDesc.multisample.mask = 0xFF'FF'FF'FF;

    rpHDRDesc.primitive.frontFace = wgpu::FrontFace::CCW;
    rpHDRDesc.primitive.cullMode = wgpu::CullMode::None;
    rpHDRDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    rpHDRDesc.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined;

    rpHDRDesc.depthStencil = nullptr;

    std::array<wgpu::BindGroupLayoutEntry, 2> bgLayoutHDREntries{};
    bgLayoutHDREntries[0].binding = 0;
    bgLayoutHDREntries[0].visibility = wgpu::ShaderStage::Fragment;
    bgLayoutHDREntries[0].texture.sampleType = wgpu::TextureSampleType::Float;
    bgLayoutHDREntries[0].texture.viewDimension = wgpu::TextureViewDimension::e2D;
    bgLayoutHDREntries[1].binding = 1;
    bgLayoutHDREntries[1].visibility = wgpu::ShaderStage::Fragment;
    bgLayoutHDREntries[1].sampler.type = wgpu::SamplerBindingType::Filtering;

    wgpu::BindGroupLayoutDescriptor bgLayoutHDRDesc{};
    bgLayoutHDRDesc.entryCount = bgLayoutHDREntries.size();
    bgLayoutHDRDesc.entries = bgLayoutHDREntries.data();

    _hdrBindGroupLayout = _renderer.Device().CreateBindGroupLayout(&bgLayoutHDRDesc);

    wgpu::PipelineLayoutDescriptor hdrPipelineLayoutDesc{};
    hdrPipelineLayoutDesc.label = "HDR pipeline layout";
    hdrPipelineLayoutDesc.bindGroupLayoutCount = 1;
    hdrPipelineLayoutDesc.bindGroupLayouts = &_hdrBindGroupLayout;

    wgpu::PipelineLayout hdrPipelineLayout = _renderer.Device().CreatePipelineLayout(&hdrPipelineLayoutDesc);

    rpHDRDesc.layout = hdrPipelineLayout;

    wgpu::SamplerDescriptor hdrSamplerDesc{};
    hdrSamplerDesc.addressModeU = wgpu::AddressMode::ClampToEdge;
    hdrSamplerDesc.addressModeV = wgpu::AddressMode::ClampToEdge;
    hdrSamplerDesc.addressModeW = wgpu::AddressMode::ClampToEdge;
    hdrSamplerDesc.minFilter = wgpu::FilterMode::Linear;
    hdrSamplerDesc.magFilter = wgpu::FilterMode::Linear;
    hdrSamplerDesc.mipmapFilter = wgpu::MipmapFilterMode::Linear;
    hdrSamplerDesc.lodMinClamp = 0.0f;
    hdrSamplerDesc.lodMaxClamp = 1000.0f;
    hdrSamplerDesc.compare = wgpu::CompareFunction::Undefined;

    _hdrSampler = _renderer.Device().CreateSampler(&hdrSamplerDesc);

    _renderPipeline = _renderer.Device().CreateRenderPipeline(&rpHDRDesc);

    UpdateHDRView(_renderer.HDRView());
}

void HDRPass::Render(const wgpu::CommandEncoder& encoder, const wgpu::TextureView& renderTarget, std::shared_ptr<const wgpu::TextureView> resolveTarget)
{
    wgpu::RenderPassColorAttachment colorDescTonemap{};
    colorDescTonemap.view = renderTarget;
    colorDescTonemap.loadOp = wgpu::LoadOp::Clear;
    colorDescTonemap.storeOp = wgpu::StoreOp::Store;
    colorDescTonemap.resolveTarget = nullptr;


    wgpu::RenderPassDescriptor hdrPassDesc{};
    hdrPassDesc.label = "HDR render pass";
    hdrPassDesc.colorAttachmentCount = 1;
    hdrPassDesc.colorAttachments = &colorDescTonemap;
    hdrPassDesc.depthStencilAttachment = nullptr;

    wgpu::RenderPassEncoder hdrPass = encoder.BeginRenderPass(&hdrPassDesc);
    
    hdrPass.SetPipeline(_renderPipeline);
    hdrPass.SetBindGroup(0, _hdrBindGroup, 0, nullptr);
    hdrPass.Draw(3, 1, 0, 0);
    hdrPass.End();
}

void HDRPass::UpdateHDRView(const wgpu::TextureView& hdrView)
{
    std::array<wgpu::BindGroupEntry, 2> bgEntriesHDR{};
    bgEntriesHDR[0].binding = 0;
    bgEntriesHDR[0].textureView = hdrView;
    bgEntriesHDR[1].binding = 1;
    bgEntriesHDR[1].sampler = _hdrSampler;

    wgpu::BindGroupDescriptor hdrBindgroupDesc{};
    hdrBindgroupDesc.layout = _hdrBindGroupLayout;
    hdrBindgroupDesc.entryCount = bgEntriesHDR.size();
    hdrBindgroupDesc.entries = bgEntriesHDR.data();

    _hdrBindGroup = _renderer.Device().CreateBindGroup(&hdrBindgroupDesc);
}
