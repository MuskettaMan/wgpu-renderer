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
#include <cmath>

using namespace std::literals::chrono_literals;

std::unique_ptr<Renderer> g_renderer;
Renderer::DeviceResources g_resources;

entt::registry g_registry;

int32_t g_mouseX;
int32_t g_mouseY;
int32_t g_mouseXPrev;
int32_t g_mouseYPrev;
float g_mouseScrollDelta{ 0.0f };

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
    GLFWwindow* window = reinterpret_cast<GLFWwindow*>(userData);
    if(!g_renderer && g_resources.device.Get())
    {
        int32_t width, height;
        glfwGetWindowSize(window, &width, &height);

        g_renderer = std::make_unique<Renderer>(g_resources, window, width, height);
        initialized = true;

        for(int i = 0; i < 1; ++i)
        {
            entt::entity entity = g_registry.create();
            Transform& transform = g_registry.emplace<Transform>(entity);
            auto& mesh = g_registry.emplace<Mesh>(entity);

            transform.scale = glm::vec3{ 2.0f };
            transform.translation = glm::vec3{ i, 0.0f, 0.0f } * transform.scale;

            auto optMesh = Mesh::CreateMesh("assets/models/DamagedHelmet.gltf", *g_renderer);
            if(!optMesh)
            {
                std::cout << "Failed getting mesh" << std::endl;
            }
            else
            {
                mesh = optMesh.value();
            }

        }
    }
    else if(!initialized)
    {
        return true;
    }
    int32_t mouseXDelta{};
    int32_t mouseYDelta{};
    {
        double xPos, yPos;
        glfwGetCursorPos(window, &xPos, &yPos); 
        g_mouseX = static_cast<int32_t>(xPos);
        g_mouseY = static_cast<int32_t>(yPos);

        mouseXDelta = g_mouseXPrev - g_mouseX;
        mouseYDelta = g_mouseYPrev - g_mouseY;
    }


    {
        int mbLeftState = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);

        static float theta = 0.0f;
        static float phi = 0.0f;
        if(mbLeftState == GLFW_PRESS)
        {
            theta += mouseXDelta * 0.002f;
            phi += mouseYDelta * -0.002f;
            phi = std::clamp(phi, -glm::pi<float>() / 2, glm::pi<float>() / 2);
        }
        static float radius = 5.0f;

        radius += g_mouseScrollDelta * -0.1f;
        radius = std::clamp(radius, 1.0f, 10.0f);

        g_mouseScrollDelta = std::lerp(g_mouseScrollDelta, 0.0f, 0.25f);

        g_renderer->GetCameraTransform().translation = glm::vec3{ cos(phi) * cos(theta) * radius, sin(phi) * radius, cos(phi) * sin(theta) * radius};

        glm::vec3 direction{ glm::vec3{ 0.0f } - g_renderer->GetCameraTransform().translation };

        g_renderer->GetCameraTransform().rotation = glm::normalize(glm::quatLookAt(direction, glm::vec3{ 0.0f, 1.0f, 0.0f }));
    }


    auto view = g_registry.view<Mesh, Transform>();
    for(auto&& [entity, mesh, transform] : view.each())
    {
        g_renderer->DrawMesh(mesh, transform);
    }

    g_renderer->Render();

    g_mouseXPrev = g_mouseX;
    g_mouseYPrev = g_mouseY;

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

    glfwSetScrollCallback(window, [](GLFWwindow* window, double xDelta, double yDelta)
    {
        g_mouseScrollDelta = yDelta;
    });


    RequestDeviceResources(g_resources);
    emscripten_request_animation_frame_loop(em_render, window);

    return 0;
}
