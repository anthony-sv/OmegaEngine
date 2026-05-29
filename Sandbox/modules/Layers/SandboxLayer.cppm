export module SandboxLayer;

import Engine.Core;
import Engine.Renderer;
import Engine.ECS;
import Engine.Scene;
import std;

// =================================================================
//
//  SandboxLayer -- the demo scene.
//
// =================================================================
//
// Shows the engine being driven entirely through its public API:
//
//   1. Renderer2D::init()                            (onAttach)
//   2. register two named scenes with the manager    (onAttach)
//      - each builds its entities + MovementSystem in its onEnter hook
//        and tears them down in its onExit hook (data-driven, no
//        Scene subclassing)
//   3. switchTo("Grid") to pick the initial scene     (onAttach)
//   4. every few seconds, switchTo the other scene    (onUpdate)
//      -> proves DEFERRED switching + enter/exit hooks
//   5. m_sceneManager.onUpdate(dt) applies the pending switch then
//      ticks the active scene's systems               (onUpdate)
//   6. RenderSystem draws the ACTIVE scene's registry  (onRender)
//
// No framebuffer, no ImGui; it renders straight to the window.
//
// =================================================================

export class SandboxLayer final : public Engine::Core::ILayer
{
public:
    SandboxLayer();

    void onAttach()           override;
    void onDetach()           override;
    void onUpdate(float dt)   override;
    void onRender(float alpha) override;

private:
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