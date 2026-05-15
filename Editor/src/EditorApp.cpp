import std;
import Engine.Core;

#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <Windows.h>

class ImGuiLayer final: public Engine::Core::ILayer {
public:
    ImGuiLayer(): ILayer { "Ω::ImGuiLayer" } {}

    void onAttach() override {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        auto& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        io.IniFilename = "omega_editor_layout.ini";

        ImGui::StyleColorsDark();

        if(io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            auto& style = ImGui::GetStyle();
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        auto* nativeWindow = Engine::Core::Application::get().window().nativeHandle();
        ImGui_ImplGlfw_InitForOpenGL(nativeWindow, true);
        ImGui_ImplOpenGL3_Init("#version 460");

        std::println("[Ω::ImGuiLayer] attached — docking + viewports");
    }

    void onDetach() override {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        std::println("[Ω::ImGuiLayer] detached");
    }

    void onRender(float) override {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void onImGuiRender() override {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if(ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            auto* backup = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup);
        }
    }
}; // class ImGuiLayer

class EditorLayer final: public Engine::Core::ILayer {
public:
    EditorLayer(): ILayer { "Ω::EditorLayer" } {}

    void onAttach() override {
        std::println("[Ω::EditorLayer] attached");
    }

    void onImGuiRender() override {
        // Ω::Fullscreen DockSpace host ────────────────────────────────
        auto const* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        constexpr ImGuiWindowFlags hostFlags =
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_MenuBar;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });
        ImGui::Begin("##Ω_DockSpaceHost", nullptr, hostFlags);
        ImGui::PopStyleVar(3);

        ImGui::DockSpace(ImGui::GetID("ΩmegaEngineDockSpace"));

        // Ω::Menu bar ─────────────────────────────────────────────────
        if(ImGui::BeginMenuBar()) {
            if(ImGui::BeginMenu("File")) {
                if(ImGui::MenuItem("Exit", "Alt+F4"))
                    Engine::Core::Application::get().quit();
                ImGui::EndMenu();
            }
            if(ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Viewport", nullptr, &m_showViewport);
                ImGui::MenuItem("Inspector", nullptr, &m_showInspector);
                ImGui::MenuItem("Hierarchy", nullptr, &m_showHierarchy);
                ImGui::MenuItem("Console", nullptr, &m_showConsole);
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        // Ω::Panels ───────────────────────────────────────────────────
        if(m_showViewport) {
            ImGui::Begin("Viewport", &m_showViewport);
            ImGui::Text("Ω Game renders here");
            auto size = ImGui::GetContentRegionAvail();
            ImGui::Text("Available: %.0fx%.0f", size.x, size.y);
            ImGui::End();
        }

        if(m_showInspector) {
            ImGui::Begin("Inspector", &m_showInspector);
            ImGui::TextDisabled("No entity selected");
            ImGui::End();
        }

        if(m_showHierarchy) {
            ImGui::Begin("Scene Hierarchy", &m_showHierarchy);
            ImGui::Text("(empty scene)");
            ImGui::End();
        }

        if(m_showConsole) {
            ImGui::Begin("Console", &m_showConsole);
            ImGui::TextColored({ 0.3f, 0.9f, 0.5f, 1.0f },
                               "[Ω] OmegaEngine started successfully");
            ImGui::TextColored({ 0.5f, 0.5f, 0.7f, 1.0f },
                               "[Ω] Docking + Viewports enabled");
            ImGui::TextColored({ 0.5f, 0.5f, 0.7f, 1.0f },
                               "[Ω] Fixed timestep: 60Hz (%.2fms)", 1000.0f / 60.0f);
            ImGui::End();
        }

        ImGui::End(); // DockSpaceHost
    }

private:
    bool m_showViewport { true };
    bool m_showInspector { true };
    bool m_showHierarchy { true };
    bool m_showConsole { true };
}; // class EditorLayer

class EditorApp final: public Engine::Core::Application {
public:
    EditorApp()
        : Application { Engine::Core::WindowProps{
            .title = "ΩmegaEngine Editor",
            .width = 1600,
            .height = 900,
            .vsync = true,
            .decorated = false
        } } {}

protected:
    void onInit() override {
        pushOverlay<ImGuiLayer>();
        pushLayer<EditorLayer>();
        std::println("[Ω::EditorApp] initialised — Ω ready");
    }

    void onShutdown() override {
        std::println("[Ω::EditorApp] shutting down — Ω offline");
    }
}; // class EditorApp

auto main(int argc, char* argv[]) -> int {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    EditorApp app;

    auto result = app.run();

    if(!result) {
        auto const& err = result.error();
        std::println(std::cerr, "[Ω FATAL] Error {}: {}", static_cast<int>(err.code), err.message);
        return 1;
    }

    return result.value();
}