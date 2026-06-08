export module Engine.Scripting:ScriptHost;

import Engine.ECS;       // ECS::Registry (the script API operates on it)
import Engine.Physics;   // Physics::PhysicsWorld (script-driven body dynamics)
import Engine.Renderer;  // Renderer::Camera2D (script-driven camera)
import Engine.Scene;     // Scene::SceneManager (script-driven scene switching)
import std;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: ScriptHost
*   half                    short
*    vec2                  ivec2
*     u32                  i32
*  _________;          ________;
* [RayTracing]       [Rasterizer]
*
════════════════════════════════════════════════════════════════════════════════*/

namespace Engine::Scripting
{

    // =================================================================
    //
    //  ScriptHost -- hosts CoreCLR (.NET) in-process and bridges to the
    //  managed scripting runtime (OmegaEngine.dll).
    //
    // =================================================================
    //
    // The .NET hosting headers (nethost/hostfxr) appear ONLY in the .cpp
    // behind an opaque Impl (PIMPL) -- the same trick the Box2D wrapper
    // uses -- so nothing that imports this interface drags in the hosting
    // API. There is ONE host per process (CoreCLR is process-global), so
    // it is a singleton.
    //
    // Flow:
    //   ensureInitialized()  -- boot CoreCLR from OmegaEngine's
    //                           runtimeconfig.json, load the assembly,
    //                           resolve the managed entry points, hand C#
    //                           the engine's function table.
    //   bindRegistry()       -- point the script API at the world the
    //                           scripts run on (set each frame before
    //                           updateScript; scripts are synchronous on
    //                           the main thread).
    //   createScript/update/destroy -- per managed instance.
    //
    // =================================================================

    // Which physics event is being delivered to a script. Crosses the managed
    // boundary as the underlying byte, so the type AND values MUST match the
    // managed Bootstrap.PhysicsEventKind enum.
    export enum class PhysicsEventKind : std::uint8_t
    {
        CollisionEnter,
        CollisionExit,
        TriggerEnter,
        TriggerExit,
    };

    export class ScriptHost
    {
    public:

        // The process-wide host.
        [[nodiscard]] static ScriptHost& instance();

        // Boot CoreCLR + load OmegaEngine.dll from `managedDir` (idempotent;
        // does nothing once ready). Returns false if hosting failed.
        bool ensureInitialized(std::filesystem::path const& managedDir);

        [[nodiscard]] bool ready() const;

        // Load the project's game assembly (e.g. Game.dll) into a collectible
        // context and watch it for rebuilds (hot reload). Idempotent.
        void loadGame(std::filesystem::path const& gameAssembly);

        // ── Project-script build orchestration ──
        // The engine compiles the project's C# scripts itself by shelling out
        // to the BuildScripts.cs tool (dotnet run --file), which writes the
        // assembly to a fixed output dir. configureBuild sets the source dir +
        // tool path; buildBlocking builds and WAITS (used when there's no
        // prior build to load); requestBuild builds in the BACKGROUND;
        // pollBuild (call each frame) rebuilds when a .cs source changes -- the
        // dll watch then hot-reloads the result.
        void configureBuild(std::filesystem::path scriptsDir, std::filesystem::path buildTool);
        void buildBlocking();
        void requestBuild();
        void pollBuild();

        // The ECS world the script API reads/writes (set before updates).
        // Switching worlds clears the previous world's script instances.
        void bindRegistry(ECS::Registry* registry);

        // The physics world the script body API steers (velocity / impulse /
        // force). May be null for worlds without physics. Set before updates.
        void bindPhysics(Physics::PhysicsWorld* physics);

        // The view camera scripts can read/move (e.g. a follow camera). May be
        // null. Set before updates.
        void bindCamera(Renderer::Camera2D* camera);

        // The scene manager scripts switch between (Scene.Load). May be null.
        // Set before updates. The switch is applied at the frame boundary.
        void bindSceneManager(Scene::SceneManager* scenes);

        // Publish this frame's dt (Time.Delta) and accumulate elapsed time.
        void setTime(float dt);

        // Apply any pending hot reload. Call once per update batch, on the
        // main thread, before ticking.
        void beginFrame();

        // Drop all live script instances. bindRegistry does this automatically
        // on a world change, but the editor's Play/Stop restores into the SAME
        // world (registry pointer unchanged), so it must clear explicitly.
        void clearInstances();

        // Full names of every concrete Script subclass in the loaded game
        // assembly (for the editor's class picker). Empty if no scripts loaded.
        [[nodiscard]] std::vector<std::string> scriptClasses();

        // A script field exposed to the editor. The value crosses as a STRING;
        // the C# side converts to/from the real field type by reflection.
        enum class ScriptFieldType : std::uint8_t { Float, Int, Bool, String, Vec2 };
        struct ScriptFieldDesc
        {
            std::string     name;
            ScriptFieldType type;
            std::string     value;   // default value (from a fresh instance)
        };

        // The editable (public / [SerializeField]) fields of `className`, each
        // with its type and default value, for the inspector to render.
        [[nodiscard]] std::vector<ScriptFieldDesc> describeFields(std::string const& className);

        // Route a physics contact/sensor event to the involved scripts'
        // OnCollision*/OnTrigger* hooks. `normal` is the contact normal (points
        // a -> b; zero for exits/triggers); the managed side re-orients it per
        // side so it points away from the other entity.
        void dispatchPhysicsEvent(std::uint32_t a, std::uint32_t b, PhysicsEventKind kind,
                                  float normalX = 0.0f, float normalY = 0.0f);

        // Tick one entity's script: the managed runtime instantiates it on
        // first sight (keyed by entity id) and drives OnUpdate.
        void tick(std::uint32_t entity, std::string const& className, float dt);

        ~ScriptHost();
        ScriptHost(ScriptHost const&)            = delete;
        ScriptHost& operator=(ScriptHost const&) = delete;

    private:
        ScriptHost();

        struct Impl;
        std::unique_ptr<Impl> m_impl;

    }; // class ScriptHost

} // namespace Scripting