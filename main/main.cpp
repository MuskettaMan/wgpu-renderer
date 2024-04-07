#include <emscripten.h>
#include <iostream>
#include <emscripten/html5_webgpu.h>
#include <emscripten/html5.h>
#include <webgpu/webgpu_cpp.h>
#include <magic_enum.hpp>

using uint32_t = unsigned int;
using uint16_t = unsigned short;

constexpr wgpu::TextureFormat SWAPCHAIN_FORMAT{ wgpu::TextureFormat::BGRA8Unorm };

static char const triangle_vert_wgsl[] = R"(
    struct VertexIn {
        @location(0) aPos : vec2<f32>,
        @location(1) aCol : vec3<f32>
    }
    struct VertexOut {
        @location(0) vCol : vec3<f32>,
        @builtin(position) Position : vec4<f32>
    }
    struct Rotation {
        @location(0) degs : f32
    }
    @group(0) @binding(0) var<uniform> uRot : Rotation;
    @vertex
    fn main(input : VertexIn) -> VertexOut {
        var rads : f32 = radians(uRot.degs);
        var cosA : f32 = cos(rads);
        var sinA : f32 = sin(rads);
        var rot : mat3x3<f32> = mat3x3<f32>(
            vec3<f32>( cosA, sinA, 0.0),
            vec3<f32>(-sinA, cosA, 0.0),
            vec3<f32>( 0.0,  0.0,  1.0));
        var output : VertexOut;
        output.Position = vec4<f32>(rot * vec3<f32>(input.aPos, 1.0), 1.0);
        output.vCol = input.aCol;
        return output;
    }
)";
static char const triangle_frag_wgsl[] = R"(
    @fragment
    fn main(@location(0) vCol : vec3<f32>) -> @location(0) vec4<f32> {
        return vec4<f32>(vCol, 1.0);
    }
)";

wgpu::Adapter g_adapter;
wgpu::Instance g_instance;
wgpu::Device g_device;
wgpu::Queue g_queue;
wgpu::SwapChain g_swapChain;

wgpu::RenderPipeline g_pipeline;
wgpu::Buffer g_vertBuf; // vertex buffer with triangle position and colours
wgpu::Buffer g_indxBuf; // index buffer
wgpu::Buffer g_uRotBuf; // uniform buffer (containing the rotation angle)
wgpu::BindGroup g_bindGroup;

float rotDeg = 0.0f;

wgpu::SwapChain CreateSwapChain()
{
    wgpu::SurfaceDescriptorFromCanvasHTMLSelector canvasDesc{};
    canvasDesc.sType = wgpu::SType::SurfaceDescriptorFromCanvasHTMLSelector;
    canvasDesc.selector = "canvas";

    wgpu::SurfaceDescriptor surfDesc{};
    surfDesc.nextInChain = reinterpret_cast<wgpu::ChainedStruct*>(&canvasDesc);
    
    wgpu::Surface surface = g_instance.CreateSurface(&surfDesc);

    wgpu::SwapChainDescriptor swapDesc{};
    swapDesc.usage = wgpu::TextureUsage::RenderAttachment;
    swapDesc.format = SWAPCHAIN_FORMAT;
    swapDesc.width = 800;
    swapDesc.height = 450;
    swapDesc.presentMode = wgpu::PresentMode::Fifo;

    return g_device.CreateSwapChain(surface, &swapDesc);
}

wgpu::Buffer CreateBuffer(const void* data, unsigned long size, wgpu::BufferUsage usage)
{
    wgpu::BufferDescriptor desc{};
    desc.usage = usage | wgpu::BufferUsage::CopyDst;
    desc.size = size;
    wgpu::Buffer buffer = g_device.CreateBuffer(&desc);
    g_queue.WriteBuffer(buffer, 0, data, size);

    return buffer;
}

wgpu::ShaderModule CreateShader(const char* const code, const char* label = nullptr)
{
    wgpu::ShaderModuleWGSLDescriptor wgsl{};
    wgsl.sType = wgpu::SType::ShaderModuleWGSLDescriptor;
    wgsl.code = code;
    wgpu::ShaderModuleDescriptor desc{};
    desc.nextInChain = reinterpret_cast<wgpu::ChainedStruct*>(&wgsl);
    desc.label = label;
    return g_device.CreateShaderModule(&desc);
}

