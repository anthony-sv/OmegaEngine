export module SandboxApp;

import Engine.Core;
import Engine.Scene;   // Scene::Project

// =================================================================
//
//  SandboxApp -- the generic RUNTIME: a player for any project.
//
// =================================================================
//
// Unlike EditorApp (which renders into an off-screen framebuffer
// and presents it through an ImGui docking UI), SandboxApp is a
// "bare game window": it renders the scene straight to the window's
// default framebuffer, no editor chrome. This exercises the engine
// purely through its PUBLIC API -- the same path a real game built
// on OmegaEngine would take.
//
// It carries no game content. It is constructed FROM a Project (the
// unit of content): the window config comes from project.json, and
// the gameplay layer plays the project's scenes. Point it at a
// different project folder -> a different game, same runtime exe.
//
// =================================================================

export class SandboxApp final : public Engine::Core::Application
{
public:
    explicit SandboxApp(Engine::Scene::Project project);

protected:
    void onInit()     override;
    void onShutdown() override;

private:
    Engine::Scene::Project m_project;   // the content this runtime plays
}; // class SandboxApp