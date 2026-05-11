module;

#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

module Engine.Core:Application;

import :Application;

namespace Engine::Core {

    void Application::WindowDeleter::operator()(GLFWwindow* ptr) const {
        if(ptr) glfwDestroyWindow(ptr);
    }

    bool Application::initGraphics() {
        if(!glfwInit()) return false;

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        auto* raw_window = glfwCreateWindow(1280, 720, "C++23 ImGui Docking", nullptr, nullptr);
        if(!raw_window) return false;

        m_window.reset(raw_window); // Transfers ownership to our unique_ptr
        glfwMakeContextCurrent(m_window.get());
        glfwSwapInterval(1);

        if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return false;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable;
        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForOpenGL(m_window.get(), true);
        ImGui_ImplOpenGL3_Init("#version 460");

        return true;
    }

    void Application::renderLoop() {
        auto& io = ImGui::GetIO();
        while(!glfwWindowShouldClose(m_window.get())) {
            glfwPollEvents();

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
            ImGui::Begin("Hello, Docking!");
            ImGui::Text("Modular RAII implementation complete.");
            ImGui::End();

            ImGui::Render();
            int w, h;
            glfwGetFramebufferSize(m_window.get(), &w, &h);
            glViewport(0, 0, w, h);
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            if(io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
                auto* backup_context = glfwGetCurrentContext();
                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();
                glfwMakeContextCurrent(backup_context);
            }
            glfwSwapBuffers(m_window.get());
        }
    }

    void Application::cleanup() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwTerminate();
    }

    std::expected<int, int> Application::run() {
        if(!initGraphics()) return std::unexpected(1);

        renderLoop();
        cleanup();

        return 0;
    }
}