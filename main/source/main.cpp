#include <emscripten.h>
#include <iostream>
#include <emscripten/html5_webgpu.h>
#include <emscripten/html5.h>
#include <webgpu/webgpu_cpp.h>
#include <magic_enum.hpp>
#include <imgui.h>
#include <backends/imgui_impl_wgpu.h>
#include <backends/imgui_impl_glfw.h>
#include <GLFW/glfw3.h>
#include "enum_util.hpp"
#include <fstream>

#include <glm.hpp>
#include <ext.hpp>

using uint32_t = unsigned int;
using int32_t = int;
using uint16_t = unsigned short;

struct Common
{
    glm::mat4 proj;
    glm::mat4 view;
    glm::mat4 vp;
    glm::vec3 color;
    float time;
    //float _padding[2];
};

struct Instance
{
    glm::mat4 model;
    glm::vec3 color;
    float _padding;
};


int32_t g_width = 1280;
int32_t g_height = 720;

GLFWwindow* g_window;
wgpu::Adapter g_adapter;
wgpu::Instance g_instance;
wgpu::Device g_device;
wgpu::Queue g_queue;
wgpu::SwapChain g_swapChain;
wgpu::Texture g_msaaTarget;
wgpu::TextureView g_msaaView;
wgpu::Texture g_depthTexture;
wgpu::TextureView g_depthTextureView;

wgpu::RenderPipeline g_pipeline;
wgpu::Buffer g_vertBuf; // vertex buffer with triangle position and colours
wgpu::Buffer g_indxBuf; // index buffer
wgpu::Buffer g_commonBuf;
wgpu::Buffer g_instanceBuf;
wgpu::BindGroupLayout g_bgLayout;
wgpu::BindGroup g_bindGroup;

wgpu::TextureFormat swapChainFormat{ wgpu::TextureFormat::BGRA8Unorm };
constexpr wgpu::TextureFormat DEPTH_STENCIL_FORMAT{ wgpu::TextureFormat::Depth24Plus };

Common g_commonData;
Instance g_instanceData;

void CreateSwapChainAndRenderTarget()
{
    wgpu::SurfaceDescriptorFromCanvasHTMLSelector canvasDesc{};
    canvasDesc.sType = wgpu::SType::SurfaceDescriptorFromCanvasHTMLSelector;
    canvasDesc.selector = "canvas";

    wgpu::SurfaceDescriptor surfDesc{};
    surfDesc.nextInChain = reinterpret_cast<wgpu::ChainedStruct*>(&canvasDesc);
    
    wgpu::Surface surface = g_instance.CreateSurface(&surfDesc);

    wgpu::SwapChainDescriptor swapDesc{};
    swapDesc.label = "Swapchain";
    swapDesc.usage = wgpu::TextureUsage::RenderAttachment;
    swapDesc.format = swapChainFormat = surface.GetPreferredFormat(g_adapter);
    swapDesc.width = g_width;
    swapDesc.height = g_height;
    swapDesc.presentMode = wgpu::PresentMode::Fifo;

    g_swapChain = g_device.CreateSwapChain(surface, &swapDesc);

    wgpu::TextureDescriptor msaaDesc{};
    msaaDesc.label = "MSAA RT";
    msaaDesc.size.width = g_width;
    msaaDesc.size.height = g_height;
    msaaDesc.sampleCount = 4;
    msaaDesc.format = swapChainFormat;
    msaaDesc.usage = wgpu::TextureUsage::RenderAttachment;
    g_msaaTarget = g_device.CreateTexture(&msaaDesc);

    g_msaaView = g_msaaTarget.CreateView();

    wgpu::TextureDescriptor depthTextureDesc{};
    depthTextureDesc.label = "Depth texture";
    depthTextureDesc.dimension = wgpu::TextureDimension::e2D;
    depthTextureDesc.format = DEPTH_STENCIL_FORMAT;
    depthTextureDesc.mipLevelCount = 1;
    depthTextureDesc.sampleCount = 4;
    depthTextureDesc.size = { static_cast<uint32_t>(g_width), static_cast<uint32_t>(g_height), 1 };
    depthTextureDesc.usage = wgpu::TextureUsage::RenderAttachment;
    depthTextureDesc.viewFormatCount = 1;
    depthTextureDesc.viewFormats = &DEPTH_STENCIL_FORMAT;

    g_depthTexture = g_device.CreateTexture(&depthTextureDesc);
    
    wgpu::TextureViewDescriptor depthTextureViewDesc{};
    depthTextureViewDesc.label = "Depth texture view";
    depthTextureViewDesc.aspect = wgpu::TextureAspect::DepthOnly;
    depthTextureViewDesc.baseArrayLayer = 0;
    depthTextureViewDesc.arrayLayerCount = 1;
    depthTextureViewDesc.mipLevelCount = 1;
    depthTextureViewDesc.dimension = wgpu::TextureViewDimension::e2D;
    depthTextureViewDesc.format = DEPTH_STENCIL_FORMAT;

    g_depthTextureView = g_depthTexture.CreateView(&depthTextureViewDesc);
}

