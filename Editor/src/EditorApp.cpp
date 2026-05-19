module EditorApp;

import Engine.Core;
import EditorLayer;
import ImGuiLayer;
import std;

EditorApp::EditorApp()
        : Engine::Core::Application {
            Engine::Core::WindowProps {
                .title = "ΩmegaEngine Editor",
                .width = 1600,
                .height = 900,
                .vsync = true,
                .decorated = false
            } 
        } {}

void EditorApp::onInit() {
    pushLayer<EditorLayer>();
    pushOverlay<ImGuiLayer>();
    std::println("[Ω::EditorApp] initialised — Ω ready");
}

void EditorApp::onShutdown() {
    std::println("[Ω::EditorApp] shutting down — Ω offline");
}