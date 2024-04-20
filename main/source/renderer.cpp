#include "renderer.hpp"

#include <iostream>
#include <ext.hpp>
#include <fstream>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_wgpu.h>
#include <cstddef>

#include "aliases.hpp"
#include "enum_util.hpp"
#include "utils.hpp"

Renderer::Renderer(DeviceResources deviceResources, GLFWwindow* window, int32_t width, int32_t height) :
    _adapter(deviceResources.adapter),
    _instance(deviceResources.instance),
    _device(deviceResources.device),
    _queue(deviceResources.queue),
    _window(window),
    _width(width),
    _height(height),
    _uniformStride(ceilToNextMultiple(sizeof(Instance), 256))
{
    _device.SetUncapturedErrorCallback([](WGPUErrorType error, const char* message, void* userdata)
                                       {
                                           std::cout << "Uncaptured device error - error  type: " << conv_enum_str<wgpu::ErrorType>(error) << "\n";
                                           if (message) std::cout << message;
                                           std::cout << std::endl;
                                       }, nullptr);

    _queue = _device.GetQueue();

    _queue.OnSubmittedWorkDone([](WGPUQueueWorkDoneStatus status, void* userdata)
                               {
                                   std::cout << "Queue work finished with status: " << conv_enum_str<wgpu::QueueWorkDoneStatus>(status) << std::endl;
                               }, nullptr);

    _cameraTransform.translation = glm::vec3{ 0.0f, 2.0f, -3.0f };
    _cameraTransform.rotation = glm::quat{ glm::vec3{ glm::radians(30.0f), 0.0f, 0.0f } };
    glm::mat4 camMat{ BuildSRT(_cameraTransform) };

    _commonData.view = glm::inverse(camMat);

    _commonBuf = CreateBuffer(&_commonData, sizeof(_commonData), wgpu::BufferUsage::Uniform, "Common uniform");
    _instanceBuf = CreateBuffer(nullptr, _uniformStride * MAX_INSTANCES, wgpu::BufferUsage::Uniform, "Instance uniform");

    CreatePipelineAndBuffers();
    Resize(_width, _height);

    if (!SetupImGui())
    {
        std::cout << "Failed initalizing ImGui" << std::endl;
        return;
    }
}

Renderer::~Renderer()
{
}

