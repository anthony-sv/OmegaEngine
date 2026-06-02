export module Engine.Scene:Project;

import Engine.Core;   // Core::Result, Core::WindowProps
import std;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: Project
*   half                    short
*    vec2                  ivec2
*     u32                  i32
*  _________;          ________;
* [RayTracing]       [Rasterizer]
*
════════════════════════════════════════════════════════════════════════════════*/

namespace Engine::Scene
{

    // =================================================================
    //
    //  Project -- the unit of CONTENT
    //
    // =================================================================
    //
    // A project is a self-contained FOLDER on disk:
    //
    //   MyGame/
    //     project.json     <- this manifest (name, startup scene, scene
    //                         list, window config)
    //     assets/          <- textures, shaders, ...
    //     scenes/          <- PURE entity data (authored in the editor)
    //     saves/           <- runtime save-games (not authored scenes)
    //
    // It is NOT compiled into any app. Both the Editor (a generic tool)
    // and the Runtime (a generic player) OPEN a project and operate on
    // its data -- one project, two tools. That is the unification.
    //
    // CONTENT ROOT: paths inside scenes ("assets/textures/Sheet.png")
    // are relative to the project root. The app sets the working
    // directory to root() at startup, so every relative path resolves
    // INTO the project -- no per-call path rewriting needed.
    //
    // =================================================================

    export class Project
    {
    public:

        // Load a project from its folder (reads <projectDir>/project.json).
        // Returns a fully-populated Project, or an error (file missing /
        // malformed JSON) -- never a half-built one.
        [[nodiscard]] static Core::Result<Project> load(std::filesystem::path const& projectDir);

        [[nodiscard]] std::filesystem::path const&    root()         const { return m_root; }
        [[nodiscard]] std::string const&              name()         const { return m_name; }
        [[nodiscard]] std::string const&              startupScene() const { return m_startupScene; }
        [[nodiscard]] std::vector<std::string> const& scenes()       const { return m_scenes; }
        [[nodiscard]] Core::WindowProps const&        window()       const { return m_window; }

        // Resolve a scene NAME to its absolute file path in the project.
        //   scenePath("level1") -> <root>/scenes/level1.json
        [[nodiscard]] std::filesystem::path scenePath(std::string const& sceneName) const
        {
            return m_root / "scenes" / (sceneName + ".json");
        }


        // -- Authoring (editor) -----------------------------------------
        //
        // In-memory mutation of the manifest. These DON'T touch disk --
        // call save() to persist the changes back to project.json. (The
        // editor pairs each mutation with a save so the on-disk manifest
        // stays in step.)

        // Append a scene if it isn't already listed. If this is the first
        // scene, it also becomes the startup scene.
        void addScene(std::string name)
        {
            if (std::ranges::find(m_scenes, name) != m_scenes.end())
                return;
            if (m_scenes.empty())
                m_startupScene = name;
            m_scenes.push_back(std::move(name));
        }

        // Drop a scene from the list. If it was the startup scene, the
        // startup falls back to the first remaining scene (or empty).
        void removeScene(std::string const& name)
        {
            std::erase(m_scenes, name);
            if (m_startupScene == name)
                m_startupScene = m_scenes.empty() ? std::string {} : m_scenes.front();
        }

        // Rename a scene in place (keeps its list position). Updates the
        // startup pointer if it referenced the old name. (The scene FILE
        // on disk is moved by the caller -- this only touches the manifest.)
        void renameScene(std::string const& from, std::string const& to)
        {
            if (auto it = std::ranges::find(m_scenes, from); it != m_scenes.end())
                *it = to;
            if (m_startupScene == from)
                m_startupScene = to;
        }

        void setStartupScene(std::string name) { m_startupScene = std::move(name); }

        // Persist the current manifest back to <root>/project.json.
        // (nlohmann is confined to the .cpp, so this is declared here and
        // defined there -- consumers never see the JSON library.)
        [[nodiscard]] Core::VoidResult save() const;

    private:
        std::filesystem::path    m_root;          // the project folder
        std::string              m_name;
        std::string              m_startupScene;  // which scene boots first
        std::vector<std::string> m_scenes;        // all scenes in the project
        Core::WindowProps        m_window;        // title / size / vsync / ...

    }; // class Project

} // namespace Scene