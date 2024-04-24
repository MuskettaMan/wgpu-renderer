#pragma once

#include "graphics/render_pass.hpp"

class ImGuiPass : public RenderPass
{
public:
    ImGuiPass(Renderer& renderer);
    virtual ~ImGuiPass();
    virtual void Render(const wgpu::CommandEncoder& encoder, const wgpu::TextureView& renderTarget, std::shared_ptr<const wgpu::TextureView> resolveTarget = nullptr) override;
};
