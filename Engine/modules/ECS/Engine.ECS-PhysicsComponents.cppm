module;

#include "glm/glm.hpp"

export module Engine.ECS:PhysicsComponents;

import std;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: Physics
*   half                    short
*    vec2                  ivec2
*     u32                  i32
*  _________;          ________;
* [RayTracing]       [Rasterizer]
*
════════════════════════════════════════════════════════════════════════════════*/

namespace Engine::ECS
{

    // -----------------------------------------------------------------
    // Velocity2D -- linear and angular velocity for movement.
    //
    // A simple movement system applies this to Transform each frame:
    //   transform.position += velocity.linear * dt;
    //   transform.rotation += velocity.angular * dt;

    // For a full physics engine (Box2D), velocity would be managed
    // by the physics solver. But for simple movement (projectiles,
    // moving platforms, scrolling backgrounds), this is all you need.
    // -----------------------------------------------------------------

    export struct Velocity2D
    {
        glm::vec2 linear  { 0.0f, 0.0f };       // world units per second
        float     angular { 0.0f };             // degrees per second
	}; // struct Velocity2D


    // -----------------------------------------------------------------
    // RigidBody2D -- physics body configuration.
    //
    // Defines HOW this entity participates in physics simulation.
    // This is a description, not a simulation -- it tells a future
    // physics system (Box2D) how to create the body.
    //
    // BodyType:
    //   Static    -- never moves. Walls, floors, world geometry.
    //               Zero cost for the physics solver.
    //   Dynamic   -- fully simulated. Gravity, forces, collisions.
    //               This is what players, enemies, and projectiles use.
    //   Kinematic -- moves by code (velocity), not by forces.
    //               Pushes dynamic bodies on contact but isn't pushed
    //               back. Moving platforms, elevators, crushers.
    // -----------------------------------------------------------------

    export struct RigidBody2D
    {
        enum class BodyType : std::uint8_t
        {
            Static,
            Dynamic,
            Kinematic
        };

        BodyType type          { BodyType::Dynamic };
        float    mass          { 1.0f };
        float    gravityScale  { 1.0f };         // 0 = no gravity
        bool     fixedRotation { false };        // prevent tumbling
	}; // struct RigidBody2D


    // -----------------------------------------------------------------
    // BoxCollider2D -- axis-aligned rectangular collision shape.
    //
    // size:   full width and height of the box (NOT half-extents).
    //         Matches the convention used by Transform::scale.
    //
    // offset: displacement from the entity's Transform position.
    //         Useful when the collision box is smaller than the
    //         sprite (e.g., a character's feet hitbox).
    //
    // density / friction / restitution:
    //   Physics material properties.
    //   - density:     affects mass calculation (mass = area * density).
    //   - friction:    sliding resistance. 0 = ice, 1 = rubber.
    //   - restitution: bounciness. 0 = no bounce, 1 = perfect bounce.
    // -----------------------------------------------------------------

    export struct BoxCollider2D
    {
        glm::vec2 size   { 1.0f, 1.0f };
        glm::vec2 offset { 0.0f, 0.0f };

        // Physics material
        float density     { 1.0f };
        float friction    { 0.3f };
        float restitution { 0.0f };
	}; // struct BoxCollider2D


    // -----------------------------------------------------------------
    // CircleCollider2D -- circular collision shape.
    //
    // More efficient than BoxCollider2D for round objects (coins,
    // bullets, balls). Circle-vs-circle is the cheapest collision
    // test in physics: just compare distance to sum of radii.
    // -----------------------------------------------------------------

    export struct CircleCollider2D
    {
        float     radius { 0.5f };
        glm::vec2 offset { 0.0f, 0.0f };

        float density     { 1.0f };
        float friction    { 0.3f };
        float restitution { 0.0f };
	}; // struct CircleCollider2D

} // namespace ECS