export module EditorLayer;

import Engine.Core;

export class EditorLayer final: public Engine::Core::ILayer {
public:
    EditorLayer();

    void onAttach() override;

    void onImGuiRender() override;

private:
    bool m_showViewport { true };
    bool m_showInspector { true };
    bool m_showHierarchy { true };
    bool m_showConsole { true };
}; // class EditorLayer