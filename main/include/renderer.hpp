#pragma once
#include <webgpu/webgpu_cpp.h>
#include <glm.hpp>
#include <queue>
#include <GLFW/glfw3.h>
#include <tiny_gltf.h>

#include "aliases.hpp"
#include "camera.hpp"
#include "mesh.hpp"
#include "transform.hpp"

constexpr uint32_t MAX_POINT_LIGHTS{ 4 };

class PBRPass;

class Renderer
{
public:

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
    void BeginEditor() const;
    void EndEditor() const;
    void Resize(int32_t width, int32_t height);

    void DrawMesh(const Mesh& mesh, const Transform& transform);
    void SetLight(uint32_t index, const glm::vec4& color, const glm::vec3& position);

    Camera& GetCamera() { return _camera; }
    Transform& GetCameraTransform() { return _cameraTransform; }
    wgpu::Buffer CreateBuffer(const void* data, unsigned long size, wgpu::BufferUsage usage, const char* label);
    wgpu::ShaderModule CreateShader(const std::string& path, const char* label = nullptr);
    const wgpu::Device& Device() const { return _device; }
    const wgpu::Queue& Queue() const { return _queue; }
    const wgpu::TextureView HDRView() const { return _hdrView; }
    const wgpu::TextureView MSAAView() const { return _msaaView; }
    const wgpu::TextureFormat DEPTH_STENCIL_FORMAT{ wgpu::TextureFormat::Depth24Plus };
    const wgpu::RenderPassDepthStencilAttachment& DepthStencilAttachment() const { return _depthStencilAttachment; }
    glm::mat4 BuildSRT(const Transform& transform) const; // TODO: Maybe move out of here.
    const wgpu::BindGroup CommonBindGroup() const { return _commonBindGroup; }
    const wgpu::BindGroupLayout CommonBindGroupLayout() const { return _commonBGLayout; }
    const PBRPass& PBRRenderPass() const { return *_pbrPass; }

    struct PointLight
    {
        glm::vec4 color;
        glm::vec3 position;
        float radius;
    };

private:
    friend Mesh;

    void SetupRenderTarget();
    void CreatePipelineAndBuffers();
    void SetupHDRPipeline();
    wgpu::Texture CreateTexture(const tinygltf::Image& image, const std::vector<uint8_t>& data, uint32_t mipLevelCount, const char* label = nullptr);
    bool SetupImGui();

    std::unique_ptr<PBRPass> _pbrPass;

    wgpu::Adapter _adapter;
    wgpu::Instance _instance;
    wgpu::Device _device;
    wgpu::Queue _queue;
    GLFWwindow* _window;
    wgpu::SwapChain _swapChain;
    wgpu::Texture _msaaTarget;
    wgpu::TextureView _msaaView;
    wgpu::Sampler _hdrSampler;
    wgpu::Texture _hdrTarget;
    wgpu::TextureView _hdrView;
    wgpu::Texture _depthTexture;
    wgpu::TextureView _depthTextureView;

    wgpu::RenderPipeline _pipelineHDR;
    wgpu::Buffer _commonBuf;
    wgpu::RenderPassDepthStencilAttachment _depthStencilAttachment;

    wgpu::BindGroupLayout _commonBGLayout;
    wgpu::BindGroupLayout _hdrBindGroupLayout;

    wgpu::BindGroup _commonBindGroup;
    wgpu::BindGroup _hdrBindGroup;

    int32_t _width = 1280;
    int32_t _height = 720;

    wgpu::TextureFormat _swapChainFormat{ wgpu::TextureFormat::BGRA8Unorm };  

    Camera _camera;
    Transform _cameraTransform;


    struct Common
    {
        glm::mat4 proj;
        glm::mat4 view;
        glm::mat4 vp; 

        glm::vec3 lightDirection;
        float time;

        glm::vec3 lightColor;
        float normalMapStrength;

        std::array<PointLight, MAX_POINT_LIGHTS> pointLights;
        
        glm::vec3 cameraPosition;
        float _padding;
    };


    mutable Common _commonData;

};
