#include <emscripten.h>
#include <iostream>
#include <emscripten/html5_webgpu.h>
#include <emscripten/html5.h>
#include <webgpu/webgpu_cpp.h>

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

wgpu::Instance instance;
wgpu::Device device;
wgpu::Queue queue;
wgpu::SwapChain swapChain;

wgpu::RenderPipeline pipeline;
wgpu::Buffer vertBuf; // vertex buffer with triangle position and colours
wgpu::Buffer indxBuf; // index buffer
wgpu::Buffer uRotBuf; // uniform buffer (containing the rotation angle)
wgpu::BindGroup bindGroup;

float rotDeg = 0.0f;

wgpu::SwapChain CreateSwapChain()
{
    wgpu::SurfaceDescriptorFromCanvasHTMLSelector canvasDesc{};
    canvasDesc.sType = wgpu::SType::SurfaceDescriptorFromCanvasHTMLSelector;
    canvasDesc.selector = "canvas";

    wgpu::SurfaceDescriptor surfDesc{};
    surfDesc.nextInChain = reinterpret_cast<wgpu::ChainedStruct*>(&canvasDesc);
    
    wgpu::Surface surface = instance.CreateSurface(&surfDesc);

    wgpu::SwapChainDescriptor swapDesc{};
    swapDesc.usage = wgpu::TextureUsage::RenderAttachment;
    swapDesc.format = SWAPCHAIN_FORMAT;
    swapDesc.width = 800;
    swapDesc.height = 450;
    swapDesc.presentMode = wgpu::PresentMode::Fifo;

    return device.CreateSwapChain(surface, &swapDesc);
}

wgpu::Buffer CreateBuffer(const void* data, unsigned long size, wgpu::BufferUsage usage)
{
    wgpu::BufferDescriptor desc{};
    desc.usage = usage | wgpu::BufferUsage::CopyDst;
    desc.size = size;
    wgpu::Buffer buffer = device.CreateBuffer(&desc);
    queue.WriteBuffer(buffer, 0, data, size);

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
    return device.CreateShaderModule(&desc);
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
    wgpu::BindGroupLayout bindGroupLayout = device.CreateBindGroupLayout(&bglDesc);

    wgpu::PipelineLayoutDescriptor layoutDesc{};
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts = &bindGroupLayout;
    wgpu::PipelineLayout pipelineLayout = device.CreatePipelineLayout(&layoutDesc);

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

    pipeline = device.CreateRenderPipeline(&rpDesc);

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

    vertBuf = CreateBuffer(vertData, sizeof(vertData), wgpu::BufferUsage::Vertex);
    indxBuf = CreateBuffer(indxData, sizeof(indxData), wgpu::BufferUsage::Index);

    uRotBuf = CreateBuffer(&rotDeg, sizeof(rotDeg), wgpu::BufferUsage::Uniform);

    wgpu::BindGroupEntry bgEntry{};
    bgEntry.binding = 0;
    bgEntry.buffer = uRotBuf;
    bgEntry.offset = 0;
    bgEntry.size = sizeof(rotDeg);

    wgpu::BindGroupDescriptor bgDesc{};
    bgDesc.layout = bindGroupLayout;
    bgDesc.entryCount = 1;
    bgDesc.entries = &bgEntry;

    bindGroup = device.CreateBindGroup(&bgDesc);
}

void Render(double time)
{
    wgpu::TextureView backBufView = swapChain.GetCurrentTextureView();

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

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder(nullptr);
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);

    rotDeg += 0.1f;
    queue.WriteBuffer(uRotBuf, 0, &rotDeg, sizeof(rotDeg));

    pass.SetPipeline(pipeline);
    pass.SetBindGroup(0, bindGroup, 0, 0);
    pass.SetVertexBuffer(0, vertBuf, 0, wgpu::kWholeSize);
    pass.SetIndexBuffer(indxBuf, wgpu::IndexFormat::Uint16, 0, wgpu::kWholeSize);

    pass.DrawIndexed(3, 1, 0, 0, 0);

    pass.End();

    wgpu::CommandBuffer commands = encoder.Finish(nullptr);

    queue.Submit(1, &commands);

    swapChain.Present();
}

EM_BOOL em_render(double time, void* userData)
{
    Render(time);
    return true;
}

EM_JS(void, glue_preint, (), {
    var entry = __glue_main_;
    if (entry) {
        /*
         * None of the WebGPU properties appear to survive Closure, including
         * Emscripten's own `preinitializedWebGPUDevice` (which from looking at
         *`library_html5` is probably designed to be inited in script before
         * loading the Wasm).
         */
        if (navigator["gpu"]) {
            navigator["gpu"]["requestAdapter"]().then(function (adapter) {
                adapter["requestDevice"]().then( function (device) {
                    Module["preinitializedWebGPUDevice"] = device;
                    entry();
                });
            }, function () {
                console.error("No WebGPU adapter; not starting");
            });
        } else {
            console.error("No support for WebGPU; not starting");
        }
    } else {
        console.error("Entry point not found; unable to start");
    }
});

#ifndef KEEP_IN_MODULE
#define KEEP_IN_MODULE extern "C" __attribute__((used, visibility("default")))
#endif

int __main__(int argc, char* argv);

KEEP_IN_MODULE void _glue_main_()
{
    __main__(0, nullptr);
}

int main() {
    glue_preint();

    return 0;
}

int __main__(int argc, char* argv)
{
    instance = wgpu::CreateInstance(nullptr);
    device = wgpu::Device(emscripten_webgpu_get_device());
    queue = device.GetQueue();
    swapChain = CreateSwapChain();
    CreatePipelineAndBuffers();

    // Begin render loop.
    emscripten_request_animation_frame_loop(em_render, nullptr);

    return 0;
}