void Renderer::Render() const
{
    glfwPollEvents();

    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    bool show = true;
    
    ImGui::Begin("Inspector");
    ImGui::InputFloat3("Light direction", &_commonData.lightDirection.x);
    ImGui::ColorEdit3("Light color", &_commonData.lightColor.r);
    ImGui::End();

    ImGui::Render();

    glm::mat4 camMat{ BuildSRT(_cameraTransform) };

    _commonData.view = glm::inverse(camMat);
    _commonData.vp = _commonData.proj * _commonData.view;
    _commonData.time = glfwGetTime();
    _queue.WriteBuffer(_commonBuf, 0, &_commonData, sizeof(_commonData));

    wgpu::TextureView backBufView = _swapChain.GetCurrentTextureView();

    wgpu::RenderPassColorAttachment colorDesc{};
    colorDesc.view = _msaaView;
    colorDesc.resolveTarget = backBufView;
    colorDesc.loadOp = wgpu::LoadOp::Clear;
    colorDesc.storeOp = wgpu::StoreOp::Discard; // TODO: Review difference with store.
    colorDesc.clearValue.r = 0.3f;
    colorDesc.clearValue.g = 0.3f;
    colorDesc.clearValue.b = 0.3f;
    colorDesc.clearValue.a = 1.0f;

    wgpu::RenderPassDepthStencilAttachment depthStencilAttachment;
    depthStencilAttachment.view = _depthTextureView;
    depthStencilAttachment.depthClearValue = 1.0f;
    depthStencilAttachment.depthLoadOp = wgpu::LoadOp::Clear;
    depthStencilAttachment.depthStoreOp = wgpu::StoreOp::Store;
    depthStencilAttachment.depthReadOnly = false;
    depthStencilAttachment.stencilClearValue = 0.f;
    depthStencilAttachment.stencilLoadOp = wgpu::LoadOp::Undefined;
    depthStencilAttachment.stencilStoreOp = wgpu::StoreOp::Undefined;
    depthStencilAttachment.stencilReadOnly = true;

    wgpu::CommandEncoderDescriptor ceDesc; 
    ceDesc.label = "Command encoder";

    wgpu::CommandEncoder encoder = _device.CreateCommandEncoder(&ceDesc);

    wgpu::RenderPassDescriptor renderPass{};
    renderPass.label = "Main render pass";
    renderPass.colorAttachmentCount = 1;
    renderPass.colorAttachments = &colorDesc;
    renderPass.depthStencilAttachment = &depthStencilAttachment;

    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
     
    pass.SetPipeline(_pipeline);

    uint32_t i{ 0 };
    while(!_drawings.empty())
    {
        auto [mesh, transform] = _drawings.front();
        _drawings.pop();

        Instance instance;
        instance.model = BuildSRT(transform);

        uint32_t dynamicOffset{ i * _uniformStride };
        _queue.WriteBuffer(_instanceBuf, dynamicOffset, &instance, sizeof(instance));

        pass.SetVertexBuffer(0, mesh.vertBuf, 0, wgpu::kWholeSize);
        pass.SetIndexBuffer(mesh.indexBuf, mesh.indexFormat, 0, wgpu::kWholeSize);

        pass.SetBindGroup(0, _standardBindGroup, 1, &dynamicOffset);
        pass.SetBindGroup(1, mesh.bindGroup, 0, nullptr);

        pass.DrawIndexed(mesh.indexCount, 1, 0, 0, 0);

        ++i;
    }

    pass.End();


    wgpu::RenderPassColorAttachment imguiColorDesc{};
    imguiColorDesc.view = backBufView;
    imguiColorDesc.loadOp = wgpu::LoadOp::Load;
    imguiColorDesc.storeOp = wgpu::StoreOp::Store;

    wgpu::RenderPassDescriptor imguiPassDesc{};
    imguiPassDesc.label = "ImGui pass";
    imguiPassDesc.colorAttachmentCount = 1;
    imguiPassDesc.colorAttachments = &imguiColorDesc;
    imguiPassDesc.depthStencilAttachment = nullptr;

    wgpu::RenderPassEncoder imguiPass = encoder.BeginRenderPass(&imguiPassDesc);
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), imguiPass.Get());
    imguiPass.End();

    wgpu::CommandBuffer commands = encoder.Finish(nullptr);

    _queue.Submit(1, &commands);
}

void Renderer::Resize(int32_t width, int32_t height)
{
    _width = width;
    _height = height;
    SetupRenderTarget();

    _camera.ratio = _width / static_cast<float>(_height);

    _commonData.proj = glm::perspective(_camera.fov, _camera.ratio, _camera.zNear, _camera.zFar);
    _commonData.vp = _commonData.proj * _commonData.view;

    _queue.WriteBuffer(_commonBuf, 0, &_commonData, sizeof(_commonData));
}

void Renderer::DrawMesh(const Mesh& mesh, const Transform& transform)
{
    _drawings.emplace(mesh, transform);
}

void Renderer::SetupRenderTarget()
{
    wgpu::SurfaceDescriptorFromCanvasHTMLSelector canvasDesc{};
    canvasDesc.sType = wgpu::SType::SurfaceDescriptorFromCanvasHTMLSelector;
    canvasDesc.selector = "canvas";

    wgpu::SurfaceDescriptor surfDesc{};
    surfDesc.nextInChain = reinterpret_cast<wgpu::ChainedStruct*>(&canvasDesc);

    wgpu::Surface surface = _instance.CreateSurface(&surfDesc);

    wgpu::SwapChainDescriptor swapDesc{};
    swapDesc.label = "Swapchain";
    swapDesc.usage = wgpu::TextureUsage::RenderAttachment;
    swapDesc.format = _swapChainFormat = surface.GetPreferredFormat(_adapter);
    swapDesc.width = _width;
    swapDesc.height = _height;
    swapDesc.presentMode = wgpu::PresentMode::Fifo;

    _swapChain = _device.CreateSwapChain(surface, &swapDesc);

    wgpu::TextureDescriptor msaaDesc{};
    msaaDesc.label = "MSAA RT";
    msaaDesc.size.width = _width;
    msaaDesc.size.height = _height;
    msaaDesc.sampleCount = 4;
    msaaDesc.format = _swapChainFormat;
    msaaDesc.usage = wgpu::TextureUsage::RenderAttachment;
    _msaaTarget = _device.CreateTexture(&msaaDesc);
    
    _msaaView = _msaaTarget.CreateView();

    wgpu::TextureDescriptor depthTextureDesc{};
    depthTextureDesc.label = "Depth texture";
    depthTextureDesc.dimension = wgpu::TextureDimension::e2D;
    depthTextureDesc.format = DEPTH_STENCIL_FORMAT;
    depthTextureDesc.mipLevelCount = 1;
    depthTextureDesc.sampleCount = 4;
    depthTextureDesc.size = { static_cast<uint32_t>(_width), static_cast<uint32_t>(_height), 1 };
    depthTextureDesc.usage = wgpu::TextureUsage::RenderAttachment;
    depthTextureDesc.viewFormatCount = 1;
    depthTextureDesc.viewFormats = &DEPTH_STENCIL_FORMAT;

    _depthTexture = _device.CreateTexture(&depthTextureDesc);

    wgpu::TextureViewDescriptor depthTextureViewDesc{};
    depthTextureViewDesc.label = "Depth texture view";
    depthTextureViewDesc.aspect = wgpu::TextureAspect::DepthOnly;
    depthTextureViewDesc.baseArrayLayer = 0;
    depthTextureViewDesc.arrayLayerCount = 1;
    depthTextureViewDesc.mipLevelCount = 1;
    depthTextureViewDesc.dimension = wgpu::TextureViewDimension::e2D;
    depthTextureViewDesc.format = DEPTH_STENCIL_FORMAT;

    _depthTextureView = _depthTexture.CreateView(&depthTextureViewDesc);
}

