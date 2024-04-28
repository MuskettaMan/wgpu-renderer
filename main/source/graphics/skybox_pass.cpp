#include "graphics/skybox_pass.hpp"
#include "renderer.hpp"

constexpr glm::vec3 CUBE_VERTICES[] = {
    // -Z   
    glm::vec3{ -1.0f,  1.0f, -1.0f },
    glm::vec3{ -1.0f, -1.0f, -1.0f },
    glm::vec3{  1.0f, -1.0f, -1.0f },
    glm::vec3{  1.0f, -1.0f, -1.0f },
    glm::vec3{  1.0f,  1.0f, -1.0f },
    glm::vec3{ -1.0f,  1.0f, -1.0f },

    // -X
    glm::vec3{ -1.0f, -1.0f,  1.0f },
    glm::vec3{ -1.0f, -1.0f, -1.0f },
    glm::vec3{ -1.0f,  1.0f, -1.0f },
    glm::vec3{ -1.0f,  1.0f, -1.0f },
    glm::vec3{ -1.0f,  1.0f,  1.0f },
    glm::vec3{ -1.0f, -1.0f,  1.0f },

    // +X
    glm::vec3{  1.0f, -1.0f, -1.0f },
    glm::vec3{  1.0f, -1.0f,  1.0f },
    glm::vec3{  1.0f,  1.0f,  1.0f },
    glm::vec3{  1.0f,  1.0f,  1.0f },
    glm::vec3{  1.0f,  1.0f, -1.0f },
    glm::vec3{  1.0f, -1.0f, -1.0f },

    // +Z
    glm::vec3{ -1.0f, -1.0f,  1.0f },
    glm::vec3{ -1.0f,  1.0f,  1.0f },
    glm::vec3{  1.0f,  1.0f,  1.0f },
    glm::vec3{  1.0f,  1.0f,  1.0f },
    glm::vec3{  1.0f, -1.0f,  1.0f },
    glm::vec3{ -1.0f, -1.0f,  1.0f },

    // +Y
    glm::vec3{ -1.0f,  1.0f, -1.0f },
    glm::vec3{  1.0f,  1.0f, -1.0f },
    glm::vec3{  1.0f,  1.0f,  1.0f },
    glm::vec3{  1.0f,  1.0f,  1.0f },
    glm::vec3{ -1.0f,  1.0f,  1.0f },
    glm::vec3{ -1.0f,  1.0f, -1.0f },

    // -Y
    glm::vec3{ -1.0f, -1.0f, -1.0f },
    glm::vec3{ -1.0f, -1.0f,  1.0f },
    glm::vec3{  1.0f, -1.0f, -1.0f },
    glm::vec3{  1.0f, -1.0f, -1.0f },
    glm::vec3{ -1.0f, -1.0f,  1.0f },
    glm::vec3{  1.0f, -1.0f,  1.0f },
};

