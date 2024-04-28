#include "renderer.hpp"

#include <iostream>
#include <ext.hpp>
#include <fstream>
#include <cstddef>

#include "aliases.hpp"
#include "enum_util.hpp"
#include "utils.hpp"
#include <stopwatch.hpp>
#include "graphics/pbr_pass.hpp"
#include <graphics/hdr_pass.hpp>
#include <graphics/imgui_pass.hpp>
#include <graphics/skybox_pass.hpp>
#include <graphics/hdri_conversion_pass.hpp>
#include "texture_loader.hpp"

Renderer::Renderer(DeviceResources deviceResources, GLFWwindow* window, int32_t width, int32_t height) :
    _adapter(deviceResources.adapter),
    _instance(deviceResources.instance),
    _device(deviceResources.device),
    _queue(deviceResources.queue),
    _window(window),
    _width(width),
    _height(height)
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
     
    _cameraTransform.translation = glm::vec3{ 0.0f, 2.0f, 3.0f };
    _cameraTransform.rotation = glm::quat{ glm::vec3{ glm::radians(30.0f), 0.0f, 0.0f } };
    glm::mat4 camMat{ BuildSRT(_cameraTransform) };
     
    _commonData.view = glm::inverse(camMat);

    _commonData.pointLights[0] = { { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.5f, 0.0f, 0.5f   }, 1.0f };
    _commonData.pointLights[1] = { { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.5f, 0.25f, -0.5f }, 1.0f };
    _commonData.pointLights[2] = { { 1.0f, 1.0f, 1.0f, 1.0f }, { -0.5f, 0.5f, -0.5f }, 1.0f }; 
    _commonData.pointLights[3] = { { 1.0f, 1.0f, 1.0f, 1.0f }, { -0.5f, 1.0f, 0.5f  }, 1.0f };
    _commonData.normalMapStrength = 0.8f;

    _commonBuf = CreateBuffer(&_commonData, sizeof(_commonData), wgpu::BufferUsage::Uniform, "Common uniform");


    CreatePipelineAndBuffers();
    Resize(_width, _height);
     
    _pbrPass = std::make_unique<PBRPass>(*this);
    _hdrPass = std::make_unique<HDRPass>(*this);
    _imGuiPass = std::make_unique<ImGuiPass>(*this);
    _skyboxPass = std::make_unique<SkyboxPass>(*this);
    HDRIConversionPass hdriConversionPass{ *this };

    _textureLoader = std::make_unique<TextureLoader>(*this); 

    {
        wgpu::CommandEncoderDescriptor ceDesc;
        ceDesc.label = "Command encoder";

        wgpu::CommandEncoder encoder = _device.CreateCommandEncoder(&ceDesc);

        for(uint32_t i = 0; i < 6; ++i)
        {
            hdriConversionPass.SetFace(i);
            hdriConversionPass.Render(encoder, _skyboxPass->SkyboxView(i));
        }
         
        wgpu::CommandBuffer commands = encoder.Finish(nullptr);

        _queue.Submit(1, &commands);
    }
}

Renderer::~Renderer() = default;

void Renderer::Render() const
{
    glfwPollEvents();


    glm::mat4 camMat{ BuildSRT(_cameraTransform) };

    _commonData.view = glm::inverse(camMat);
    _commonData.vp = _commonData.proj * _commonData.view;
    _commonData.time = glfwGetTime();
    _commonData.cameraPosition = _cameraTransform.translation;
    _queue.WriteBuffer(_commonBuf, 0, &_commonData, sizeof(_commonData));

    wgpu::CommandEncoderDescriptor ceDesc; 
    ceDesc.label = "Command encoder";

    wgpu::CommandEncoder encoder = _device.CreateCommandEncoder(&ceDesc);
    wgpu::TextureView backBufView = _swapChain.GetCurrentTextureView();

    _skyboxPass->Render(encoder, _msaaView);
    _pbrPass->Render(encoder, _msaaView, std::make_shared<wgpu::TextureView>(_hdrView)); // TODO: Seperate resolve into own pass.
    _hdrPass->Render(encoder, backBufView);
    _imGuiPass->Render(encoder, backBufView);

    wgpu::CommandBuffer commands = encoder.Finish(nullptr);

    _queue.Submit(1, &commands);
}