void CreatePipelineAndBuffers()
{
    wgpu::ShaderModule vertModule = CreateShader(triangle_vert_wgsl);
    wgpu::ShaderModule fragModule = CreateShader(triangle_frag_wgsl);

    wgpu::BufferBindingLayout buf{};
    buf.type = wgpu::BufferBindingType::Uniform;

    wgpu::BindGroupLayoutEntry bglEntry{};
    bglEntry.binding = 0;
    bglEntry.visibility = wgpu::ShaderStage::Vertex;
    bglEntry.buffer = buf;

    wgpu::BindGroupLayoutDescriptor bglDesc{};
    bglDesc.entryCount = 1;
    bglDesc.entries = &bglEntry;
    wgpu::BindGroupLayout bindGroupLayout = g_device.CreateBindGroupLayout(&bglDesc);

    wgpu::PipelineLayoutDescriptor layoutDesc{};
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts = &bindGroupLayout;
    wgpu::PipelineLayout pipelineLayout = g_device.CreatePipelineLayout(&layoutDesc);

    wgpu::VertexAttribute vertAttrs[2] = {};
    vertAttrs[0].format = wgpu::VertexFormat::Float32x2;
    vertAttrs[0].offset = 0;
    vertAttrs[0].shaderLocation = 0;
    vertAttrs[1].format = wgpu::VertexFormat::Float32x3;
    vertAttrs[1].offset = sizeof(float) * 2;
    vertAttrs[1].shaderLocation = 1;

    wgpu::VertexBufferLayout vertexBufferLayout{};
    vertexBufferLayout.arrayStride = sizeof(float) * 5;
    vertexBufferLayout.attributeCount = 2;
    vertexBufferLayout.attributes = vertAttrs;

    wgpu::BlendState blend{};
    blend.color.operation = wgpu::BlendOperation::Add;
    blend.color.srcFactor = wgpu::BlendFactor::One;
    blend.color.dstFactor = wgpu::BlendFactor::One;
    blend.alpha.operation = wgpu::BlendOperation::Add;
    blend.alpha.srcFactor = wgpu::BlendFactor::One;
    blend.alpha.dstFactor = wgpu::BlendFactor::One;

    wgpu::ColorTargetState colorTarget{};
    colorTarget.format = SWAPCHAIN_FORMAT;
    colorTarget.blend = &blend;
    colorTarget.writeMask = wgpu::ColorWriteMask::All;

    wgpu::FragmentState fragment{};
    fragment.module = fragModule;
    fragment.entryPoint = "main";
    fragment.targetCount = 1;
    fragment.targets = &colorTarget;

    wgpu::RenderPipelineDescriptor rpDesc{};
    rpDesc.fragment = &fragment;
    rpDesc.layout = pipelineLayout;
    rpDesc.depthStencil = nullptr;

    rpDesc.vertex.module = vertModule;
    rpDesc.vertex.entryPoint = "main";
    rpDesc.vertex.bufferCount = 1;
    rpDesc.vertex.buffers = &vertexBufferLayout;

    rpDesc.multisample.count = 1;
    rpDesc.multisample.mask = 0xFF'FF'FF'FF;
    rpDesc.multisample.alphaToCoverageEnabled = false;

    rpDesc.primitive.frontFace = wgpu::FrontFace::CCW;
    rpDesc.primitive.cullMode = wgpu::CullMode::None;
    rpDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    rpDesc.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined;

    g_pipeline = g_device.CreateRenderPipeline(&rpDesc);

    // create the buffers (x, y, r, g, b)
    float const vertData[] = {
        -0.8f, -0.8f, 0.0f, 0.0f, 1.0f, // BL
         0.8f, -0.8f, 0.0f, 1.0f, 0.0f, // BR
        -0.0f,  0.8f, 1.0f, 0.0f, 0.0f, // top
    };
    uint16_t const indxData[] = {
        0, 1, 2,
        0 // padding (better way of doing this?)
    };

    g_vertBuf = CreateBuffer(vertData, sizeof(vertData), wgpu::BufferUsage::Vertex);
    g_indxBuf = CreateBuffer(indxData, sizeof(indxData), wgpu::BufferUsage::Index);

    g_uRotBuf = CreateBuffer(&rotDeg, sizeof(rotDeg), wgpu::BufferUsage::Uniform);

    wgpu::BindGroupEntry bgEntry{};
    bgEntry.binding = 0;
    bgEntry.buffer = g_uRotBuf;
    bgEntry.offset = 0;
    bgEntry.size = sizeof(rotDeg);

    wgpu::BindGroupDescriptor bgDesc{};
    bgDesc.layout = bindGroupLayout;
    bgDesc.entryCount = 1;
    bgDesc.entries = &bgEntry;

    g_bindGroup = g_device.CreateBindGroup(&bgDesc);
}

void Render(double time)
{
    wgpu::TextureView backBufView = g_swapChain.GetCurrentTextureView();

    wgpu::RenderPassColorAttachment colorDesc{};
    colorDesc.view = backBufView;
    colorDesc.depthSlice = wgpu::kDepthSliceUndefined;
    colorDesc.loadOp = wgpu::LoadOp::Clear;
    colorDesc.storeOp = wgpu::StoreOp::Store;
    colorDesc.clearValue.r = 0.3f;
    colorDesc.clearValue.g = 0.3f;
    colorDesc.clearValue.b = 0.3f;
    colorDesc.clearValue.a = 1.0f;

    wgpu::RenderPassDescriptor renderPass{};
    renderPass.colorAttachmentCount = 1;
    renderPass.colorAttachments = &colorDesc;

    wgpu::CommandEncoder encoder = g_device.CreateCommandEncoder(nullptr);
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);

    rotDeg += 0.1f;
    g_queue.WriteBuffer(g_uRotBuf, 0, &rotDeg, sizeof(rotDeg));

    pass.SetPipeline(g_pipeline);
    pass.SetBindGroup(0, g_bindGroup, 0, 0);
    pass.SetVertexBuffer(0, g_vertBuf, 0, wgpu::kWholeSize);
    pass.SetIndexBuffer(g_indxBuf, wgpu::IndexFormat::Uint16, 0, wgpu::kWholeSize);

    pass.DrawIndexed(3, 1, 0, 0, 0);

    pass.End();

    wgpu::CommandBuffer commands = encoder.Finish(nullptr);

    g_queue.Submit(1, &commands);
}

EM_BOOL em_render(double time, void* userData)
{
    Render(time);
    return true;
}

template <typename To, typename From>
constexpr To conv_enum(From value)
{
    return *reinterpret_cast<To*>(&value);
}

template <typename To, typename From>
constexpr std::string_view conv_enum_str(From value)
{
    return magic_enum::enum_name(conv_enum<To>(value));
}

int main(int argc, char** argv)
{
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


            g_swapChain = CreateSwapChain();
            CreatePipelineAndBuffers();

            // Begin render loop.
            emscripten_request_animation_frame_loop(em_render, nullptr);
        }, nullptr);

    }, nullptr);


    return 0;
}
