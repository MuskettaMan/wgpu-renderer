#pragma once

#include <webgpu/webgpu_cpp.h>
class Renderer;

class RenderPass
{
public:
    RenderPass(Renderer& renderer) : _renderer(renderer) {}
    virtual ~RenderPass();

    virtual void Render(const wgpu::CommandEncoder& encoder) = 0;

protected:
    Renderer& _renderer;
};
