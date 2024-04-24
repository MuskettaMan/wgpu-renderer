#pragma once

#include "render_pass.hpp"
#include "renderer.hpp"
#include <webgpu/webgpu_cpp.h>
#include <glm.hpp>
#include <queue>

constexpr uint32_t MAX_INSTANCES{ 4096 };

class PBRPass : public RenderPass
{
public:
    struct Vertex
    {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec3 tangent;
        glm::vec3 bitangent;
        glm::vec2 uv;
    };

    PBRPass(Renderer& renderer);
    virtual ~PBRPass();

    virtual void Render(const wgpu::CommandEncoder& encoder) override;

    void DrawMesh(const Mesh& mesh, const Transform& transform) const;
    const wgpu::BindGroupLayout& PBRBindGroupLayout() const { return _pbrBindGroupLayout; }

private:
    struct Instance
    {
        glm::mat4 model;
        glm::mat4 transInvModel;
    };

    wgpu::BindGroupLayout _pbrBindGroupLayout;
    wgpu::BindGroupLayout _instanceBindGroupLayout;
    wgpu::BindGroup _instanceBindGroup;
    uint32_t _uniformStride;
    wgpu::Buffer _instanceBuffer;
    wgpu::RenderPipeline _pipeline;
    wgpu::ShaderModule _vertModule;
    wgpu::ShaderModule _fragModule;

    mutable std::queue<std::tuple<Mesh, Transform>> _drawings;
};
