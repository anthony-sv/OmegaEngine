export module EditorLayer;

import Engine.Core;
import Engine.Renderer;
import Engine.ECS;
import Engine.Scene;
import std;

export class EditorLayer final: public Engine::Core::ILayer {
public:
    EditorLayer(); 

    void onAttach()              override;
    void onDetach()              override;
    void onUpdate(float dt)      override;
    void onRender(float alpha)   override;
    void onImGuiRender()         override;

private:
    bool m_showViewport  { true };
    bool m_showInspector { true };
    bool m_showHierarchy { true };
    bool m_showConsole   { true };

    bool m_viewportHovered { false };

    // Ω::Renderer — resources that EditorLayer owns
    std::optional<Engine::Renderer::Texture2D>    m_texture;
    std::optional<Engine::Renderer::Camera2D>     m_camera;

    // Ω::Renderer Phase 2 — off-screen render target for the Viewport panel
    std::optional<Engine::Renderer::Framebuffer> m_framebuffer;

    // Ω::Renderer Phase 8 — sprite sheet atlas and sub-textures
    // The atlas (m_spriteSheet) must outlive every SubTexture2D that
    // references it, because SubTexture2D holds a non-owning pointer.
    std::optional<Engine::Renderer::Texture2D>    m_spriteSheet;
    std::optional<Engine::Renderer::SubTexture2D> m_spriteA;
    std::optional<Engine::Renderer::SubTexture2D> m_spriteB;
    std::optional<Engine::Renderer::SubTexture2D> m_spriteC;

    Engine::Scene::SceneManager m_sceneManager;

    // The entity currently selected in the Hierarchy panel; the
    // Inspector edits its components. Default-null; guarded with
    // valid() since a scene rebuild invalidates old handles.
    Engine::ECS::Entity m_selected;
}; // class EditorLayer