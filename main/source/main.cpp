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
#include <backends/imgui_impl_wgpu.h>
#include <backends/imgui_impl_glfw.h>

#include "graphics/skybox_pass.hpp"

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
            transform.rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3{ 1.0f, 0.0f, 0.0f }); //* glm::angleAxis(glm::radians(90.0f), glm::vec3{ 0.0f, 0.0f, 1.0f });
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

        /*for (int i = 0; i < MAX_POINT_LIGHTS; ++i)
        {
            entt::entity lightEntity = g_registry.create();
            Transform& transform = g_registry.emplace<Transform>(lightEntity);
            transform.translation = glm::vec3{ 0.0f, 0.0f, 0.0f };

            auto& light = g_registry.emplace<Renderer::PointLight>(lightEntity);
            light.color = glm::vec4{ 1.0f, 1.0f, 1.0f, 1.0f };
            light.position = transform.translation;
        }

        auto lightView = g_registry.view<Renderer::PointLight>();
        auto transformView = g_registry.view<Transform>();
        (lightView[*(lightView.begin() + 0)]).color = glm::vec4{ 1.0f, 0.0f, 0.0f, 20.0f };
        (lightView[*(lightView.begin() + 1)]).color = glm::vec4{ 0.0f, 1.0f, 0.0f, 20.0f };
        (lightView[*(lightView.begin() + 2)]).color = glm::vec4{ 0.0f, 0.75f, 1.0f, 20.0f };
        (lightView[*(lightView.begin() + 3)]).color = glm::vec4{ 1.0f, 0.0f, 1.0f, 20.0f };

        (transformView[*(transformView.begin() + 0)]).translation = glm::vec3{ 1.5f, 0.0f, 2.0f };
        (transformView[*(transformView.begin() + 1)]).translation = glm::vec3{ -1.5f, 0.0f, 2.0f };
        (transformView[*(transformView.begin() + 2)]).translation = glm::vec3{ 1.0f, 2.0f, 1.0f };
        (transformView[*(transformView.begin() + 3)]).translation = glm::vec3{ -1.0f, 2.0f, 1.0f };*/
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


    {
        auto view = g_registry.view<Mesh, Transform>();
        for (auto&& [entity, mesh, transform] : view.each())
        {
            g_renderer->DrawMesh(mesh, transform);
        }
    } 
    {
        auto view = g_registry.view<Renderer::PointLight, Transform>();
        uint32_t i = 0;
        for (auto&& [entity, light, transform] : view.each()) 
        {
            g_renderer->SetLight(i, light.color, transform.translation);

           ++i;
        } 
    }

    // TODO: Move this to a system.
    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Light");   
    {
        float exposure = g_renderer->GetSkyboxPass().GetExposure();
        if (ImGui::DragFloat("Exposure", &exposure, 0.01f, 0.0f, 10.0f))
        {
            g_renderer->GetSkyboxPass().SetExposure(exposure);
        }

        auto view = g_registry.view<Renderer::PointLight, Transform>();
        uint32_t i = 0;
        for (entt::entity entity : view) 
        {
            auto [light, transform] = view.get<Renderer::PointLight, Transform>(entity);
            std::string name = "Light " + std::to_string(i);
            ImGui::BeginChild(name.c_str(), ImVec2(0, 80), true);
            ImGui::Text("Light %d", entity);
            ImGui::ColorEdit4("Color", &light.color.x);
            ImGui::DragFloat3("Position", &transform.translation.x);   
            ImGui::EndChild();
            
            ++i;
        }
    }
    ImGui::End();

    ImGui::Begin("transforms");
    {
        auto view = g_registry.view<Transform>(); 
        for (entt::entity entity : view)
        {
            auto& transform = view.get<Transform>(entity);
            std::string name = "Transform " + std::to_string(static_cast<uint32_t>(entity));
            ImGui::BeginChild(name.c_str(), ImVec2(0, 80), true);
            ImGui::Text("Transform %d", entity);
            ImGui::DragFloat3("Translation", &transform.translation.x);
            ImGui::DragFloat3("Scale", &transform.scale.x);
            glm::vec3 euler = glm::degrees(glm::eulerAngles(transform.rotation));
            if (ImGui::DragFloat3("Rotation", &euler.x))
            {
                transform.rotation = glm::quat{ glm::radians(euler) };
            }
            ImGui::EndChild();
        }
    }
    ImGui::End();


    ImGui::Render();

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
