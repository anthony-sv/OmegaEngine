export module Engine.Scene:SceneManager;

import :World;
import std;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: SceneManager
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
    //  SceneManager -- owns named worlds and the active one.
    //
    // =================================================================
    //
    // WHY THIS EXISTS:
    //
    //   A game has many scenes (menu, level 1, level 2). A layer should
    //   NOT own a single World member -- it owns a SceneManager, which
    //   owns every world and tracks which one is active. A "scene" is a
    //   World identified by NAME, not by a C++ type: there is no
    //   MenuScene subclass, just a World named "Menu" whose contents
    //   were built by a builder/onEnter hook. (Data-driven -- the same
    //   path the future serializer/Editor will use.)
    //
    // OWNERSHIP & STABILITY:
    //
    //   Worlds are stored as unique_ptr in a map. unique_ptr gives each
    //   World a STABLE heap address, which is required: a World is
    //   non-movable because its systems hold a Registry& into it. The
    //   active world is just a raw pointer into that map (non-owning).
    //
    // DEFERRED SWITCHING:
    //
    //   switchTo(name) does NOT change the active world immediately --
    //   it records a pending request. The switch is applied at the start
    //   of the next onUpdate (a frame boundary), so you can safely call
    //   switchTo() from inside a system or an event handler without
    //   destroying/replacing the world that is currently being iterated.
    //
    //   When the switch applies: the old world's exit() hook runs, the
    //   active pointer changes, then the new world's enter() hook runs.
    //
    // =================================================================

    export class SceneManager
    {
    public:

        // Create a new, empty world with the given name and return a
        // reference so the caller can configure it (set onEnter/onExit
        // hooks, etc.). Does NOT make it active -- call switchTo().
        // If a world with this name already exists, the existing one is
        // returned unchanged.
        World& create(std::string name)
        {
            if (auto it = m_worlds.find(name); it != m_worlds.end())
                return *it->second;

            auto  world = std::make_unique<World>(name);
            World& ref  = *world;
            m_worlds.emplace(std::move(name), std::move(world));
            return ref;
        }

        // Request that `name` become the active world. Applied at the
        // start of the next onUpdate (deferred). Safe to call mid-frame.
        void switchTo(std::string name)
        {
            m_pending = std::move(name);
        }

        // The currently active world, or nullptr if none is active yet
        // (e.g. before the first onUpdate applies the initial switchTo).
        [[nodiscard]] World*       active()       { return m_active; }
        [[nodiscard]] World const* active() const { return m_active; }

        [[nodiscard]] bool         has(std::string const& name)  const { return m_worlds.contains(name); }
        [[nodiscard]] std::size_t  sceneCount()                  const { return m_worlds.size(); }

        // Apply any pending switch (frame boundary), then tick the
        // active world's systems. Call once from the layer's onUpdate.
        void onUpdate(float dt)
        {
            applyPendingSwitch();

            if (m_active)
                m_active->onUpdate(dt);
        }


    private:

        void applyPendingSwitch()
        {
            if (!m_pending)
                return;

            auto it = m_worlds.find(*m_pending);
            if (it == m_worlds.end())
            {
                std::println(std::cerr,
                             "[Ω::SceneManager] switchTo: no scene named '{}'",
                             *m_pending
                );
                m_pending.reset();
                return;
            }

            World* next = it->second.get();

            if (next != m_active)
            {
                if (m_active)
                    m_active->exit();      // tear down the outgoing world

                m_active = next;
                m_active->enter();         // build/activate the incoming world

                std::println("[Ω::SceneManager] active scene -> '{}'", m_active->name());
            }

            m_pending.reset();
        }

        // name -> world. unique_ptr keeps each World at a stable address.
        std::unordered_map<std::string, std::unique_ptr<World>> m_worlds;

        World*                     m_active  { nullptr };   // non-owning
        std::optional<std::string> m_pending {};            // requested switch

    }; // class SceneManager

} // namespace Scene