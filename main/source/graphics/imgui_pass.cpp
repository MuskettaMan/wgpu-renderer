#include "graphics/imgui_pass.hpp"
#include "renderer.hpp"
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_wgpu.h>
#include "enum_util.hpp"
#include <iostream>

ImGuiPass::ImGuiPass(Renderer& renderer) : RenderPass(renderer, renderer.SwapChainFormat())
{
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOther(_renderer.Window(), true);
    ImGui_ImplGlfw_InstallEmscriptenCanvasResizeCallback("#canvas");

    ImGui_ImplWGPU_InitInfo imguiInitInfo{};
    imguiInitInfo.DepthStencilFormat = conv_enum<WGPUTextureFormat>(wgpu::TextureFormat::Undefined);
    imguiInitInfo.Device = _renderer.Device().Get();
    imguiInitInfo.NumFramesInFlight = 3;
    imguiInitInfo.PipelineMultisampleState = { nullptr, 1,  0xFF'FF'FF'FF, false };
    imguiInitInfo.RenderTargetFormat = conv_enum<WGPUTextureFormat>(_renderFormat);
    ImGui_ImplWGPU_Init(&imguiInitInfo);
    ImGui_ImplWGPU_CreateDeviceObjects();

    // Prevent opening .ini with emscripten file system.
    io.IniFilename = nullptr;
}

ImGuiPass::~ImGuiPass() = default;

void ImGuiPass::Render(const wgpu::CommandEncoder& encoder, const wgpu::TextureView& renderTarget, std::shared_ptr<const wgpu::TextureView> resolveTarget)
{
    wgpu::RenderPassColorAttachment imguiColorDesc{};
    imguiColorDesc.view = renderTarget;
    imguiColorDesc.loadOp = wgpu::LoadOp::Load;
    imguiColorDesc.storeOp = wgpu::StoreOp::Store;

    wgpu::RenderPassDescriptor imguiPassDesc{};
    imguiPassDesc.label = "ImGui render pass";
    imguiPassDesc.colorAttachmentCount = 1;
    imguiPassDesc.colorAttachments = &imguiColorDesc;
    imguiPassDesc.depthStencilAttachment = nullptr;

    wgpu::RenderPassEncoder imguiPass = encoder.BeginRenderPass(&imguiPassDesc);
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), imguiPass.Get());
    imguiPass.End();
}
