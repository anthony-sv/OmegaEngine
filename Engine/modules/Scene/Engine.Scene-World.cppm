export module Engine.Scene:World;

import Engine.ECS;    // ECS::Registry, ECS::Entity
import Engine.Core;   // Core::ISystem
import std;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: World
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
    //  World -- a Registry plus the systems that run on it.
    //
    // =================================================================
    //
    // WHAT IS A WORLD?
    //
    //   A World is the container for one playable "scene": all its
    //   entities and components (the Registry), plus the update-phase
    //   systems that operate on them every frame. A game has several
    //   worlds (main menu, level 1, level 2); the SceneManager owns
    //   them by name and switches the active one. ("World" is the ECS
    //   term for this container -- cf. Bevy's World, flecs' world. A
    //   *scene* is a named world the manager tracks.)
    //
    // WHAT IT SOLVES:
    //
    //   Core's ISystem only has onUpdate(dt) -- no way to reach the
    //   Registry. The World is the wiring: it OWNS the Registry and
    //   hands a reference to each system it runs. That's why every
    //   system added here receives the World's Registry& as the first
    //   constructor argument (see addSystem below).
    //
    // PHASES:
    //
    //   The World drives the UPDATE phase only -- onUpdate(dt) ticks
    //   every system in insertion order. Rendering is a separate concern
    //   owned by the layer (it picks the camera and target, then calls
    //   RenderSystem::render(world.registry(), camera)). Update-phase
    //   systems (movement, animation, physics) live here; the render
    //   system stays phase-separate by design.
    //
    // WHY NON-COPYABLE / NON-MOVABLE:
    //
    //   Each system stores a Registry& that points INTO this World's
    //   m_registry member. If the World were moved, that member would
    //   change address and every system's reference would dangle. So a
    //   World is pinned: construct it in place (behind unique_ptr in the
    //   SceneManager) and never move it. Same rationale as Application.
    //
    // =================================================================

    export class World
    {
    public:

        // A world's lifecycle hook. Called with the World itself so the
        // hook can populate it (create entities, add systems) on enter,
        // or tidy up on exit -- without subclassing World.
        using Hook = std::function<void(World&)>;

        // An action hook: invoked when a bound input action fires while
        // this world is active (the "doAction" equivalent, data-driven --
        // no World subclass). Actions also flow as Core::ActionEvent on
        // the EventBus for systems that prefer to subscribe directly.
        using ActionHook = std::function<void(World&, Core::ActionEvent const&)>;

        World() = default;
        explicit World(std::string name) : m_name { std::move(name) } {}

        ~World()
        {
            // Shut systems down in reverse order of registration, while
            // the Registry is still alive (it outlives m_systems: see
            // member declaration order below).
            shutdownSystems();
        }

        // Pinned in memory -- systems hold references into us.
        World(World const&)            = delete;
        World& operator=(World const&) = delete;
        World(World&&)                 = delete;
        World& operator=(World&&)      = delete;


        // -- Identity ---------------------------------------------------

        [[nodiscard]] std::string const& name() const { return m_name; }
        void setName(std::string name) { m_name = std::move(name); }


        // -- Lifecycle hooks --------------------------------------------
        // Set per-world setup/teardown logic without subclassing. The
        // SceneManager calls enter() when this world becomes active and
        // exit() when it is replaced. Typical use: build entities +
        // systems in onEnter, clear() them in onExit.

        void setOnEnter(Hook hook) { m_onEnter = std::move(hook); }
        void setOnExit (Hook hook) { m_onExit  = std::move(hook); }

        // Invoked by the SceneManager (you normally don't call these).
        void enter() { if (m_onEnter) m_onEnter(*this); }
        void exit()  { if (m_onExit)  m_onExit(*this);  }


        // -- Input actions ----------------------------------------------
        // Each world owns its OWN bindings (key -> action name), set in
        // its onEnter hook and changeable at runtime. The SceneManager
        // points Core::Input at the ACTIVE world's map, so only this
        // scene's bindings are live. setOnAction registers the handler;
        // dispatchAction is called by the SceneManager when an ActionEvent
        // arrives for the active world.

        [[nodiscard]] Core::ActionMap&       actions()       { return m_actions; }
        [[nodiscard]] Core::ActionMap const& actions() const { return m_actions; }

        void setOnAction(ActionHook hook) { m_onAction = std::move(hook); }

        void dispatchAction(Core::ActionEvent const& event)
        {
            if (m_onAction) m_onAction(*this, event);
        }


        // -- Registry access --------------------------------------------

        [[nodiscard]] ECS::Registry&       registry()       { return m_registry; }
        [[nodiscard]] ECS::Registry const& registry() const { return m_registry; }

        // Entity-creation convenience (forwards to the Registry) so
        // callers can write world.createEntity("Player") instead of
        // world.registry().create("Player").
        [[nodiscard]] ECS::Entity createEntity()                       { return m_registry.create(); }
        [[nodiscard]] ECS::Entity createEntity(std::string_view name)  { return m_registry.create(name); }


        // -- Systems ----------------------------------------------------

        // Add an update-phase system. The system type must derive from
        // Core::ISystem and take this World's Registry& as its FIRST
        // constructor parameter; any extra Args are forwarded after it:
        //
        //   world.addSystem<MovementSystem>();
        //   world.addSystem<GravitySystem>(9.81f);   // -> GravitySystem(registry, 9.81f)
        //
        // The registry is injected automatically -- callers never pass
        // it. onInit() is called immediately; onShutdown() runs when the
        // World is destroyed. Returns a reference to the created system
        // (e.g. to toggle its `enabled` flag).
        template <typename T, typename... Args>
            requires std::derived_from<T, Core::ISystem>
        T& addSystem(Args&&... args)
        {
            auto  system = std::make_unique<T>(m_registry, std::forward<Args>(args)...);
            T&    ref    = *system;

            system->onInit();
            m_systems.push_back(std::move(system));
            return ref;
        }

        // Tick every enabled system once. Call from the layer's
        // onUpdate(dt) (the fixed-timestep update phase).
        void onUpdate(float dt)
        {
            for (auto& system : m_systems)
                if (system->enabled)
                    system->onUpdate(dt);
        }

        [[nodiscard]] std::size_t systemCount() const { return m_systems.size(); }


        // -- Teardown ---------------------------------------------------

        // Return the World to an empty state: shut down + drop all
        // systems, then destroy all entities. The World stays reusable
        // (you can rebuild it). Typically called from an onExit hook.
        void clear()
        {
            shutdownSystems();
            m_systems.clear();
            m_registry.clear();
        }


    private:

        void shutdownSystems()
        {
            for (auto it = m_systems.rbegin(); it != m_systems.rend(); ++it)
                (*it)->onShutdown();
        }

        // Declaration order matters: m_registry is declared before
        // m_systems so it is destroyed LATER -- after m_systems, whose
        // elements hold a Registry& into it.
        std::string                                       m_name;
        ECS::Registry                                     m_registry;
        std::vector<std::unique_ptr<Core::ISystem>>       m_systems;
        Core::ActionMap                                   m_actions;
        Hook                                              m_onEnter;
        Hook                                              m_onExit;
        ActionHook                                        m_onAction;

    }; // class World

} // namespace Scene