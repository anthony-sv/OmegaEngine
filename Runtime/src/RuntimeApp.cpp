module SandboxApp;

import Engine.Core;
import Engine.Scene;
import SandboxLayer;
import std;

// Window config comes from the project's manifest (project.json). The
// base Application is initialised with it BEFORE m_project is moved-in
// (the `project` parameter is still alive at that point).
SandboxApp::SandboxApp(Engine::Scene::Project project)
    : Engine::Core::Application { project.window() }
    , m_project                 { std::move(project) }
{}

void SandboxApp::onInit()
{
    // Input is a Core platform device, driven by the Application loop --
    // nothing to register here. Just query Engine::Core::Input anywhere.

    // One gameplay layer, handed its own copy of the project to play.
    // No ImGui overlay -- this is a bare game window.
    pushLayer<SandboxLayer>(m_project);
    std::println("[Ω::SandboxApp] initialised — Ω ready (project '{}')", m_project.name());
}

void SandboxApp::onShutdown()
{
    std::println("[Ω::SandboxApp] shutting down — Ω offline");
}