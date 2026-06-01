export module EditorLayer;

import Engine.Core;
import Engine.Renderer;
import Engine.ECS;
import Engine.Scene;
import Engine.Physics;
import std;

export class EditorLayer final: public Engine::Core::ILayer {
public:
    explicit EditorLayer(Engine::Scene::Project project);

    void onAttach()              override;
    void onDetach()              override;
    void onUpdate(float dt)      override;
    void onRender(float alpha)   override;
    void onImGuiRender()         override;

private:
    // Scene save/load (File menu + Ctrl+S / Ctrl+O). Round-trips the
    // active world to "<world>.json" via the SceneSerializer.
    void saveScene();
    void loadScene();

    // Play-in-editor. Edit mode (m_playing == false) keeps the world
    // STATIC so it can be authored; Play snapshots the scene and ticks
    // the systems (physics/movement/animation); Stop restores the
    // snapshot, discarding whatever the simulation did.
    void togglePlay();

    // Switch the active scene (Scene menu). Deferred via the SceneManager.
    void switchScene(std::string name);

    // Append a line to the in-editor Console panel (hotkey feedback etc.).
    void logConsole(std::string message);

    bool m_showViewport  { true };
    bool m_showInspector { true };
    bool m_showHierarchy { true };
    bool m_showConsole   { true };
    bool m_showColliders { true };   // collider wireframe overlay (authoring aid)

    // Console panel log (hotkey actions, scene switches, ...). Capped.
    std::vector<std::string> m_consoleLog;
    bool                     m_consoleScrollToBottom { false };

    bool m_viewportHovered { false };

    // The project being edited (owned by value, like the runtime layer --
    // independent of the app's copy; the layer outlives the app's derived
    // members at shutdown).
    Engine::Scene::Project m_project;

    // false = Edit mode (world static, authorable); true = Play
    // (systems tick, physics simulates in the viewport).
    bool m_playing { false };

    std::optional<Engine::Renderer::Camera2D>     m_camera;
    std::optional<Engine::Renderer::Framebuffer> m_framebuffer;

    Engine::Scene::SceneManager m_sceneManager;

    // The entity currently selected in the Hierarchy panel; the
    // Inspector edits its components. Default-null; guarded with
    // valid() since a scene rebuild invalidates old handles.
    Engine::ECS::Entity m_selected;
}; // class EditorLayer