wgpu::Buffer CreateBuffer(const void* data, unsigned long size, wgpu::BufferUsage usage, const char* label)
{
    wgpu::BufferDescriptor desc{};
    desc.label = label;
    desc.usage = usage | wgpu::BufferUsage::CopyDst;
    desc.size = (size + 3) & ~3; // Ensure multiple of 4.
    wgpu::Buffer buffer = g_device.CreateBuffer(&desc);
    g_queue.WriteBuffer(buffer, 0, data, desc.size);
    
    return buffer;
}

wgpu::ShaderModule CreateShader(const std::string& path, const char* label = nullptr)
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
    return g_device.CreateShaderModule(&desc);
}

void CreatePipelineAndBuffers()
{
    float near = 0.001f;
    float far = 100.0f;
    float ratio = g_width / static_cast<float>(g_height); // TODO: update on screen resize.
    float focalLength = 2.0f;
    float fov = 2 * glm::atan(1 / focalLength);

    g_commonData.view = glm::lookAt(glm::vec3{ 0.0f, 2.0f, -3.0f }, glm::vec3{ 0.0f }, glm::vec3{ 0.0f, 1.0f, 0.0f });
    g_commonData.proj = glm::perspective(fov, ratio, near, far);
    g_commonData.vp = g_commonData.proj * g_commonData.view;
    g_commonData.color = glm::vec3{ 1.0f, 0.5f, 0.5f };

    g_instanceData.model = glm::identity<glm::mat4>();
    g_instanceData.color = glm::vec3{ 0.5f, 1.0f, 0.5f };

    g_commonBuf = CreateBuffer(&g_commonData, sizeof(g_commonData), wgpu::BufferUsage::Uniform, "Common uniform");
    g_instanceBuf = CreateBuffer(&g_instanceData, sizeof(g_instanceData), wgpu::BufferUsage::Uniform, "Instance uniform");

    wgpu::BindGroupLayoutEntry bgLayoutEntry[2]{};
    bgLayoutEntry[0].binding = 0;
    bgLayoutEntry[0].visibility = wgpu::ShaderStage::Vertex;
    bgLayoutEntry[0].buffer.type = wgpu::BufferBindingType::Uniform;
    bgLayoutEntry[0].buffer.minBindingSize = sizeof(g_commonData);

    bgLayoutEntry[1].binding = 1;
    bgLayoutEntry[1].visibility = wgpu::ShaderStage::Vertex;
    bgLayoutEntry[1].buffer.type = wgpu::BufferBindingType::Uniform;
    bgLayoutEntry[1].buffer.minBindingSize = sizeof(g_instanceData);

    wgpu::BindGroupLayoutDescriptor bgLayoutDesc{};
    bgLayoutDesc.label = "Default binding group layout";
    bgLayoutDesc.entryCount = 2;
    bgLayoutDesc.entries = bgLayoutEntry;
    g_bgLayout = g_device.CreateBindGroupLayout(&bgLayoutDesc);

    wgpu::BindGroupEntry bgEntry[2]{};
    bgEntry[0].binding = 0;
    bgEntry[0].buffer = g_commonBuf;
    bgEntry[0].size = sizeof(g_commonData);

    bgEntry[1].binding = 1;
    bgEntry[1].buffer = g_instanceBuf;
    bgEntry[1].size = sizeof(g_instanceData);

    wgpu::BindGroupDescriptor bgDesc{};
    bgDesc.layout = g_bgLayout;
    bgDesc.entryCount = bgLayoutDesc.entryCount;
    bgDesc.entries = bgEntry;
    g_bindGroup = g_device.CreateBindGroup(&bgDesc);

    wgpu::ShaderModule vertModule = CreateShader("assets/vertex.wgsl", "Vertex shader");
    wgpu::ShaderModule fragModule = CreateShader("assets/frag.wgsl", "Fragment shader");

    wgpu::PipelineLayoutDescriptor layoutDesc{};
    layoutDesc.label = "Default pipeline layout";
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts = &g_bgLayout;
    wgpu::PipelineLayout pipelineLayout = g_device.CreatePipelineLayout(&layoutDesc);

    wgpu::VertexAttribute vertAttrs[2] = {};
    vertAttrs[0].format = wgpu::VertexFormat::Float32x3;
    vertAttrs[0].offset = 0;
    vertAttrs[0].shaderLocation = 0;
    vertAttrs[1].format = wgpu::VertexFormat::Float32x3;
    vertAttrs[1].offset = sizeof(float) * 3;
    vertAttrs[1].shaderLocation = 1;

    wgpu::VertexBufferLayout vertexBufferLayout{};
    vertexBufferLayout.arrayStride = sizeof(float) * 6;
    vertexBufferLayout.attributeCount = 2;
    vertexBufferLayout.attributes = vertAttrs;
    vertexBufferLayout.stepMode = wgpu::VertexStepMode::Vertex;

    wgpu::BlendState blend{};
    blend.color.operation = wgpu::BlendOperation::Add;
    blend.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
    blend.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    blend.alpha.operation = wgpu::BlendOperation::Add;
    blend.alpha.srcFactor = wgpu::BlendFactor::Zero;
    blend.alpha.dstFactor = wgpu::BlendFactor::One;

    wgpu::ColorTargetState colorTarget{};
    colorTarget.format = swapChainFormat;
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


    g_pipeline = g_device.CreateRenderPipeline(&rpDesc);

    // create the buffers (x, y, r, g, b)
    float const vertData[] = {
         -0.5f, -0.3f, -0.5f,    1.0, 1.0, 1.0,
         +0.5f, -0.3f, -0.5f,    1.0, 1.0, 1.0,
        +0.5f, -0.3f, +0.5f,    1.0, 1.0, 1.0,
        -0.5f, -0.3f, +0.5f,    1.0, 1.0, 1.0,
                                
        +0.0, +0.5, +0.0,    0.5, 0.5, 0.5,
    };
    uint16_t const indxData[] = {
        0, 1, 2,
        0, 2, 3,
        0, 1, 4,
        1, 2, 4,
        2, 3, 4,
        3, 0, 4,
    };

    g_vertBuf = CreateBuffer(vertData, sizeof(vertData), wgpu::BufferUsage::Vertex, "Vertex buffer");
    g_indxBuf = CreateBuffer(indxData, sizeof(indxData), wgpu::BufferUsage::Index, "Index buffer");
}

