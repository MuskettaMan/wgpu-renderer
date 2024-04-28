#include "graphics/pbr_pass.hpp"
#include "utils.hpp"
#include <renderer.hpp>
#include <cstddef>
#include "mesh.hpp"
#include <iostream>

PBRPass::PBRPass(Renderer& renderer) : 
    RenderPass(renderer, wgpu::TextureFormat::RGBA16Float),
    _uniformStride(ceilToNextMultiple(sizeof(Instance), 256)),
    _vertModule(_renderer.CreateShader("assets/vertex.wgsl", "Vertex shader")),
    _fragModule(_renderer.CreateShader("assets/frag.wgsl", "Fragment shader"))
{
    _instanceBuffer = _renderer.CreateBuffer(nullptr, _uniformStride * MAX_INSTANCES, wgpu::BufferUsage::Uniform, "PBR instances buffer");

    std::array<wgpu::BindGroupLayoutEntry, 1> instanceBGLayoutEntry{};
    instanceBGLayoutEntry[0].binding = 0;
    instanceBGLayoutEntry[0].visibility = wgpu::ShaderStage::Vertex;
    instanceBGLayoutEntry[0].buffer.type = wgpu::BufferBindingType::Uniform;
    instanceBGLayoutEntry[0].buffer.minBindingSize = sizeof(Instance);
    instanceBGLayoutEntry[0].buffer.hasDynamicOffset = true;

    wgpu::BindGroupLayoutDescriptor bgLayoutDesc{};
    bgLayoutDesc.label = "Instance binding group layout";
    bgLayoutDesc.entryCount = instanceBGLayoutEntry.size();
    bgLayoutDesc.entries = instanceBGLayoutEntry.data();
    _instanceBindGroupLayout = _renderer.Device().CreateBindGroupLayout(&bgLayoutDesc);

    wgpu::BindGroupLayoutEntry textureLayoutEntry{};
    textureLayoutEntry.visibility = wgpu::ShaderStage::Fragment;
    textureLayoutEntry.texture.sampleType = wgpu::TextureSampleType::Float;
    textureLayoutEntry.texture.viewDimension = wgpu::TextureViewDimension::e2D;
    textureLayoutEntry.texture.multisampled = false;

    std::array<wgpu::BindGroupLayoutEntry, 8> pbrBGLayoutEntries{};
    pbrBGLayoutEntries[0].binding = 0;
    pbrBGLayoutEntries[0].visibility = wgpu::ShaderStage::Fragment;
    pbrBGLayoutEntries[0].buffer.type = wgpu::BufferBindingType::Uniform;
    pbrBGLayoutEntries[0].buffer.minBindingSize = sizeof(Material);

    pbrBGLayoutEntries[1].binding = 1;
    pbrBGLayoutEntries[1].visibility = wgpu::ShaderStage::Fragment;
    pbrBGLayoutEntries[1].sampler.type = wgpu::SamplerBindingType::Filtering;

    pbrBGLayoutEntries[2] = textureLayoutEntry;
    pbrBGLayoutEntries[2].binding = 2;
    pbrBGLayoutEntries[3] = textureLayoutEntry;
    pbrBGLayoutEntries[3].binding = 3;
    pbrBGLayoutEntries[4] = textureLayoutEntry;
    pbrBGLayoutEntries[4].binding = 4;
    pbrBGLayoutEntries[5] = textureLayoutEntry;
    pbrBGLayoutEntries[5].binding = 5;
    pbrBGLayoutEntries[6] = textureLayoutEntry;
    pbrBGLayoutEntries[6].binding = 6;
    pbrBGLayoutEntries[7] = textureLayoutEntry;
    pbrBGLayoutEntries[7].binding = 7;

    wgpu::BindGroupLayoutDescriptor pbrBGLayoutDesc{};
    pbrBGLayoutDesc.label = "PBR bind group layout";
    pbrBGLayoutDesc.entryCount = pbrBGLayoutEntries.size();
    pbrBGLayoutDesc.entries = pbrBGLayoutEntries.data();
    _pbrBindGroupLayout = _renderer.Device().CreateBindGroupLayout(&pbrBGLayoutDesc);


    std::array<wgpu::BindGroupEntry, 1> bgEntry{};
    bgEntry[0].binding = 0;
    bgEntry[0].buffer = _instanceBuffer;
    bgEntry[0].size = sizeof(Instance);

    wgpu::BindGroupDescriptor bgDesc{};
    bgDesc.label = "Instance bind group";
    bgDesc.layout = _instanceBindGroupLayout;
    bgDesc.entryCount = bgLayoutDesc.entryCount;
    bgDesc.entries = bgEntry.data();
    _instanceBindGroup = _renderer.Device().CreateBindGroup(&bgDesc);

    wgpu::PipelineLayoutDescriptor layoutDesc{};
    layoutDesc.label = "Default pipeline layout";
    std::array<wgpu::BindGroupLayout, 3> bindGroupLayouts{ _renderer.CommonBindGroupLayout(), _instanceBindGroupLayout, _pbrBindGroupLayout};
    layoutDesc.bindGroupLayoutCount = bindGroupLayouts.size();
    layoutDesc.bindGroupLayouts = bindGroupLayouts.data();
    wgpu::PipelineLayout pipelineLayout = _renderer.Device().CreatePipelineLayout(&layoutDesc);

    std::vector<wgpu::VertexAttribute> vertAttrs = {};
    vertAttrs.emplace_back(wgpu::VertexFormat::Float32x3, offsetof(Vertex, position),  0);
    vertAttrs.emplace_back(wgpu::VertexFormat::Float32x3, offsetof(Vertex, normal),    1);
    vertAttrs.emplace_back(wgpu::VertexFormat::Float32x3, offsetof(Vertex, tangent),   2);
    vertAttrs.emplace_back(wgpu::VertexFormat::Float32x3, offsetof(Vertex, bitangent), 3);
    vertAttrs.emplace_back(wgpu::VertexFormat::Float32x2, offsetof(Vertex, uv),        4);

    wgpu::VertexBufferLayout vertexBufferLayout{};
    vertexBufferLayout.arrayStride = sizeof(Vertex);
    vertexBufferLayout.attributeCount = vertAttrs.size();
    vertexBufferLayout.attributes = vertAttrs.data();
    vertexBufferLayout.stepMode = wgpu::VertexStepMode::Vertex;

    wgpu::BlendState blend{};
    blend.color.operation = wgpu::BlendOperation::Add;
    blend.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
    blend.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    blend.alpha.operation = wgpu::BlendOperation::Add;
    blend.alpha.srcFactor = wgpu::BlendFactor::Zero;
    blend.alpha.dstFactor = wgpu::BlendFactor::One;

    wgpu::ColorTargetState colorTarget{};
    colorTarget.format = wgpu::TextureFormat::RGBA16Float; // TODO: Match this with renderer format, instead of hardcoding
    colorTarget.blend = &blend;
    colorTarget.writeMask = wgpu::ColorWriteMask::All;

    wgpu::FragmentState fragment{};
    fragment.module = _fragModule;
    fragment.entryPoint = "main"; // TODO: Make separate shader class, that has this composed.
    fragment.targetCount = 1;
    fragment.targets = &colorTarget;
    fragment.constantCount = 0; // TODO: research constants.
    fragment.constants = nullptr;

    wgpu::VertexState vertex{};
    vertex.module = _vertModule;
    vertex.entryPoint = "main";
    vertex.bufferCount = 1;
    vertex.buffers = &vertexBufferLayout;

    wgpu::RenderPipelineDescriptor rpDesc{};
    rpDesc.label = "PBR render pipeline";
    rpDesc.fragment = &fragment;
    rpDesc.vertex = vertex;
    rpDesc.layout = pipelineLayout;
    rpDesc.depthStencil = nullptr;

    rpDesc.multisample.count = 4; // TODO: Match this with renderer target, instead of hardcoding
    rpDesc.multisample.mask = 0xFF'FF'FF'FF;
    rpDesc.multisample.alphaToCoverageEnabled = false;

    rpDesc.primitive.frontFace = wgpu::FrontFace::CCW;
    rpDesc.primitive.cullMode = wgpu::CullMode::None;
    rpDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    rpDesc.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined;

    wgpu::DepthStencilState depthState{};
    depthState.depthCompare = wgpu::CompareFunction::Less;
    depthState.depthWriteEnabled = true;
    depthState.format = _renderer.DEPTH_STENCIL_FORMAT;
    depthState.stencilReadMask = 0;
    depthState.stencilWriteMask = 0;

    rpDesc.depthStencil = &depthState;

    _pipeline = _renderer.Device().CreateRenderPipeline(&rpDesc);
}