SkyboxPass::SkyboxPass(Renderer& renderer) : RenderPass(renderer, wgpu::TextureFormat::RGBA16Float)
{
    _instance.model = glm::identity<glm::mat4>();

    _vertexBuffer = _renderer.CreateBuffer(CUBE_VERTICES, sizeof(CUBE_VERTICES), wgpu::BufferUsage::Vertex, "Skybox vertex buffer");
    _instanceBuffer = _renderer.CreateBuffer(&_instance, sizeof(_instance), wgpu::BufferUsage::Uniform, "Skybox instance buffer");
    _skyboxShader = _renderer.CreateShader("assets/skybox.wgsl", "Skybox shader");

    std::array<wgpu::BindGroupLayoutEntry, 3> skyboxBGLayoutEntries{};
    skyboxBGLayoutEntries[0].binding = 0;
    skyboxBGLayoutEntries[0].visibility = wgpu::ShaderStage::Vertex;
    skyboxBGLayoutEntries[0].buffer.minBindingSize = sizeof(_instance);
    skyboxBGLayoutEntries[0].buffer.type = wgpu::BufferBindingType::Uniform;
    skyboxBGLayoutEntries[0].buffer.hasDynamicOffset = false;

    skyboxBGLayoutEntries[1].binding = 1;
    skyboxBGLayoutEntries[1].visibility = wgpu::ShaderStage::Fragment;
    skyboxBGLayoutEntries[1].sampler.type = wgpu::SamplerBindingType::Filtering;

    skyboxBGLayoutEntries[2].binding = 2;
    skyboxBGLayoutEntries[2].visibility = wgpu::ShaderStage::Fragment;
    skyboxBGLayoutEntries[2].texture.sampleType = wgpu::TextureSampleType::Float;
    skyboxBGLayoutEntries[2].texture.viewDimension = wgpu::TextureViewDimension::Cube;

    wgpu::BindGroupLayoutDescriptor bindGroupLayoutDesc{};
    bindGroupLayoutDesc.label = "Skybox bind group layout";
    bindGroupLayoutDesc.entryCount = skyboxBGLayoutEntries.size();
    bindGroupLayoutDesc.entries = skyboxBGLayoutEntries.data();

    _skyboxBGL = _renderer.Device().CreateBindGroupLayout(&bindGroupLayoutDesc);

    std::array<wgpu::BindGroupLayout, 2> bindGroupLayouts{ _renderer.CommonBindGroupLayout(), _skyboxBGL };

    wgpu::PipelineLayoutDescriptor pipelineLayoutDesc{};
    pipelineLayoutDesc.bindGroupLayoutCount = bindGroupLayouts.size();
    pipelineLayoutDesc.bindGroupLayouts = bindGroupLayouts.data();

    wgpu::PipelineLayout skyboxPipelineLayout = _renderer.Device().CreatePipelineLayout(&pipelineLayoutDesc);
    
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

    wgpu::FragmentState fragmentState{};
    fragmentState.module = _skyboxShader;
    fragmentState.entryPoint = "fs_main";
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    wgpu::VertexAttribute vertexAttribute{};
    vertexAttribute.format = wgpu::VertexFormat::Float32x3;
    vertexAttribute.offset = 0;
    vertexAttribute.shaderLocation = 0;

    wgpu::VertexBufferLayout vertexBufferLayout{};
    vertexBufferLayout.arrayStride = sizeof(glm::vec3);
    vertexBufferLayout.stepMode = wgpu::VertexStepMode::Vertex;
    vertexBufferLayout.attributeCount = 1;
    vertexBufferLayout.attributes = &vertexAttribute;

    wgpu::RenderPipelineDescriptor renderPipelineDesc{};
    renderPipelineDesc.label = "Skybox pipeline";
    renderPipelineDesc.layout = skyboxPipelineLayout;
    renderPipelineDesc.vertex.module = _skyboxShader;
    renderPipelineDesc.vertex.entryPoint = "vs_main";
    renderPipelineDesc.vertex.bufferCount = 1;
    renderPipelineDesc.vertex.buffers = &vertexBufferLayout;
    renderPipelineDesc.fragment = &fragmentState;
    renderPipelineDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    renderPipelineDesc.primitive.cullMode = wgpu::CullMode::None; // Review later.
    
    wgpu::DepthStencilState depthStencilState{};
    depthStencilState.format = _renderer.DEPTH_STENCIL_FORMAT;
    depthStencilState.depthWriteEnabled = false;
    depthStencilState.depthCompare = wgpu::CompareFunction::Less; // Review later.

    renderPipelineDesc.depthStencil = &depthStencilState;
    renderPipelineDesc.multisample.count = 4; // TODO: Match this with renderer target, instead of hardcoding
    renderPipelineDesc.multisample.mask = 0xFF'FF'FF'FF;
    renderPipelineDesc.multisample.alphaToCoverageEnabled = false;

    _skyboxPipeline = _renderer.Device().CreateRenderPipeline(&renderPipelineDesc);
    
    wgpu::SamplerDescriptor samplerDesc{};
    samplerDesc.label = "Skybox sampler";
    samplerDesc.addressModeU = wgpu::AddressMode::ClampToEdge;
    samplerDesc.addressModeV = wgpu::AddressMode::ClampToEdge;
    samplerDesc.addressModeW = wgpu::AddressMode::ClampToEdge;
    samplerDesc.minFilter = wgpu::FilterMode::Linear;
    samplerDesc.magFilter = wgpu::FilterMode::Linear;
    samplerDesc.mipmapFilter = wgpu::MipmapFilterMode::Linear;

    _skyboxSampler = _renderer.Device().CreateSampler(&samplerDesc);
     
    wgpu::TextureDescriptor skyboxTextureDesc{};
    skyboxTextureDesc.label = "Skybox texture";
    skyboxTextureDesc.dimension = wgpu::TextureDimension::e2D;
    skyboxTextureDesc.size = { 2048, 2048, 6 };
    skyboxTextureDesc.format = wgpu::TextureFormat::RGBA16Float;
    skyboxTextureDesc.mipLevelCount = 1;
    skyboxTextureDesc.sampleCount = 1;
    skyboxTextureDesc.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;

    wgpu::TextureViewDescriptor skyboxViewDesc{};
    skyboxViewDesc.dimension = wgpu::TextureViewDimension::Cube;
    skyboxViewDesc.format = wgpu::TextureFormat::RGBA16Float;
    skyboxViewDesc.baseArrayLayer = 0;
    skyboxViewDesc.arrayLayerCount = 6;
    skyboxViewDesc.baseMipLevel = 0;
    skyboxViewDesc.mipLevelCount = 1;

    _skyboxTexture = _renderer.Device().CreateTexture(&skyboxTextureDesc);
    _skyboxView = _skyboxTexture.CreateView(&skyboxViewDesc);


    std::array<wgpu::BindGroupEntry, 3> bgEntries{};
    bgEntries[0].binding = 0;
    bgEntries[0].buffer = _instanceBuffer;
    bgEntries[0].offset = 0;
    bgEntries[1].binding = 1;
    bgEntries[1].sampler = _skyboxSampler;
    bgEntries[2].binding = 2;
    bgEntries[2].textureView = _skyboxView;

    wgpu::BindGroupDescriptor bindGroupDesc{};
    bindGroupDesc.layout = _skyboxBGL;
    bindGroupDesc.entryCount = bgEntries.size();
    bindGroupDesc.entries = bgEntries.data();

    _skyboxBindGroup = _renderer.Device().CreateBindGroup(&bindGroupDesc); 
}