void Render(double time)
{
    glfwPollEvents();

    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    bool show = true;
    //ImGui::ShowDemoWindow(&show);
    // TODO: Add imgui code here.

    ImGui::Render();

    wgpu::TextureView backBufView = g_swapChain.GetCurrentTextureView();

    wgpu::RenderPassColorAttachment colorDesc{};
    colorDesc.view = g_msaaView;
    colorDesc.resolveTarget = backBufView;
    colorDesc.loadOp = wgpu::LoadOp::Clear; 
    colorDesc.storeOp = wgpu::StoreOp::Discard; // TODO: Review difference with store.
    colorDesc.clearValue.r = 0.3f;
    colorDesc.clearValue.g = 0.3f;
    colorDesc.clearValue.b = 0.3f;
    colorDesc.clearValue.a = 1.0f;

    wgpu::RenderPassDepthStencilAttachment depthStencilAttachment;
    depthStencilAttachment.view = g_depthTextureView;
    depthStencilAttachment.depthClearValue = 1.0f;
    depthStencilAttachment.depthLoadOp = wgpu::LoadOp::Clear;
    depthStencilAttachment.depthStoreOp = wgpu::StoreOp::Store;
    depthStencilAttachment.depthReadOnly = false;
    depthStencilAttachment.stencilClearValue = 0.f;
    depthStencilAttachment.stencilLoadOp = wgpu::LoadOp::Undefined; 
    depthStencilAttachment.stencilStoreOp = wgpu::StoreOp::Undefined;
    depthStencilAttachment.stencilReadOnly = true;

    g_commonData.time = glfwGetTime();
    g_queue.WriteBuffer(g_commonBuf, offsetof(Common, time), &g_commonData.time, sizeof(Common::time));

    wgpu::CommandEncoderDescriptor ceDesc;
    ceDesc.label = "Command encoder";

    wgpu::CommandEncoder encoder = g_device.CreateCommandEncoder(&ceDesc);

    wgpu::RenderPassDescriptor renderPass{};
    renderPass.label = "Main render pass";
    renderPass.colorAttachmentCount = 1;
    renderPass.colorAttachments = &colorDesc;
    renderPass.depthStencilAttachment = &depthStencilAttachment;

    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);

    pass.SetPipeline(g_pipeline);
    pass.SetVertexBuffer(0, g_vertBuf, 0, wgpu::kWholeSize);
    pass.SetIndexBuffer(g_indxBuf, wgpu::IndexFormat::Uint16, 0, wgpu::kWholeSize);
    pass.SetBindGroup(0, g_bindGroup, 0, nullptr);

    pass.DrawIndexed(18, 1, 0, 0, 0);

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
    

    g_queue.Submit(1, &commands);
}