void Renderer::CreatePipelineAndBuffers()
{
    std::array<wgpu::BindGroupLayoutEntry, 2> bgLayoutEntry{};
    bgLayoutEntry[0].binding = 0;
    bgLayoutEntry[0].visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
    bgLayoutEntry[0].buffer.type = wgpu::BufferBindingType::Uniform;
    bgLayoutEntry[0].buffer.minBindingSize = sizeof(_commonData);

    bgLayoutEntry[1].binding = 1;
    bgLayoutEntry[1].visibility = wgpu::ShaderStage::Vertex;
    bgLayoutEntry[1].buffer.type = wgpu::BufferBindingType::Uniform;
    bgLayoutEntry[1].buffer.minBindingSize = sizeof(Instance);
    bgLayoutEntry[1].buffer.hasDynamicOffset = true;

    wgpu::BindGroupLayoutDescriptor bgLayoutDesc{};
    bgLayoutDesc.label = "Default binding group layout";
    bgLayoutDesc.entryCount = bgLayoutEntry.size();
    bgLayoutDesc.entries = bgLayoutEntry.data();
    _bgLayouts.standard = _device.CreateBindGroupLayout(&bgLayoutDesc);

    std::array<wgpu::BindGroupEntry, 2> bgEntry{};
    assert(bgLayoutEntry.size() == bgEntry.size() && "Bindgroup entry descriptions don't match their sizes");

    bgEntry[0].binding = 0;
    bgEntry[0].buffer = _commonBuf;
    bgEntry[0].size = sizeof(_commonData);

    bgEntry[1].binding = 1;
    bgEntry[1].buffer = _instanceBuf;
    bgEntry[1].size = sizeof(Instance);
     
    wgpu::BindGroupDescriptor bgDesc{};
    bgDesc.label = "Standard bind group";
    bgDesc.layout = _bgLayouts.standard;
    bgDesc.entryCount = bgLayoutDesc.entryCount;
    bgDesc.entries = bgEntry.data();
    _standardBindGroup = _device.CreateBindGroup(&bgDesc);


    // Create mesh bind group layout.
    std::array<wgpu::BindGroupLayoutEntry, 2> bgLayoutEntriesMesh{};
    bgLayoutEntriesMesh[0].binding = 0;
    bgLayoutEntriesMesh[0].visibility = wgpu::ShaderStage::Fragment;
    bgLayoutEntriesMesh[0].texture.sampleType = wgpu::TextureSampleType::Float;
    bgLayoutEntriesMesh[0].texture.viewDimension = wgpu::TextureViewDimension::e2D;
    bgLayoutEntriesMesh[0].texture.multisampled = false;

    bgLayoutEntriesMesh[1].binding = 1;
    bgLayoutEntriesMesh[1].visibility = wgpu::ShaderStage::Fragment;
    bgLayoutEntriesMesh[1].sampler.type = wgpu::SamplerBindingType::Filtering;

    wgpu::BindGroupLayoutDescriptor bgLayoutMeshDesc{};
    bgLayoutMeshDesc.label = "Mesh binding group layout";
    bgLayoutMeshDesc.entryCount = bgLayoutEntriesMesh.size();
    bgLayoutMeshDesc.entries = bgLayoutEntriesMesh.data();
    _bgLayouts.mesh = _device.CreateBindGroupLayout(&bgLayoutMeshDesc);


    wgpu::ShaderModule vertModule = CreateShader("assets/vertex.wgsl", "Vertex shader");
    wgpu::ShaderModule fragModule = CreateShader("assets/frag.wgsl", "Fragment shader");

    wgpu::PipelineLayoutDescriptor layoutDesc{};
    layoutDesc.label = "Default pipeline layout";
    layoutDesc.bindGroupLayoutCount = 2;
    layoutDesc.bindGroupLayouts = &_bgLayouts.standard;
    wgpu::PipelineLayout pipelineLayout = _device.CreatePipelineLayout(&layoutDesc);

    std::vector<wgpu::VertexAttribute> vertAttrs = {};
    vertAttrs.emplace_back(wgpu::VertexFormat::Float32x3, offsetof(Vertex, position), 0);
    vertAttrs.emplace_back(wgpu::VertexFormat::Float32x3, offsetof(Vertex, normal), 1);
    vertAttrs.emplace_back(wgpu::VertexFormat::Float32x2, offsetof(Vertex, uv), 2);

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
    colorTarget.format = _swapChainFormat; 
    colorTarget.blend = &blend;
    colorTarget.writeMask = wgpu::ColorWriteMask::All;

    wgpu::FragmentState fragment{};
    fragment.module = fragModule;
    fragment.entryPoint = "main";
    fragment.targetCount = 1;
    fragment.targets = &colorTarget;
    fragment.constantCount = 0; 
    fragment.constants = nullptr;

    wgpu::RenderPipelineDescriptor rpDesc{};
    rpDesc.label = "Render pipeline";
    rpDesc.fragment = &fragment;
    rpDesc.layout = pipelineLayout;
    rpDesc.depthStencil = nullptr;

    rpDesc.vertex.module = vertModule;
    rpDesc.vertex.entryPoint = "main";
    rpDesc.vertex.bufferCount = 1;
    rpDesc.vertex.buffers = &vertexBufferLayout;

    rpDesc.multisample.count = 4;
    rpDesc.multisample.mask = 0xFF'FF'FF'FF;
    rpDesc.multisample.alphaToCoverageEnabled = false;

    rpDesc.primitive.frontFace = wgpu::FrontFace::CCW;
    rpDesc.primitive.cullMode = wgpu::CullMode::None;
    rpDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    rpDesc.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined; 

    wgpu::DepthStencilState depthState{};
    depthState.depthCompare = wgpu::CompareFunction::Less;
    depthState.depthWriteEnabled = true;
    depthState.format = DEPTH_STENCIL_FORMAT;
    depthState.stencilReadMask = 0;
    depthState.stencilWriteMask = 0;

    rpDesc.depthStencil = &depthState;


    _pipeline = _device.CreateRenderPipeline(&rpDesc);
} 

