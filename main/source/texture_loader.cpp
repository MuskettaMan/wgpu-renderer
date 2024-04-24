#include "texture_loader.hpp"
#include <cassert>
#include "renderer.hpp"

TextureLoader::TextureLoader(const Renderer& renderer) : _renderer(renderer)
{
    std::array<wgpu::BindGroupLayoutEntry, 2> bgLayoutEntries{};
    bgLayoutEntries[0].binding = 0;
    bgLayoutEntries[0].visibility = wgpu::ShaderStage::Compute;
    bgLayoutEntries[0].texture.sampleType = wgpu::TextureSampleType::Float;
    bgLayoutEntries[0].texture.viewDimension = wgpu::TextureViewDimension::e2D;

    bgLayoutEntries[1].binding = 1;
    bgLayoutEntries[1].visibility = wgpu::ShaderStage::Compute;
    bgLayoutEntries[1].storageTexture.access = wgpu::StorageTextureAccess::WriteOnly;
    bgLayoutEntries[1].storageTexture.format = wgpu::TextureFormat::RGBA8Unorm; // TODO: Check if using a different format will work. Original uses srgb.
    bgLayoutEntries[1].storageTexture.viewDimension = wgpu::TextureViewDimension::e2D;

    wgpu::BindGroupLayoutDescriptor bgLayoutDesc{};
    bgLayoutDesc.label = "Texture binding group layout";
    bgLayoutDesc.entryCount = bgLayoutEntries.size();
    bgLayoutDesc.entries = bgLayoutEntries.data();

    _bgLayout = _renderer.Device().CreateBindGroupLayout(&bgLayoutDesc);

    wgpu::PipelineLayoutDescriptor bgPipelineLayoutDesc{};
    bgPipelineLayoutDesc.bindGroupLayoutCount = 1;
    bgPipelineLayoutDesc.bindGroupLayouts = &_bgLayout;

    wgpu::PipelineLayout bgPipelineLayout = _renderer.Device().CreatePipelineLayout(&bgPipelineLayoutDesc);



    wgpu::ComputePipelineDescriptor computePipelineDesc{};
    computePipelineDesc.label = "Mip map generation";
    computePipelineDesc.compute.entryPoint = "main";
    computePipelineDesc.compute.module = _renderer.CreateShader("assets/mip-comp.wgsl", "Mip map generation");
    computePipelineDesc.layout = bgPipelineLayout;
    _computePipeline = _renderer.Device().CreateComputePipeline(&computePipelineDesc);
}

wgpu::Texture TextureLoader::LoadTexture(const std::string & path, const char* label) const
{
    assert(false && "Not implemented");
    return wgpu::Texture();
}

wgpu::Texture TextureLoader::LoadTexture(const std::vector<uint8_t>& data, uint32_t width, uint32_t height, wgpu::TextureFormat format, uint32_t mipLevels, const char* label) const
{
    wgpu::TextureDescriptor textureDesc{};
    textureDesc.label = label;
    textureDesc.dimension = wgpu::TextureDimension::e2D;
    textureDesc.size = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };
    textureDesc.format = wgpu::TextureFormat::RGBA8Unorm;
    textureDesc.mipLevelCount = mipLevels;
    textureDesc.sampleCount = 1;
    textureDesc.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::StorageBinding;
    textureDesc.viewFormatCount = 0;
    textureDesc.viewFormats = nullptr;

    auto texture = _renderer.Device().CreateTexture(&textureDesc);

    wgpu::ImageCopyTexture destination{};
    destination.texture = texture;
    destination.origin = { 0, 0, 0 };
    destination.aspect = wgpu::TextureAspect::All;
    destination.mipLevel = 0;

    wgpu::TextureDataLayout source{};
    source.offset = 0;
    source.bytesPerRow = 4 * width;
    source.rowsPerImage = height;

    _renderer.Queue().WriteTexture(&destination, data.data(), data.size(), &source, &textureDesc.size);
    if (mipLevels == 1)
        return texture;

    std::vector<wgpu::TextureView> mipViews;
    std::vector<wgpu::Extent3D> mipSizes;

    mipSizes.resize(mipLevels);
    mipSizes[0] = textureDesc.size;

    mipViews.reserve(mipSizes.size());
    for (uint32_t i = 0; i < mipLevels; ++i)
    {
        wgpu::TextureViewDescriptor viewDesc{};
        std::string labelStr{ "MIP level #" + std::to_string(i) };
        viewDesc.label = labelStr.c_str();
        viewDesc.baseMipLevel = i;
        viewDesc.aspect = wgpu::TextureAspect::All;
        viewDesc.dimension = wgpu::TextureViewDimension::e2D;
        viewDesc.format = textureDesc.format;
        viewDesc.baseArrayLayer = 0;
        viewDesc.arrayLayerCount = 1;
        viewDesc.mipLevelCount = 1;

        mipViews.emplace_back(texture.CreateView(&viewDesc));

        if (i > 0)
        {
            wgpu::Extent3D previousSize = mipSizes[i - 1];
            mipSizes[i] = { previousSize.width / 2, previousSize.height / 2, previousSize.depthOrArrayLayers / 2 };
        }
    }


    

    wgpu::CommandEncoderDescriptor ceDesc;
    ceDesc.label = "Command encoder";

    wgpu::CommandEncoder encoder = _renderer.Device().CreateCommandEncoder(&ceDesc);

    wgpu::ComputePassDescriptor computePassDesc{};
    computePassDesc.label = "Mip map generation compute pass";
    computePassDesc.timestampWrites = 0;
    wgpu::ComputePassEncoder computePass = encoder.BeginComputePass(&computePassDesc);

    computePass.SetPipeline(_computePipeline);

    for (size_t level = 1; level < mipLevels; ++level)
    {
        std::array<wgpu::BindGroupEntry, 2> bgEntries{};
        bgEntries[0].binding = 0;
        bgEntries[0].textureView = mipViews[level - 1];

        bgEntries[1].binding = 1;
        bgEntries[1].textureView = mipViews[level];

        wgpu::BindGroupDescriptor bgDesc{};
        bgDesc.entryCount = bgEntries.size();
        bgDesc.entries = bgEntries.data();
        bgDesc.layout = _bgLayout;

        wgpu::BindGroup bg = _renderer.Device().CreateBindGroup(&bgDesc);

        computePass.SetBindGroup(0, bg, 0, nullptr);

        uint32_t invocationCountX = mipSizes[level].width;
        uint32_t invocationCountY = mipSizes[level].height;
        const uint32_t workgroupSizePerDim = 8;
        uint32_t workgroupCountX = (invocationCountX + workgroupSizePerDim - 1) / workgroupSizePerDim;
        uint32_t workgroupCountY = (invocationCountY + workgroupSizePerDim - 1) / workgroupSizePerDim;

        computePass.DispatchWorkgroups(workgroupCountX, workgroupCountY, 1);
    }
    computePass.End();

    wgpu::CommandBuffer commands = encoder.Finish(nullptr);
    _renderer.Queue().Submit(1, &commands);

    return texture;
}