EM_BOOL em_render(double time, void* userData)
{
    Render(time);
    return true;
}

bool SetupImGui()
{
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOther(g_window, true);
    ImGui_ImplGlfw_InstallEmscriptenCanvasResizeCallback("#canvas");

    ImGui_ImplWGPU_InitInfo imguiInitInfo{};
    imguiInitInfo.DepthStencilFormat = conv_enum<WGPUTextureFormat>(wgpu::TextureFormat::Undefined);
    imguiInitInfo.Device = g_device.Get();
    imguiInitInfo.NumFramesInFlight = 3;
    imguiInitInfo.PipelineMultisampleState = { nullptr, 1,  0xFF'FF'FF'FF, false };
    imguiInitInfo.RenderTargetFormat = conv_enum<WGPUTextureFormat>(swapChainFormat);
    ImGui_ImplWGPU_Init(&imguiInitInfo);
    ImGui_ImplWGPU_CreateDeviceObjects();

    // Prevent opening .ini with emscripten file system.
    io.IniFilename = nullptr;

    return true;
}

int main(int argc, char** argv)
{
    double width, height;
    emscripten_get_element_css_size("#canvas", &width, &height);

    g_width = width;
    g_height = height;

    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    g_window = glfwCreateWindow(g_width, g_height, "window title", nullptr, nullptr);

    glfwShowWindow(g_window);

    glfwSetWindowSizeCallback(g_window, [](GLFWwindow* window, int width, int height)
    {
        g_width = width;
        g_height = height;
        CreateSwapChainAndRenderTarget();
    });

    g_instance = wgpu::CreateInstance(nullptr);

    wgpu::RequestAdapterOptions options{};
    options.powerPreference = wgpu::PowerPreference::HighPerformance;
    g_instance.RequestAdapter(&options, [](WGPURequestAdapterStatus status, WGPUAdapter adapter, char const* message, void* userdata)
    {
        if(status != WGPURequestAdapterStatus_Success)
        {
            std::cout << "Failed requesting adapter: " << message << std::endl;
            return;
        }

        g_adapter = wgpu::Adapter(adapter);

        wgpu::DeviceDescriptor deviceDesc{};
        deviceDesc.label = "Device";
        deviceDesc.requiredFeatureCount = 0;
        deviceDesc.requiredLimits = nullptr;
        deviceDesc.nextInChain = nullptr;
        deviceDesc.defaultQueue.nextInChain = nullptr;
        deviceDesc.defaultQueue.label = "Default queue";

        g_adapter.RequestDevice(&deviceDesc, [](WGPURequestDeviceStatus status, WGPUDeviceImpl* deviceHandle, char const* message, void* userdata)
        {
            if (status != WGPURequestDeviceStatus_Success)
            {
                std::cout << "Failed requesting device: " << message << std::endl;
                return;
            }

            g_device = wgpu::Device(deviceHandle);
            g_device.SetUncapturedErrorCallback([](WGPUErrorType error, const char* message, void* userdata)
            {
                std::cout << "Uncaptured device error - error  type: " << conv_enum_str<wgpu::ErrorType>(error) << "\n";
                if (message) std::cout << message;
                std::cout << std::endl;
            }, nullptr);

            g_queue = g_device.GetQueue();
            
            g_queue.OnSubmittedWorkDone([](WGPUQueueWorkDoneStatus status, void* userdata)
            {
                std::cout << "Queue work finished with status: " << conv_enum_str<wgpu::QueueWorkDoneStatus>(status) << std::endl;
            }, nullptr);

            CreateSwapChainAndRenderTarget();
            CreatePipelineAndBuffers();

            if(!SetupImGui())
            {
                std::cout << "Failed initalizing ImGui" << std::endl;
                return;
            }

            // Begin render loop.
            emscripten_request_animation_frame_loop(em_render, nullptr);

        }, nullptr);

    }, nullptr);

    return 0;
}