SkyboxPass::~SkyboxPass() = default;

void SkyboxPass::Render(const wgpu::CommandEncoder& encoder, const wgpu::TextureView& renderTarget, std::shared_ptr<const wgpu::TextureView> resolveTarget)
{
    Transform cameraTransform = _renderer.GetCameraTransform();
    _instance.model = glm::translate(glm::mat4{ 1.0f }, cameraTransform.translation);
    _renderer.Queue().WriteBuffer(_instanceBuffer, 0, &_instance, sizeof(_instance));

    wgpu::RenderPassColorAttachment colorDesc{};
    colorDesc.view = renderTarget;
    colorDesc.resolveTarget = resolveTarget ? *resolveTarget : nullptr;
    colorDesc.loadOp = wgpu::LoadOp::Clear;
    colorDesc.storeOp = wgpu::StoreOp::Store;
    colorDesc.clearValue.r = 0.3f;
    colorDesc.clearValue.g = 0.3f;
    colorDesc.clearValue.b = 0.3f;
    colorDesc.clearValue.a = 1.0f;

    wgpu::RenderPassDescriptor renderPass{};
    renderPass.label = "Skybox render pass";
    renderPass.colorAttachmentCount = 1;
    renderPass.colorAttachments = &colorDesc;
    renderPass.depthStencilAttachment = &_renderer.DepthStencilAttachment();

    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);

    pass.SetPipeline(_skyboxPipeline);
    pass.SetVertexBuffer(0, _vertexBuffer, 0, wgpu::kWholeSize);
    pass.SetBindGroup(0, _renderer.CommonBindGroup(), 0, nullptr);
    pass.SetBindGroup(1, _skyboxBindGroup, 0, nullptr);

    pass.Draw(36, 1, 0, 0);

    pass.End();
}
