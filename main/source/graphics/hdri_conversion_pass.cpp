#include "graphics/hdri_conversion_pass.hpp"
#include "stb_image.h"
#include "renderer.hpp"
#include <utils.hpp>

HDRIConversionPass::HDRIConversionPass(Renderer& renderer) : RenderPass(renderer, wgpu::TextureFormat::RGBA16Float), _uniformStride(ceilToNextMultiple(sizeof(_currentFace), 256))
{
    std::array<wgpu::BindGroupLayoutEntry, 3> bgLayoutEntries{};
    bgLayoutEntries[0].binding = 0;
    bgLayoutEntries[0].visibility = wgpu::ShaderStage::Fragment;
    bgLayoutEntries[0].texture.sampleType = wgpu::TextureSampleType::Float;
    bgLayoutEntries[0].texture.viewDimension = wgpu::TextureViewDimension::e2D;
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
    bgLayoutDesc.label = "HDRI to cubemap bind group layout";
    bgLayoutDesc.entryCount = bgLayoutEntries.size();
    bgLayoutDesc.entries = bgLayoutEntries.data();

    _bindGroupLayout = _renderer.Device().CreateBindGroupLayout(&bgLayoutDesc);

    wgpu::PipelineLayoutDescriptor bgPipelineLayoutDesc{};
    bgPipelineLayoutDesc.bindGroupLayoutCount = 1;
    bgPipelineLayoutDesc.bindGroupLayouts = &_bindGroupLayout;
     
    _pipelineLayout = _renderer.Device().CreatePipelineLayout(&bgPipelineLayoutDesc);

    wgpu::ShaderModule shader = _renderer.CreateShader("assets/shaders/hdri-to-cubemap.wgsl", "HDRI to cubemap shader module");

    wgpu::RenderPipelineDescriptor renderPipelineDesc{};
    renderPipelineDesc.label = "HDRI to cubemap pipeline";
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

    _faceUniformBuffer = _renderer.CreateBuffer(&_faceUniformBuffer, _uniformStride * 6, wgpu::BufferUsage::Uniform, "HDRI to cubemap face uniform buffer");

    _hdriData = stbi_loadf("assets/textures/symmetrical_garden_02_4k.hdr", &_hdrWidth, &_hdrHeight, &_hdrChannels, STBI_rgb_alpha);

    if (!_hdriData)
    {
        throw std::runtime_error("Failed to load HDR image.");
    }

    wgpu::TextureDescriptor hdrTextureDesc{};
    hdrTextureDesc.label = "HDR texture"; // TODO: Add path in label.
    hdrTextureDesc.dimension = wgpu::TextureDimension::e2D;
    hdrTextureDesc.size.width = _hdrWidth;
    hdrTextureDesc.size.height = _hdrHeight;
    hdrTextureDesc.size.depthOrArrayLayers = 1;
    hdrTextureDesc.sampleCount = 1; 
    hdrTextureDesc.format = wgpu::TextureFormat::RGBA16Float;
    hdrTextureDesc.mipLevelCount = 1;
    hdrTextureDesc.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::StorageBinding;

    _hdrTexture = _renderer.Device().CreateTexture(&hdrTextureDesc);

    wgpu::ImageCopyTexture hdrTextureCopy{};
    hdrTextureCopy.texture = _hdrTexture;
    hdrTextureCopy.aspect = wgpu::TextureAspect::All;
    hdrTextureCopy.mipLevel = 0;
    hdrTextureCopy.origin = { 0, 0, 0 };

    wgpu::TextureDataLayout hdrDataLayout{};
    hdrDataLayout.offset = 0;
    hdrDataLayout.bytesPerRow = _hdrWidth * 4 * sizeof(uint16_t);
    hdrDataLayout.rowsPerImage = _hdrHeight;

    wgpu::Extent3D hdrSize{};
    hdrSize.width = _hdrWidth;
    hdrSize.height = _hdrHeight;
    hdrSize.depthOrArrayLayers = 1;

    std::vector<float> hdrData{};
    hdrData.reserve(_hdrWidth * _hdrHeight * 4);
    hdrData.assign(_hdriData, _hdriData + _hdrWidth * _hdrHeight * 4);
    auto hdrData16 = convertFloat32ToFloat16(hdrData);

    _renderer.Queue().WriteTexture(&hdrTextureCopy, hdrData16.data(), _hdrWidth * _hdrHeight * 4 * sizeof(uint16_t), &hdrDataLayout, &hdrSize);

    wgpu::TextureViewDescriptor hdrViewDesc{};
    hdrViewDesc.label = "HDR texture view";
    hdrViewDesc.dimension = wgpu::TextureViewDimension::e2D;
    hdrViewDesc.format = hdrTextureDesc.format;
    hdrViewDesc.baseMipLevel = 0;
    hdrViewDesc.mipLevelCount = 1; 
    hdrViewDesc.baseArrayLayer = 0;
    hdrViewDesc.arrayLayerCount = 1;
    hdrViewDesc.aspect = wgpu::TextureAspect::All; 

    _hdrView = _hdrTexture.CreateView(&hdrViewDesc);

    wgpu::SamplerDescriptor hdrSamplerDesc{};
    hdrSamplerDesc.label = "HDR sampler";
    hdrSamplerDesc.addressModeU = wgpu::AddressMode::ClampToEdge;
    hdrSamplerDesc.addressModeV = wgpu::AddressMode::ClampToEdge;
    hdrSamplerDesc.addressModeW = wgpu::AddressMode::ClampToEdge;
    hdrSamplerDesc.minFilter = wgpu::FilterMode::Linear;
    hdrSamplerDesc.magFilter = wgpu::FilterMode::Linear;
    hdrSamplerDesc.mipmapFilter = wgpu::MipmapFilterMode::Linear;
    _hdrSampler = _renderer.Device().CreateSampler(&hdrSamplerDesc);
}

HDRIConversionPass::~HDRIConversionPass()
{ 
    _hdrTexture.Destroy();
    _faceUniformBuffer.Destroy();
    stbi_image_free(_hdriData);
}

void HDRIConversionPass::Render(const wgpu::CommandEncoder& encoder, const wgpu::TextureView& renderTarget, std::shared_ptr<const wgpu::TextureView> resolveTarget)
{
    std::array<wgpu::BindGroupEntry, 3> bgEntriesHDR{};
    bgEntriesHDR[0].binding = 0;
    bgEntriesHDR[0].textureView = _hdrView;
    bgEntriesHDR[1].binding = 1;
    bgEntriesHDR[1].sampler = _hdrSampler;
    bgEntriesHDR[2].binding = 2;
    bgEntriesHDR[2].buffer = _faceUniformBuffer;
    bgEntriesHDR[2].size = sizeof(_currentFace);

    wgpu::BindGroupDescriptor hdrBindgroupDesc{};
    hdrBindgroupDesc.layout = _bindGroupLayout;
    hdrBindgroupDesc.entryCount = bgEntriesHDR.size();
    hdrBindgroupDesc.entries = bgEntriesHDR.data();

    _hdrBindGroup = _renderer.Device().CreateBindGroup(&hdrBindgroupDesc);

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

    hdrPass.SetBindGroup(0, _hdrBindGroup, 1, &dynamicOffset);
    hdrPass.Draw(3, 1, 0, 0);

    hdrPass.End();
}
