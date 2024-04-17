#pragma once
#include <webgpu/webgpu_cpp.h>
#include <glm.hpp>
#include <queue>
#include <GLFW/glfw3.h>

#include "aliases.hpp"
#include "camera.hpp"
#include "mesh.hpp"
#include "transform.hpp"

constexpr uint32_t MAX_INSTANCES{ 4096 };

class Renderer
{
public:
    struct Vertex
    {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec3 color;
    };

    struct DeviceResources
    {
        wgpu::Adapter adapter;
        wgpu::Instance instance;
        wgpu::Device device;
        wgpu::Queue queue;
    };

    Renderer(DeviceResources deviceResources, GLFWwindow* window, int32_t width, int32_t height);
    ~Renderer();

    void Render() const;
    void Resize(int32_t width, int32_t height);

    void DrawMesh(const Mesh& mesh, const Transform& transform);

    Camera& GetCamera() { return _camera; }
    Transform& GetCameraTransform() { return _cameraTransform; }

private:
    friend Mesh;

    void SetupRenderTarget();
    void CreatePipelineAndBuffers();
    wgpu::Buffer CreateBuffer(const void* data, unsigned long size, wgpu::BufferUsage usage, const char* label);
    wgpu::ShaderModule CreateShader(const std::string& path, const char* label = nullptr);
    glm::mat4 BuildSRT(const Transform& transform) const;
    bool SetupImGui();

    wgpu::Adapter _adapter;
    wgpu::Instance _instance;
    wgpu::Device _device;
    wgpu::Queue _queue;
    GLFWwindow* _window;
    wgpu::SwapChain _swapChain;
    wgpu::Texture _msaaTarget;
    wgpu::TextureView _msaaView;
    wgpu::Texture _depthTexture;
    wgpu::TextureView _depthTextureView;

    wgpu::RenderPipeline _pipeline;
    wgpu::Buffer _commonBuf;
    wgpu::Buffer _instanceBuf;
    wgpu::BindGroupLayout _bgLayout;
    wgpu::BindGroup _bindGroup;

    int32_t _width = 1280;
    int32_t _height = 720;

    wgpu::TextureFormat _swapChainFormat{ wgpu::TextureFormat::BGRA8Unorm };
    const wgpu::TextureFormat DEPTH_STENCIL_FORMAT{ wgpu::TextureFormat::Depth24Plus };

    Camera _camera;
    Transform _cameraTransform;

    uint32_t _uniformStride;

    struct Common
    {
        glm::mat4 proj;
        glm::mat4 view;
        glm::mat4 vp;
        float time;

        float _padding[3];
    };

    struct Instance
    {
        glm::mat4 model;
    };

    mutable Common _commonData;

    mutable std::queue<std::tuple<Mesh, Transform>> _drawings;
};
