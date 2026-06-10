export module Engine.Scripting:ScriptSystem;

import Engine.Core;      // Core::ISystem, Core::EventBus
import Engine.ECS;       // Registry, ScriptComponent
import Engine.Physics;   // PhysicsWorld + collision/trigger events
import :ScriptHost;
import std;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: ScriptSystem
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
    //  ScriptSystem -- ticks every entity's C# script.
    //
    // =================================================================
    //
    // An UPDATE-phase Core::ISystem. onInit boots the host; each onUpdate
    // points the script API at this world, then for every ScriptComponent:
    // lazily instantiates its managed object (createScript) the first time,
    // and calls into C# OnUpdate. `managedDir` is where OmegaEngine.dll +
    // its runtimeconfig.json live (the executable directory).
    //
    // =================================================================

    export class ScriptSystem final : public Core::ISystem
    {
    public:

        // `managedDir` holds the runtime (OmegaEngine.dll + BuildScripts.cs),
        // usually the executable directory. `scriptsDir` is the project's
        // script SOURCE folder (e.g. <project>/scripts): the engine compiles it
        // itself and loads + hot-reloads the result. Empty = no project scripts.
        ScriptSystem(ECS::Registry& registry,
                     std::filesystem::path managedDir,
                     std::filesystem::path scriptsDir = {}
        )
            : m_registry   { registry }
            , m_managedDir { std::move(managedDir) }
            , m_scriptsDir { std::move(scriptsDir) }
        {}

        void onInit() override
        {
            auto& host = ScriptHost::instance();
            host.ensureInitialized(m_managedDir);

            // Build + load the project's scripts ONCE (every world shares the
            // one host, and onInit runs per world).
            static bool booted = false;
            if (m_scriptsDir.empty() || booted)
                return;
            booted = true;

            auto const dir = std::filesystem::absolute(m_scriptsDir);
            auto const dll = dir / "bin" / "managed" / "Game.dll";
            host.configureBuild(dir, m_managedDir / "BuildScripts.cs");

            if (std::filesystem::exists(dll))
            {
                host.loadGame(dll);     // load the last build instantly...
                host.requestBuild();    // ...and refresh it in the background
            }
            else
            {
                host.buildBlocking();   // nothing to load yet -> build first
                host.loadGame(dll);
            }
        }

        // Wire physics into the script API: the body velocity/impulse/force
        // calls steer `world`, and contact/sensor events on `bus` are routed
        // to scripts' OnCollision*/OnTrigger* hooks. Call from the layer after
        // BOTH the ScriptSystem and the PhysicsSystem exist.
        void usePhysics(Physics::PhysicsWorld& world, Core::EventBus& bus)
        {
            m_physics = &world;

            // Only the active world steps physics, so its events are the only
            // ones on the bus -- a single subscription routes to the host's
            // (active-world) instances. Guard so multiple worlds don't each
            // subscribe and fire the callbacks N times.
            static bool subscribed = false;
            if (subscribed)
                return;
            subscribed = true;

            bus.subscribe<Physics::CollisionEnterEvent>([](Physics::CollisionEnterEvent const& e)
            {
                ScriptHost::instance().dispatchPhysicsEvent(
                    static_cast<std::uint32_t>(e.a.id()), static_cast<std::uint32_t>(e.b.id()),
                    PhysicsEventKind::CollisionEnter, e.manifold.normal.x, e.manifold.normal.y);
            });
            bus.subscribe<Physics::CollisionExitEvent>([](Physics::CollisionExitEvent const& e)
            {
                ScriptHost::instance().dispatchPhysicsEvent(
                    static_cast<std::uint32_t>(e.a.id()), static_cast<std::uint32_t>(e.b.id()),
                    PhysicsEventKind::CollisionExit);
            });
            bus.subscribe<Physics::TriggerEnterEvent>([](Physics::TriggerEnterEvent const& e)
            {
                ScriptHost::instance().dispatchPhysicsEvent(
                    static_cast<std::uint32_t>(e.sensor.id()), static_cast<std::uint32_t>(e.other.id()),
                    PhysicsEventKind::TriggerEnter);
            });
            bus.subscribe<Physics::TriggerExitEvent>([](Physics::TriggerExitEvent const& e)
            {
                ScriptHost::instance().dispatchPhysicsEvent(
                    static_cast<std::uint32_t>(e.sensor.id()), static_cast<std::uint32_t>(e.other.id()),
                    PhysicsEventKind::TriggerExit);
            });
        }

        void onUpdate(float dt) override
        {
            auto& host = ScriptHost::instance();
            if (!host.ready())
                return;

            host.bindRegistry(&m_registry);
            host.bindPhysics(m_physics);
            host.setTime(dt);
            host.pollBuild();       // rebuild project scripts when a source changes
            host.beginFrame();      // apply any pending hot reload (main thread)

            // The managed runtime instantiates each script on first sight and
            // keys instances by entity id, so we just tick every one.
            for (auto&& [entity, script] : m_registry.view<ECS::ScriptComponent>().each())
                host.tick(static_cast<std::uint32_t>(entity), script.className, dt);

            // A graph runs as its GENERATED class -- the editor codegens
            // graphs/<name>.ngraph into "Game.<name>" (same build + hot-reload
            // pipeline as hand-written scripts), so the runtime just resolves
            // the name by convention.
            for (auto&& [entity, graph] : m_registry.view<ECS::GraphComponent>().each())
                if (!graph.graphName.empty())
                    host.tick(static_cast<std::uint32_t>(entity), "Game." + graph.graphName, dt);
        }

    private:
        ECS::Registry&         m_registry;
        std::filesystem::path  m_managedDir;
        std::filesystem::path  m_scriptsDir;
        Physics::PhysicsWorld* m_physics { nullptr };

    }; // class ScriptSystem

} // namespace Scripting