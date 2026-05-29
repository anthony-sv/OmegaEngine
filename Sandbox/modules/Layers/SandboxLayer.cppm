export module SandboxLayer;

import Engine.Core;
import Engine.Renderer;
import Engine.ECS;
import std;

// =================================================================
//
//  SandboxLayer -- the demo scene.
//
// =================================================================
//
// Shows the engine being driven entirely through its public API:
//
//   1. Renderer2D::init()                  (onAttach)
//   2. build an ECS scene in a Registry    (onAttach)
//   3. mutate components each frame         (onUpdate)
//   4. RenderSystem draws the scene         (onRender)
//
// No framebuffer, no ImGui -- it renders straight to the window.
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

    // The scene database. Rule of zero -- a plain value member.
    Engine::ECS::Registry m_registry;

    // A handle to one entity we animate every frame, to exercise the
    // live ECS write path (get<Transform>() mutation). Default-null
    // until onAttach creates it.
    Engine::ECS::Entity m_spinner;

    // Accumulated time, used to drive the demo animation.
    float m_elapsed { 0.0f };
}; // class SandboxLayer