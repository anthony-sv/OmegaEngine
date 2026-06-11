export module SandboxLayer;

import Engine.Core;
import Engine.Renderer;
import Engine.ECS;
import Engine.Scene;
import std;

// =================================================================
//
//  SandboxLayer -- the GENERIC runtime layer.
//
// =================================================================
//
// This layer no longer hardcodes any game content. It is handed a
// Project (the unit of content) and plays it:
//
//   1. Renderer2D::init()                              (onAttach)
//   2. register ONE world per scene in project.scenes(), each loading
//      its entities from the project's scenes/<name>.json on enter
//   3. switchTo(project.startupScene())                (onAttach)
//   4. SPACE cycles to the next scene in the project's list -- the
//      switch is data-driven (the scene list), not hardcoded
//   5. m_sceneManager.onUpdate(dt) applies the pending switch then
//      ticks the active scene's systems                (onUpdate)
//   6. RenderSystem draws the ACTIVE scene's registry  (onRender)
//
// Systems + input bindings are engine CODE; entities are pure DATA
// from the project. Swap the project folder -> a different game, same
// runtime. No framebuffer, no ImGui; it renders straight to the window.
//
// =================================================================

export class SandboxLayer final : public Engine::Core::ILayer
{
public:
    explicit SandboxLayer(Engine::Scene::Project project);

    void onAttach()           override;
    void onDetach()           override;
    void onUpdate(float dt)   override;
    void onRender(float alpha) override;

private:
    // The content this runtime plays. Owned by VALUE (a copy) so its
    // lifetime is tied to the layer, independent of the app's own copy
    // -- the layer outlives the app's derived members at shutdown.
    Engine::Scene::Project m_project;

    // The camera we view the scene through. optional<> because it's
    // constructed in onAttach (once the window/GL context exists),
    // not at layer construction time.
    std::optional<Engine::Renderer::Camera2D> m_camera;

    // Owns every world and tracks the active one. The layer holds the
    // MANAGER, not a World -- so multiple scenes (and switching between
    // them) are first-class. Scene switching is driven by input actions
    // (Space -> "NextScene"), bound per scene in onAttach.
    Engine::Scene::SceneManager m_sceneManager;
}; // class SandboxLayer