PBRPass::~PBRPass() = default;

void PBRPass::Render(const wgpu::CommandEncoder& encoder, const wgpu::TextureView& renderTarget, std::shared_ptr<const wgpu::TextureView> resolveTarget)
{
    wgpu::RenderPassColorAttachment colorDesc{};
    colorDesc.view = renderTarget;
    colorDesc.resolveTarget = resolveTarget ? *resolveTarget : nullptr;
    colorDesc.loadOp = wgpu::LoadOp::Load;
    colorDesc.storeOp = wgpu::StoreOp::Discard; // TODO: Review difference with store.
    colorDesc.clearValue.r = 0.3f;
    colorDesc.clearValue.g = 0.3f;
    colorDesc.clearValue.b = 0.3f;
    colorDesc.clearValue.a = 1.0f;

    wgpu::RenderPassDescriptor renderPass{};
    renderPass.label = "Main render pass";
    renderPass.colorAttachmentCount = 1;
    renderPass.colorAttachments = &colorDesc;
    renderPass.depthStencilAttachment = &_renderer.DepthStencilAttachment();

    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);

    pass.SetPipeline(_pipeline);

    uint32_t i{ 0 };
    while (!_drawings.empty())
    {
        auto [mesh, transform] = _drawings.front();
        _drawings.pop();

        Instance instance;
        instance.model = _renderer.BuildSRT(transform);
        instance.transInvModel = glm::mat4{ glm::mat3{ glm::transpose(glm::inverse(instance.model)) } };

        uint32_t dynamicOffset{ i * _uniformStride };
        _renderer.Queue().WriteBuffer(_instanceBuffer, dynamicOffset, &instance, sizeof(instance)); // TODO: write entire buffer once.

        pass.SetVertexBuffer(0, mesh.vertBuf, 0, wgpu::kWholeSize);
        pass.SetIndexBuffer(mesh.indexBuf, mesh.indexFormat, 0, wgpu::kWholeSize);

        pass.SetBindGroup(0, _renderer.CommonBindGroup(), 0, nullptr);
        pass.SetBindGroup(1, _instanceBindGroup, 1, &dynamicOffset);
        pass.SetBindGroup(2, mesh.bindGroup, 0, nullptr);

        pass.DrawIndexed(mesh.indexCount, 1, 0, 0, 0);

        ++i;
    }

    pass.End();
}

void PBRPass::DrawMesh(const Mesh& mesh, const Transform& transform) const
{
    _drawings.emplace(mesh, transform);
}
