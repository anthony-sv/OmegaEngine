export module EditorLayer;

import Engine.Core;
import Engine.Renderer;
import std;

export class EditorLayer final: public Engine::Core::ILayer {
public:
    EditorLayer(); 

    void onAttach()              override;
    void onDetach()              override;
    void onRender(float alpha)   override;
    void onImGuiRender()         override;

private:
    bool m_showViewport  { true };
    bool m_showInspector { true };
    bool m_showHierarchy { true };
    bool m_showConsole   { true };

    // Ω::Renderer Phase 1 test — raw triangle
    std::optional<Engine::Renderer::Shader> m_testShader;
    std::uint32_t m_testVAO { 0 };
    std::uint32_t m_testVBO { 0 };

    // Ω::Renderer Phase 2 — off-screen render target for the Viewport panel
    std::optional<Engine::Renderer::Framebuffer> m_framebuffer;
}; // class EditorLayer