#pragma once

#include <webgpu/webgpu_cpp.h>
class Renderer;

class RenderPass
{
public:
    RenderPass(Renderer& renderer, wgpu::TextureFormat renderFormat) : _renderer(renderer), _renderFormat(renderFormat) {}
    virtual ~RenderPass();

    virtual void Render(const wgpu::CommandEncoder& encoder, const wgpu::TextureView& renderTarget, std::shared_ptr<const wgpu::TextureView> resolveTarget = nullptr) = 0;

protected:
    Renderer& _renderer;
    const wgpu::TextureFormat _renderFormat{ wgpu::TextureFormat::Undefined };
};
