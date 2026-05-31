export module Engine.ECS:Registry;

// =================================================================
//
//  Registry + Entity -- the ECS core, backed by EnTT.
//
//  Wraps entt::registry behind a clean engine API (Entity handle +
//  Registry facade). EnTT stays an implementation detail -- consumers
//  of Engine.
//
// =================================================================

import "ThirdParty/EnTT.hpp";
import :CoreComponents;
import std;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: Registry
*   half                    short
*    vec2                  ivec2
*     u32                  i32
*  _________;          ________;
* [RayTracing]       [Rasterizer]
*
════════════════════════════════════════════════════════════════════════════════*/

namespace Engine::ECS
{

    // =================================================================
    //
    //  Entity -- lightweight, non-owning handle (EnTT-backed).
    //
    // =================================================================
    //
    // Wraps an entt::entity ID + a pointer to the entt::registry.
    // Provides the same add/get/has/remove template API as the
    // custom implementation. sizeof(Entity) == 12 bytes.
    //
    // =================================================================

    export class Entity
    {
    public:

        // -- Construction -----------------------------------------------

        Entity() = default;

        Entity(entt::entity id, entt::registry* registry)
            : m_id       { id }
            , m_registry { registry }
        {}


        // -- Component access -------------------------------------------

        // Add a component to this entity. Args forwarded to T's ctor.
        //
        //   entity.add<Transform>({ {0, 0}, 0.0f, {1, 1} });
        //   entity.add<NameComponent>("Player");
        //
        // EnTT's emplace<T> is O(1) amortized.

        template <typename T, typename... Args>
        T& add(Args&&... args)
        {
            return m_registry->emplace<T>(m_id, std::forward<Args>(args)...);
        }

        // Get a mutable reference to an existing component.
        //
        //   auto& tf = entity.get<Transform>();
        //   tf.position.x += 5.0f;
        //
        // Precondition: entity MUST have component T.

        template <typename T>
        [[nodiscard]] T& get()
        {
            return m_registry->get<T>(m_id);
        }

        template <typename T>
        [[nodiscard]] T const& get() const
        {
            return m_registry->get<T>(m_id);
        }

        // Check if this entity has a specific component.
        // EnTT's all_of<T> is O(1) sparse-set lookup.

        template <typename T>
        [[nodiscard]] bool has() const
        {
            return m_registry->all_of<T>(m_id);
        }

        // Remove a component. Precondition: entity MUST have T.

        template <typename T>
        void remove()
        {
            m_registry->remove<T>(m_id);
        }


        // -- Lifetime check ---------------------------------------------

        [[nodiscard]] bool valid() const
        {
            return m_registry && m_registry->valid(m_id);
        }


        // -- Identity ---------------------------------------------------

        [[nodiscard]] entt::entity id() const { return m_id; }


        [[nodiscard]] explicit operator bool() const
        {
            return m_registry != nullptr && m_registry->valid(m_id);
        }

        [[nodiscard]] bool operator==(Entity const&) const = default;


    private:
        entt::entity    m_id       { entt::null };
        entt::registry* m_registry { nullptr };

    }; // class Entity



    // =================================================================
    //
    //  Registry -- EnTT-backed entity/component database.
    //
    // =================================================================
    //
    // Thin wrapper around entt::registry.
    //
    // =================================================================

    export class Registry
    {
    public:

        // -- Entity creation --------------------------------------------

        [[nodiscard]] Entity create()
        {
            return Entity { m_registry.create(), &m_registry };
        }

        [[nodiscard]] Entity create(std::string_view name)
        {
            auto entity = create();
            entity.add<NameComponent>(std::string{ name });
            return entity;
        }

        void destroy(Entity entity)
        {
            m_registry.destroy(entity.id());
        }

        // Rebuild an Entity handle from a raw entity value (the full
        // entt id incl. version, as returned by Entity::id()). Used by
        // external systems -- e.g. physics, which stashes the id in a
        // Box2D body's userData -- to map results back to entities
        // without exposing entt. A stale/recycled id yields an Entity
        // whose valid() is false.
        [[nodiscard]] Entity entityFromId(std::uint32_t raw)
        {
            return Entity { static_cast<entt::entity>(raw), &m_registry };
        }


        // -- Validity ---------------------------------------------------

        [[nodiscard]] bool valid(entt::entity id) const
        {
            return m_registry.valid(id);
        }


        // -- Component access (by raw entt::entity) ---------------------

        template <typename T, typename... Args>
        T& addComponent(entt::entity id, Args&&... args)
        {
            return m_registry.emplace<T>(id, std::forward<Args>(args)...);
        }

        template <typename T>
        [[nodiscard]] T& getComponent(entt::entity id)
        {
            return m_registry.get<T>(id);
        }

        template <typename T>
        [[nodiscard]] T const& getComponent(entt::entity id) const
        {
            return m_registry.get<T>(id);
        }

        template <typename T>
        [[nodiscard]] bool hasComponent(entt::entity id) const
        {
            return m_registry.all_of<T>(id);
        }

        template <typename T>
        void removeComponent(entt::entity id)
        {
            m_registry.remove<T>(id);
        }


        // -- Queries (views) --------------------------------------------
        // Returns an EnTT view for iteration.
        //
        //   auto view = registry.view<Transform, SpriteRenderer>();
        //   for (auto [entity, tf, sr] : view.each()) { ... }

        template <typename... Components>
        [[nodiscard]] auto view()
        {
            return m_registry.view<Components...>();
        }

        template <typename... Components>
        [[nodiscard]] auto view() const
        {
            return m_registry.view<Components...>();
        }

        // Visit EVERY alive entity (regardless of components), as an
        // Entity handle. Used by tools like the editor Hierarchy that
        // need the full entity list, not a component query.
        //
        // Non-template on purpose: taking std::function means the EnTT
        // entity-storage iteration is compiled HERE (inside Engine.ECS,
        // which imports the EnTT header unit) rather than at the call
        // site -- so callers never need EnTT visible.
        void eachEntity(std::function<void(Entity)> const& func)
        {
            // Non-const storage<Type>() returns a reference (the const
            // overload returns a pointer). Iterate it; valid() filters
            // any released slots so only alive entities reach func.
            auto& storage = m_registry.storage<entt::entity>();

            for (auto const e : storage)
                if (m_registry.valid(e))
                    func(Entity { e, &m_registry });
        }


        // -- Statistics -------------------------------------------------

        [[nodiscard]] std::size_t entityCount() const
        {
            // EnTT tracks alive entities via the entity storage's
            // free_list boundary: entities [0..free_list) are alive.
            return static_cast<std::size_t>(m_registry.storage<entt::entity>()->free_list());
        }

        // Destroy ALL entities and their components. The Registry stays
        // usable afterward (you can create fresh entities). Used by
        // Scene::clear() when a scene is torn down on exit.
        void clear()
        {
            m_registry.clear();
        }


        // -- Escape hatch -----------------------------------------------
        // Direct access to entt::registry for advanced features.

        [[nodiscard]] entt::registry&       raw()       { return m_registry; }
        [[nodiscard]] entt::registry const& raw() const { return m_registry; }


    private:
        entt::registry m_registry;

    }; // class Registry

} // namespace ECS