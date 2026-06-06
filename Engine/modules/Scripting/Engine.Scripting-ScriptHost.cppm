export module Engine.Scripting:ScriptHost;

import Engine.ECS;       // ECS::Registry (the script API operates on it)
import Engine.Physics;   // Physics::PhysicsWorld (script-driven body dynamics)
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

        // The ECS world the script API reads/writes (set before updates).
        // Switching worlds clears the previous world's script instances.
        void bindRegistry(ECS::Registry* registry);

        // The physics world the script body API steers (velocity / impulse /
        // force). May be null for worlds without physics. Set before updates.
        void bindPhysics(Physics::PhysicsWorld* physics);

        // Publish this frame's dt (Time.Delta) and accumulate elapsed time.
        void setTime(float dt);

        // Apply any pending hot reload. Call once per update batch, on the
        // main thread, before ticking.
        void beginFrame();

        // Route a physics contact/sensor event to the involved scripts'
        // OnCollision*/OnTrigger* hooks.
        void dispatchPhysicsEvent(std::uint32_t a, std::uint32_t b, PhysicsEventKind kind);

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