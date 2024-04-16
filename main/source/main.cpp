#include <emscripten.h>
#include <iostream>
#include <emscripten/html5_webgpu.h>
#include <emscripten/html5.h>
#include <webgpu/webgpu_cpp.h>
#include <imgui.h>
#include <thread>
#include <GLFW/glfw3.h>

#include "aliases.hpp"
#include "renderer.hpp"
#include <entt.hpp>

using namespace std::literals::chrono_literals;

std::unique_ptr<Renderer> g_renderer;
Renderer::DeviceResources g_resources;

entt::registry g_registry;

void RequestDeviceResources(Renderer::DeviceResources& deviceResources)
{
    deviceResources.instance = wgpu::CreateInstance(nullptr);

    wgpu::RequestAdapterOptions options{};
    options.powerPreference = wgpu::PowerPreference::HighPerformance;

    deviceResources.instance.RequestAdapter(&options, [](WGPURequestAdapterStatus status, WGPUAdapter adapter, char const* message, void* userData)
    {
        Renderer::DeviceResources* deviceResources {reinterpret_cast<Renderer::DeviceResources*>(userData)};
        if(status != WGPURequestAdapterStatus_Success)
        {
            std::cout << "Failed requesting adapter: " << message << std::endl;
            return;
        }

        deviceResources->adapter = wgpu::Adapter(adapter);

        wgpu::DeviceDescriptor deviceDesc{};
        deviceDesc.label = "Device";
        deviceDesc.requiredFeatureCount = 0;
        deviceDesc.requiredLimits = nullptr;
        deviceDesc.nextInChain = nullptr;
        deviceDesc.defaultQueue.nextInChain = nullptr;
        deviceDesc.defaultQueue.label = "Default queue";

        deviceResources->adapter.RequestDevice(&deviceDesc, [](WGPURequestDeviceStatus status, WGPUDeviceImpl* deviceHandle, char const* message, void* userData)
        {
            Renderer::DeviceResources* deviceResources{ reinterpret_cast<Renderer::DeviceResources*>(userData) };
            if (status != WGPURequestDeviceStatus_Success)
            {
                std::cout << "Failed requesting device: " << message << std::endl;
                return;
            }

            deviceResources->device = wgpu::Device(deviceHandle);
            deviceResources->queue = deviceResources->device.GetQueue();

        }, deviceResources);

    }, &deviceResources);
}

EM_BOOL em_render(double time, void* userData)
{
    static bool initialized = false;
    if(!g_renderer && g_resources.device.Get())
    {
        GLFWwindow* window = reinterpret_cast<GLFWwindow*>(userData);
        int32_t width, height;
        glfwGetWindowSize(window, &width, &height);

        g_renderer = std::make_unique<Renderer>(g_resources, window, width, height);
        initialized = true;

        // create the buffers (x, y, r, g, b)
        std::vector<Renderer::Vertex> vertices = {
            Renderer::Vertex{glm::vec3{ -0.5f, -0.3f, -0.5f }, glm::vec3{ 1.0f, 1.0f, 1.0f } },
            Renderer::Vertex{glm::vec3{ +0.5f, -0.3f, -0.5f }, glm::vec3{ 1.0f, 1.0f, 1.0f } },
            Renderer::Vertex{glm::vec3{ +0.5f, -0.3f, +0.5f }, glm::vec3{ 1.0f, 1.0f, 1.0f } },
            Renderer::Vertex{glm::vec3{ -0.5f, -0.3f, +0.5f }, glm::vec3{ 1.0f, 1.0f, 1.0f } },

            Renderer::Vertex{glm::vec3{ +0.0f, +0.5f, +0.0f }, glm::vec3{ 0.5f, 0.5f, 0.5f } }
        };
        std::vector<uint32_t> indices = {
            0, 1, 2,
            0, 2, 3,
            0, 1, 4,
            1, 2, 4,
            2, 3, 4,
            3, 0, 4,
        };

        for(int i = 0; i < 2; ++i)
        {
            entt::entity entity = g_registry.create();
            Transform& transform = g_registry.emplace<Transform>(entity);
            auto& mesh = g_registry.emplace<Mesh>(entity);

            transform.scale = glm::vec3{ 0.1f };
            transform.translation = glm::vec3{ i - 2.0f, 0.0f, 0.0f } * transform.scale;

            auto optMesh = Mesh::CreateMesh("assets/models/DamagedHelmet.gltf", *g_renderer);
            if(!optMesh)
            {
                std::cout << "Failed getting mesh" << std::endl;
            }
        }
    }
    else if(!initialized)
    {
        return true;
    }

    auto view = g_registry.view<Mesh, Transform>();
    for(auto&& [entity, mesh, transform] : view.each())
    {
        g_renderer->DrawMesh(mesh, transform);
    }

    g_renderer->Render();
    return true;
}

int main(int argc, char** argv)
{
    double width, height;
    emscripten_get_element_css_size("#canvas", &width, &height);

    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(width, height, "window title", nullptr, nullptr);

    glfwShowWindow(window);

    glfwSetWindowSizeCallback(window, [](GLFWwindow* window, int width, int height)
    {
        g_renderer->Resize(width, height);
    });


    RequestDeviceResources(g_resources);
    emscripten_request_animation_frame_loop(em_render, window);

    return 0;
}
