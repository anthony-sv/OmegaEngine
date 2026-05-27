export module EditorLayer;

import Engine.Core;
import Engine.Renderer;
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

    // Ω::Renderer — test quad
    std::optional<Engine::Renderer::Shader>       m_testShader;
    std::optional<Engine::Renderer::VertexBuffer> m_vertexBuffer;
    std::optional<Engine::Renderer::IndexBuffer>  m_indexBuffer;
    std::optional<Engine::Renderer::VertexArray>  m_vertexArray;
    std::optional<Engine::Renderer::Texture2D>    m_texture;
    std::optional<Engine::Renderer::Camera2D>     m_camera;

    // Ω::Renderer Phase 2 — off-screen render target for the Viewport panel
    std::optional<Engine::Renderer::Framebuffer> m_framebuffer;
}; // class EditorLayer