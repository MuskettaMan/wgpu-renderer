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

constexpr uint32_t MAX_INSTANCES{ 4096 };
constexpr uint32_t MAX_POINT_LIGHTS{ 4 };

class Renderer
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
    wgpu::Buffer CreateBuffer(const void* data, unsigned long size, wgpu::BufferUsage usage, const char* label);
    wgpu::Texture CreateTexture(const tinygltf::Image& image, const std::vector<uint8_t>& data, uint32_t mipLevelCount, const char* label = nullptr);
    wgpu::ShaderModule CreateShader(const std::string& path, const char* label = nullptr);
    glm::mat4 BuildSRT(const Transform& transform) const;
    bool SetupImGui();

    wgpu::Device& GetDevice() { return _device; }
    wgpu::BindGroupLayout& GetMeshBindGroupLayout() { return _bgLayouts.mesh; }

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

    wgpu::RenderPipeline _pipeline;
    wgpu::RenderPipeline _pipelineHDR;
    wgpu::Buffer _commonBuf;
    wgpu::Buffer _instanceBuf;

    struct
    { 
        wgpu::BindGroupLayout standard;
        wgpu::BindGroupLayout mesh;
    } _bgLayouts;

    wgpu::BindGroupLayout _hdrBindGroupLayout;

    wgpu::BindGroup _standardBindGroup;
    wgpu::BindGroup _hdrBindGroup;

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

        glm::vec3 lightDirection;
        float time;

        glm::vec3 lightColor;
        float normalMapStrength;

        std::array<PointLight, MAX_POINT_LIGHTS> pointLights;
        
        glm::vec3 cameraPosition;
        float _padding;
    };

    struct Instance
    {
        glm::mat4 model;
        glm::mat4 transInvModel;
    };

    mutable Common _commonData;

    mutable std::queue<std::tuple<Mesh, Transform>> _drawings;
};