void Renderer::Resize(int32_t width, int32_t height)
{
    _width = width;
    _height = height;
    SetupRenderTarget();

    if(_hdrPass)
        _hdrPass->UpdateHDRView(_hdrView);

    _camera.ratio = _width / static_cast<float>(_height);

    _commonData.proj = glm::perspective(_camera.fov, _camera.ratio, _camera.zNear, _camera.zFar);
    _commonData.vp = _commonData.proj * _commonData.view;

    _queue.WriteBuffer(_commonBuf, 0, &_commonData, sizeof(_commonData));
}

void Renderer::DrawMesh(const Mesh& mesh, const Transform& transform)
{
    _pbrPass->DrawMesh(mesh, transform);
}

void Renderer::SetLight(uint32_t index, const glm::vec4& color, const glm::vec3& position)
{
    if (index >= MAX_POINT_LIGHTS)
    {
        std::cerr << "Trying to set a light out of bounds" << std::endl;
        return;
    }

    _commonData.pointLights[index].color = color;
    _commonData.pointLights[index].position = position;

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
    msaaDesc.format = wgpu::TextureFormat::RGBA16Float;
    msaaDesc.usage = wgpu::TextureUsage::RenderAttachment;
    _msaaTarget = _device.CreateTexture(&msaaDesc);
    
    _msaaView = _msaaTarget.CreateView();

    wgpu::TextureDescriptor hdrDesc{ msaaDesc };
    hdrDesc.label = "HDR RT";
    hdrDesc.sampleCount = 1;
    hdrDesc.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
    _hdrTarget = _device.CreateTexture(&hdrDesc);
    _hdrView = _hdrTarget.CreateView();  

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

    _depthStencilAttachment.view = _depthTextureView;
    _depthStencilAttachment.depthClearValue = 1.0f;
    _depthStencilAttachment.depthLoadOp = wgpu::LoadOp::Clear;
    _depthStencilAttachment.depthStoreOp = wgpu::StoreOp::Store;
    _depthStencilAttachment.depthReadOnly = false;
    _depthStencilAttachment.stencilClearValue = 0.f;
    _depthStencilAttachment.stencilLoadOp = wgpu::LoadOp::Undefined;
    _depthStencilAttachment.stencilStoreOp = wgpu::StoreOp::Undefined;
    _depthStencilAttachment.stencilReadOnly = true;
}

void Renderer::CreatePipelineAndBuffers()
{
    std::array<wgpu::BindGroupLayoutEntry, 1> bgLayoutEntry{};
    bgLayoutEntry[0].binding = 0;
    bgLayoutEntry[0].visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
    bgLayoutEntry[0].buffer.type = wgpu::BufferBindingType::Uniform;
    bgLayoutEntry[0].buffer.minBindingSize = sizeof(_commonData);

    wgpu::BindGroupLayoutDescriptor bgLayoutDesc{};
    bgLayoutDesc.label = "Common binding group layout";
    bgLayoutDesc.entryCount = bgLayoutEntry.size();
    bgLayoutDesc.entries = bgLayoutEntry.data();
    _commonBGLayout = _device.CreateBindGroupLayout(&bgLayoutDesc);

    std::array<wgpu::BindGroupEntry, 1> bgEntry{};
    assert(bgLayoutEntry.size() == bgEntry.size() && "Bindgroup entry descriptions don't match their sizes");

    bgEntry[0].binding = 0;
    bgEntry[0].buffer = _commonBuf;
    bgEntry[0].size = sizeof(_commonData);
     
    wgpu::BindGroupDescriptor bgDesc{};
    bgDesc.label = "Common bind group";
    bgDesc.layout = _commonBGLayout;
    bgDesc.entryCount = bgLayoutDesc.entryCount;
    bgDesc.entries = bgEntry.data();
    _commonBindGroup = _device.CreateBindGroup(&bgDesc); 
}

wgpu::Buffer Renderer::CreateBuffer(const void* data, unsigned long size, wgpu::BufferUsage usage, const char* label) const
{
    wgpu::BufferDescriptor desc{};
    desc.label = label;
    desc.usage = usage | wgpu::BufferUsage::CopyDst; 
    desc.size = (size + 3) & ~3; // Ensure multiple of 4.
    wgpu::Buffer buffer = _device.CreateBuffer(&desc);
    _queue.WriteBuffer(buffer, 0, data, desc.size);

    return buffer;
}

wgpu::ShaderModule Renderer::CreateShader(const std::string& path, const char* label) const
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
