module;

#include "glm/glm.hpp"

export module Engine.Physics:Events;

import Engine.ECS;     // ECS::Entity
import :Math;          // ContactManifold
import std;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: Physics::Events
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
    //  Physics outputs that reference entities.
    //
    // =================================================================
    //
    // The four collision/trigger events are published on the
    // Core::EventBus by the PhysicsSystem after each fixed step --
    // gameplay subscribes the same way it does for input ActionEvents.
    //
    // COLLISION vs TRIGGER:
    //   - Collision events come from SOLID shapes that physically
    //     respond (the solver pushes them apart). They carry the
    //     contact manifold (where + how hard they hit).
    //   - Trigger events come from SENSOR shapes (isSensor = true):
    //     overlap is detected and reported but NO collision response
    //     happens. Used for pickups, checkpoints, damage volumes.
    //
    // Entities are reported as ECS::Entity handles -- the PhysicsSystem
    // resolves Box2D's internal ids back to entities before publishing.
    //
    // =================================================================

    // Two solid shapes began touching this step.
    export struct CollisionEnterEvent
    {
        ECS::Entity     a;
        ECS::Entity     b;
        ContactManifold manifold;   // normal points from a -> b
    }; // struct CollisionEnterEvent

    // Two solid shapes stopped touching. (No manifold: they are apart.)
    export struct CollisionExitEvent
    {
        ECS::Entity a;
        ECS::Entity b;
    }; // struct CollisionExitEvent

    // A shape entered a sensor volume.
    export struct TriggerEnterEvent
    {
        ECS::Entity sensor;   // the entity whose collider is a sensor
        ECS::Entity other;    // the visitor that entered it
    }; // struct TriggerEnterEvent

    // A shape left a sensor volume.
    export struct TriggerExitEvent
    {
        ECS::Entity sensor;
        ECS::Entity other;
    }; // struct TriggerExitEvent


    // =================================================================
    //
    //  RaycastHit -- result of PhysicsWorld/PhysicsSystem raycast.
    //
    // =================================================================
    //
    // Not an event, but grouped here because it also reports an
    // ECS::Entity. `hit` is false when the ray reached its full length
    // without striking anything (the other fields are then unset).
    //
    // =================================================================

    export struct RaycastHit
    {
        bool        hit      { false };
        ECS::Entity entity;                       // what was struck
        glm::vec2   point    { 0.0f, 0.0f };      // world-space hit point
        glm::vec2   normal   { 0.0f, 0.0f };      // surface normal at hit
        float       fraction { 0.0f };            // [0,1] along the ray
    }; // struct RaycastHit

} // namespace Physics