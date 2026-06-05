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

    // Build + wire a world for a scene: add the systems and (if the file
    // exists) load its entity data from scenes/<name>.json. Shared by
    // onAttach (existing scenes) and newScene (a fresh, empty one).
    // Idempotent -- it only wires a world the first time it is entered.
    void setupWorld(Engine::Scene::World& world);

    // ── Entity authoring ──────────────────────────────────
    // Create / delete / duplicate entities in the active world. These mutate the live registry
    Engine::ECS::Entity createEntity(std::string name);  // empty: Name + Transform
    Engine::ECS::Entity createSpriteEntity();            // + a white SpriteRenderer
    Engine::ECS::Entity createTilemapEntity();           // + an empty TilemapComponent
    void                duplicateSelected();             // clone all components
    void                deleteSelected();

    // The Tile Palette panel: shows the selected tilemap's tileset (atlas
    // cells or collection tiles) and lets you pick the active brush tile +
    // brush size. The selection feeds the painting step.
    void drawTilePalette();

    // Tile painting in the viewport (when the Paint tool is active and a
    // tilemap is selected): left-drag paints the brush, right-drag erases,
    // Shift = rectangle fill, Alt = eyedropper, with a hovered-cell
    // highlight. The image rect is passed as floats so this interface
    // doesn't depend on ImGui types. Undo/redo restore whole-grid snapshots
    // captured per stroke.
    void paintViewport(float imgMinX, float imgMinY, float imgMaxX, float imgMaxY);
    void undoTilePaint();
    void redoTilePaint();

    // The Tilemap Layers panel: lists the tilemap entities (a "layer" is a
    // tilemap), each with show/hide, select, z-order reorder and opacity.
    void drawLayersPanel();

    // ── Scene authoring ───────────────────────────────────
    // Create / rename / delete project scenes + choose the startup one.
    // Each mutates the in-memory Project AND persists project.json.
    void newScene(std::string name);
    void renameScene(std::string from, std::string to);
    void deleteScene(std::string name);
    void setStartupScene(std::string name);

    // Poll EDGE-triggered keyboard shortcuts (Ctrl+S/O/P/D, Del, 1/2/3).
    // Called once per RENDER frame from onImGuiRender -- NOT from onUpdate,
    // which runs on the fixed-timestep tick and is skipped on frames where
    // no tick elapses, so quick taps were being dropped. (Continuous camera
    // controls stay in onUpdate, where integrating over fixedDt is correct.)
    void handleShortcuts();

    // Append a line to the in-editor Console panel (hotkey feedback etc.).
    void logConsole(std::string message);

    bool m_showViewport  { true };
    bool m_showInspector { true };
    bool m_showHierarchy { true };
    bool m_showConsole   { true };
    bool m_showColliders { true };   // collider wireframe overlay (authoring aid)
    bool m_showGrid      { true };   // tilemap cell-grid overlay (authoring aid)
    bool m_showPalette   { true };   // Tile Palette panel
    bool m_showLayers    { true };   // Tilemap Layers panel

    // Active painting brush (set by the Tile Palette, used when painting):
    // tile id to paint (-1 = erase), and the square brush size in cells.
    int m_brushTile { 0 };
    int m_brushSize { 1 };

    // Viewport tool: tile painting (true) vs the gizmo (false).
    bool m_paintMode { false };

    // Painting-stroke state + per-stroke undo/redo of tile grids.
    bool m_painting    { false };   // mid freehand stroke
    bool m_rectDrag    { false };   // mid Shift rectangle drag
    int  m_rectAnchorX { 0 };
    int  m_rectAnchorY { 0 };
    std::vector<int>    m_strokeBefore;   // grid snapshot taken at stroke start
    Engine::ECS::Entity m_strokeTarget;   // the tilemap being painted

    struct TileEdit
    {
        Engine::ECS::Entity target;   // which tilemap
        std::vector<int>    tiles;    // its grid before this edit
    };
    std::vector<TileEdit> m_undoStack;
    std::vector<TileEdit> m_redoStack;

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

    // ── Scene-management state ────────────────────────────
    // Deleting/renaming the ACTIVE scene can't free its world this frame
    // (we're still rendering it). We switch away, then remove the old
    // world once the deferred switch has applied -- tracked here.
    std::optional<std::string> m_pendingSceneRemoval;

    // The New/Rename Scene modal popups are opened from the menu but must
    // run OUTSIDE it (ImGui rule), so the menu sets a request flag.
    bool        m_openNewScenePopup    { false };
    bool        m_openRenameScenePopup { false };
    char        m_sceneNameBuf[64]     {};       // shared text buffer for both
    std::string m_renameSceneFrom;               // scene being renamed

    // ── Gizmo state (ImGuizmo) ─────────────────────────────
    // The active manipulation mode, kept as a plain int so ImGuizmo's
    // enum stays out of this interface: 0 = translate, 1 = rotate,
    // 2 = scale. Toggled with the 1/2/3 keys.
    int m_gizmoOp { 0 };
}; // class EditorLayer