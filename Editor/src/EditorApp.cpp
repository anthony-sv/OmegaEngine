module EditorApp;

import Engine.Core;
import Engine.Scene;
import EditorLayer;
import ImGuiLayer;
import std;

namespace Editor
{

    // The editor window is EDITOR chrome (borderless, dark) -- NOT the
    // project's game-window config. The project name goes in the title.
    EditorApp::EditorApp(Engine::Scene::Project project)
        : Engine::Core::Application {
              Engine::Core::WindowProps {
                  .title     = "ΩmegaEngine Editor — " + project.name(),
                  .width     = 1600,
                  .height    = 900,
                  .vsync     = true,
                  .decorated = false
              }
          }
        , m_project { std::move(project) }
    {}

    void EditorApp::onInit() {
        pushLayer<EditorLayer>(m_project);   // the editor layer gets its own copy
        pushOverlay<ImGuiLayer>();
        std::println("[Ω::EditorApp] initialised — editing project '{}'", m_project.name());
    }

    void EditorApp::onShutdown() {
        std::println("[Ω::EditorApp] shutting down — Ω offline");
    }
} // namespace Editor