wgpu::Buffer Renderer::CreateBuffer(const void* data, unsigned long size, wgpu::BufferUsage usage, const char* label)
{
    wgpu::BufferDescriptor desc{};
    desc.label = label;
    desc.usage = usage | wgpu::BufferUsage::CopyDst; 
    desc.size = (size + 3) & ~3; // Ensure multiple of 4.
    wgpu::Buffer buffer = _device.CreateBuffer(&desc);
    _queue.WriteBuffer(buffer, 0, data, desc.size);

    return buffer;
}

wgpu::Texture Renderer::CreateTexture(const tinygltf::Image& image, const std::vector<uint8_t>& data, uint32_t mipLevelCount, const char* label)
{
    wgpu::TextureDescriptor textureDesc{};
    textureDesc.dimension = wgpu::TextureDimension::e2D;
    textureDesc.size = { static_cast<uint32_t>(image.width), static_cast<uint32_t>(image.height), 1 };
    textureDesc.format = wgpu::TextureFormat::RGBA8UnormSrgb;
    textureDesc.mipLevelCount = mipLevelCount;
    textureDesc.sampleCount = 1;
    textureDesc.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding;
    textureDesc.viewFormatCount = 0;
    textureDesc.viewFormats = nullptr;

    auto texture = _device.CreateTexture(&textureDesc);

    wgpu::ImageCopyTexture destination{};
    destination.texture = texture;
    destination.origin = { 0, 0, 0 };
    destination.aspect = wgpu::TextureAspect::All;

    wgpu::TextureDataLayout source{};
    source.offset = 0;

    wgpu::Extent3D mipLevelSize = textureDesc.size;
    wgpu::Extent3D previousMipLevelSize;
    std::vector<uint8_t> previousMipData{};
    for (size_t i = 0; i < mipLevelCount; ++i)
    {
        std::vector<uint8_t> mipData(4 * mipLevelSize.width * mipLevelSize.height);
        if (i == 0)
        {
            mipData.assign(data.begin(), data.end());
        }
        else
        {
            // Generate mip data.
            for (uint32_t y = 0; y < mipLevelSize.height; ++y)
            {
                for (uint32_t x = 0; x < mipLevelSize.width; ++x)
                {
                    uint8_t* p = &mipData[4 * (y * mipLevelSize.width + x)];

                    uint8_t* p00 = &previousMipData[4 * ((2 * y + 0) * previousMipLevelSize.width + (2 * x + 0))];
                    uint8_t* p01 = &previousMipData[4 * ((2 * y + 0) * previousMipLevelSize.width + (2 * x + 1))];
                    uint8_t* p10 = &previousMipData[4 * ((2 * y + 1) * previousMipLevelSize.width + (2 * x + 0))];
                    uint8_t* p11 = &previousMipData[4 * ((2 * y + 1) * previousMipLevelSize.width + (2 * x + 1))];
                    
                    // Average
                    p[0] = (p00[0] + p01[0] + p10[0] + p11[0]) / 4;
                    p[1] = (p00[1] + p01[1] + p10[1] + p11[1]) / 4;
                    p[2] = (p00[2] + p01[2] + p10[2] + p11[2]) / 4;
                    p[3] = (p00[3] + p01[3] + p10[3] + p11[3]) / 4;
                }
            }
        }
  
        destination.mipLevel = i;

        source.bytesPerRow = 4 * mipLevelSize.width;
        source.rowsPerImage = mipLevelSize.height;

        _queue.WriteTexture(&destination, mipData.data(), mipData.size(), &source, &mipLevelSize);

        previousMipLevelSize = mipLevelSize;
        mipLevelSize.width /= 2;
        mipLevelSize.height /= 2;

        previousMipData = std::move(mipData);
    }


    return texture;
}

