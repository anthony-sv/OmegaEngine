module;

#include "glm/glm.hpp"

export module Engine.Physics:PhysicsWorld;

import :Math;
import std;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: PhysicsWorld
*   half                    short
*    vec2                  ivec2
*     u32                  i32
*  _________;          ________;
* [RayTracing]       [Rasterizer]
*
════════════════════════════════════════════════════════════════════════════════*/

namespace Engine::Physics
{

    // =================================================================
    //
    //  PhysicsWorld -- the Box2D wrapper (PIMPL).
    //
    // =================================================================
    //
    // Owns one b2World and the entity <-> body mapping. Box2D is an
    // IMPLEMENTATION DETAIL: it appears only in the .cpp (behind the
    // opaque Impl), never here -- so nothing that includes this header
    // pulls in <box2d>, and the backend could be swapped without
    // touching callers.
    //
    // ENGINE-AGNOSTIC: this class knows nothing about the ECS. Entities
    // are passed as plain 32-bit ids (EntityId). The PhysicsSystem is
    // the bridge that maps those ids to ECS::Entity handles. This keeps
    // the physics core reusable and the dependency graph acyclic.
    //
    // LIFECYCLE: move-only (it owns a heap Impl). Created with a gravity
    // vector; bodies are created/destroyed on demand; step() advances
    // the simulation and buffers contact/sensor events for the frame.
    //
    // =================================================================

    export class PhysicsWorld
    {
    public:

        // A stable per-entity id (the ECS raw entity value). 0xFFFFFFFF
        // is reserved as "none".
        using EntityId = std::uint32_t;

        // Body simulation type. Order matches Box2D's b2BodyType so the
        // translation is a direct cast.
        enum class BodyType : std::uint8_t { Static, Kinematic, Dynamic };

        // Body construction parameters (engine-side mirror of b2BodyDef).
        struct BodyDef
        {
            BodyType  type           { BodyType::Dynamic };
            glm::vec2 position        { 0.0f, 0.0f };
            float     angle           { 0.0f };          // RADIANS
            glm::vec2 linearVelocity  { 0.0f, 0.0f };
            float     gravityScale    { 1.0f };
            bool      fixedRotation   { false };
		}; // struct BodyDef

        // Shape material + role (engine-side mirror of b2ShapeDef).
        struct ShapeDef
        {
            float density     { 1.0f };
            float friction    { 0.3f };
            float restitution { 0.0f };
            bool  isSensor    { false };
		}; // struct ShapeDef

        // A buffered contact between two solid shapes (begin/end). For
        // end events the manifold is empty (the shapes are apart).
        struct ContactPair
        {
            EntityId        a { };
            EntityId        b { };
            ContactManifold manifold;
		}; // struct ContactPair

        // A buffered sensor overlap (begin/end).
        struct SensorPair
        {
            EntityId sensor  { };
            EntityId visitor { };
		}; // struct SensorPair

        // Low-level ray result (uint32 entity; the system wraps it into
        // the public RaycastHit with an ECS::Entity).
        struct RayHit
        {
            bool      hit      { false };
            EntityId  entity   { };
            glm::vec2 point    { 0.0f, 0.0f };
            glm::vec2 normal   { 0.0f, 0.0f };
            float     fraction { 0.0f };
		}; // struct RayHit


        explicit PhysicsWorld(glm::vec2 gravity = { 0.0f, -9.8f });
        ~PhysicsWorld();

        PhysicsWorld(PhysicsWorld&&) noexcept;
        PhysicsWorld& operator=(PhysicsWorld&&) noexcept;
        PhysicsWorld(PhysicsWorld const&)            = delete;
        PhysicsWorld& operator=(PhysicsWorld const&) = delete;


        // -- World config ----------------------------------------------

        void setGravity(glm::vec2 gravity);


        // -- Body lifecycle --------------------------------------------
        // Each create*Body attaches ONE shape to a new body and records
        // the entity id (in body userData + an internal map). `halfSize`
        // is half-extents (Box2D convention); `offset` shifts the shape
        // from the body origin.

        void createBoxBody    (EntityId id, BodyDef const&, ShapeDef const&, glm::vec2 halfSize, glm::vec2 offset);
        void createCircleBody (EntityId id, BodyDef const&, ShapeDef const&, float radius,       glm::vec2 offset);
        void createPolygonBody(EntityId id, BodyDef const&, ShapeDef const&, std::span<glm::vec2 const> points);

        // One box of a multi-fixture body: half-extents + local centre.
        struct BoxShape
        {
            glm::vec2 halfSize { 0.5f, 0.5f };
            glm::vec2 center   { 0.0f, 0.0f };
		}; // struct BoxShape

        // A single STATIC body carrying MANY box fixtures, used for tilemap
        // collision: one body for the whole grid, one box per solid cell.
        // Far cheaper than a body per cell. `position` is the body origin
        // (the tilemap's grid origin); each box centre is relative to it.
        void createStaticBoxesBody(EntityId id, glm::vec2 position, ShapeDef const&, std::span<BoxShape const> boxes);

        void destroyBody(EntityId id);
        [[nodiscard]] bool hasBody(EntityId id) const;

        // Kinematic control / teleport.
        void setLinearVelocity(EntityId id, glm::vec2 velocity);
        void setTransform     (EntityId id, glm::vec2 position, float angle);


        // -- Simulation ------------------------------------------------
        // Advance by a FIXED timestep, then buffer this step's contact
        // and sensor events (retrievable via the *Events() spans below).

        void step(float dt, int subStepCount = 4);

        // Visit every live body: fn(entityId, position, angle[radians]).
        void eachBody(std::function<void(EntityId, glm::vec2, float)> const& fn) const;


        // -- Buffered events (valid until the next step) ---------------

        [[nodiscard]] std::span<ContactPair const> contactBeginEvents() const;
        [[nodiscard]] std::span<ContactPair const> contactEndEvents()   const;
        [[nodiscard]] std::span<SensorPair  const> sensorBeginEvents()  const;
        [[nodiscard]] std::span<SensorPair  const> sensorEndEvents()    const;


        // -- Queries ---------------------------------------------------

        [[nodiscard]] RayHit raycast(glm::vec2 origin, glm::vec2 direction, float maxDistance) const;
        void overlapAABB(AABB const& region, std::function<void(EntityId)> const& fn) const;


    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;

    }; // class PhysicsWorld

} // namespace Physics