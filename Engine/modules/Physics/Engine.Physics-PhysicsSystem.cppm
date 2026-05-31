module;

#include "glm/glm.hpp"

export module Engine.Physics:PhysicsSystem;

import Engine.Core;    // Core::ISystem, Core::EventBus
import Engine.ECS;     // ECS::Registry, ECS::Entity, components
import :PhysicsWorld;
import :Events;
import :Math;
import std;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: PhysicsSystem
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
    //  PhysicsSystem -- the ECS <-> PhysicsWorld bridge.
    //
    // =================================================================
    //
    // An UPDATE-phase Core::ISystem that runs on the FIXED timestep
    // (the World ticks systems from the layer's fixed-update). Each
    // step it:
    //
    //   1. syncToSim     -- create bodies for entities that gained a
    //                       RigidBody2D + collider; destroy bodies whose
    //                       entity/components went away; push kinematic
    //                       velocities.
    //   2. world.step    -- Box2D integrates, solves and resolves
    //                       contacts (one call; the broad/narrow phase
    //                       and solver are all inside).
    //   3. syncFromSim   -- write each body's transform back to the
    //                       entity's Transform (radians -> degrees).
    //   4. dispatchEvents-- translate buffered contacts/sensors into
    //                       Collision*/Trigger* events on the EventBus.
    //
    // AUTHORITY: for Dynamic bodies the simulation owns the Transform
    // (it overwrites it each step). Static bodies seed their body once
    // and never move. Kinematic bodies are driven by Velocity2D.
    //
    // The PhysicsWorld speaks raw uint32 entity ids; this system is the
    // only place that maps them back to ECS::Entity handles.
    //
    // =================================================================

    export class PhysicsSystem final : public Core::ISystem
    {
    public:

        PhysicsSystem(ECS::Registry& registry, Core::EventBus& bus, glm::vec2 gravity = { 0.0f, -9.8f });

        void onUpdate(float dt) override;


        // -- Queries (return ECS::Entity, unlike the raw PhysicsWorld) --

        // Cast a ray from `origin` along `direction` for `maxDistance`
        // world units; returns the closest hit (or { hit=false }).
        [[nodiscard]] RaycastHit raycast(glm::vec2 origin, glm::vec2 direction, float maxDistance) const;

        // Invoke `fn` for every entity whose collider overlaps `region`.
        void overlapAABB(AABB const& region, std::function<void(ECS::Entity)> const& fn) const;


        // Direct access (gravity changes, manual queries, etc.).
        [[nodiscard]] PhysicsWorld&       world()       { return m_world; }
        [[nodiscard]] PhysicsWorld const& world() const { return m_world; }


    private:

        void syncToSim();
        void syncFromSim();
        void dispatchEvents();

        ECS::Registry&  m_registry;
        Core::EventBus& m_bus;
        PhysicsWorld    m_world;

    }; // class PhysicsSystem

} // namespace Physics