wgpu::ShaderModule Renderer::CreateShader(const std::string& path, const char* label)
{
    std::ifstream file{};
    file.open(path);
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    std::string source(size, ' ');
    file.seekg(0);
    file.read(source.data(), size);

    wgpu::ShaderModuleWGSLDescriptor wgsl{};
    wgsl.sType = wgpu::SType::ShaderModuleWGSLDescriptor;
    wgsl.code = source.c_str();
    wgpu::ShaderModuleDescriptor desc{};
    desc.nextInChain = reinterpret_cast<wgpu::ChainedStruct*>(&wgsl);
    desc.label = label;
    return _device.CreateShaderModule(&desc);
}

glm::mat4 Renderer::BuildSRT(const Transform& transform) const
{
    glm::mat4 matrix{ glm::identity<glm::mat4>() };
    matrix = glm::translate(matrix, transform.translation);
    matrix = glm::scale(matrix, transform.scale);
    matrix = matrix * glm::mat4_cast(transform.rotation);

    return matrix;
}

bool Renderer::SetupImGui()
{
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOther(_window, true);
    ImGui_ImplGlfw_InstallEmscriptenCanvasResizeCallback("#canvas");

    ImGui_ImplWGPU_InitInfo imguiInitInfo{};
    imguiInitInfo.DepthStencilFormat = conv_enum<WGPUTextureFormat>(wgpu::TextureFormat::Undefined);
    imguiInitInfo.Device = _device.Get();
    imguiInitInfo.NumFramesInFlight = 3;
    imguiInitInfo.PipelineMultisampleState = { nullptr, 1,  0xFF'FF'FF'FF, false };
    imguiInitInfo.RenderTargetFormat = conv_enum<WGPUTextureFormat>(_swapChainFormat);
    ImGui_ImplWGPU_Init(&imguiInitInfo);
    ImGui_ImplWGPU_CreateDeviceObjects();

    // Prevent opening .ini with emscripten file system.
    io.IniFilename = nullptr;

    return true;
}
