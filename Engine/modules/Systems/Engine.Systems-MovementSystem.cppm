module;

#include "glm/glm.hpp"

export module Engine.Systems:MovementSystem;

import Engine.Core;   // Core::ISystem
import Engine.ECS;    // Registry, Transform, Velocity2D
import std;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: MovementSystem
*   half                    short
*    vec2                  ivec2
*     u32                  i32
*  _________;          ________;
* [RayTracing]       [Rasterizer]
*
════════════════════════════════════════════════════════════════════════════════*/

namespace Engine::Systems
{

    // =================================================================
    //
    //  MovementSystem -- integrates Velocity2D into Transform.
    //
    // =================================================================
    //
    // The engine's first UPDATE-phase system, and the one that proves
    // the Scene/ISystem wiring works end to end.
    //
    //   for each entity with Transform + Velocity2D:
    //       position += linear  * dt      (world units / second)
    //       rotation += angular * dt      (degrees / second)
    //
    // This is plain Euler integration -- enough for projectiles,
    // drifting objects, spinning props, scrolling backgrounds. (A real
    // physics solver, when we add Box2D, will own dynamic bodies
    // instead; MovementSystem stays for the simple kinematic cases.)
    //
    // WHY IT DERIVES FROM Core::ISystem:
    //
    //   ISystem is the update-phase abstraction (onInit / onUpdate(dt)
    //   / onShutdown). It had no way to reach the Registry on its own --
    //   the Scene fixes that by injecting a Registry& at construction.
    //   So this system is a normal ISystem that happens to hold the
    //   registry it was given. This is exactly the role ISystem was
    //   always meant to play; it just wasn't wired up until now.
    //
    // =================================================================

    export class MovementSystem final : public Core::ISystem
    {
    public:

        // The Scene injects its Registry& here (addSystem passes it as
        // the first constructor argument automatically).
        explicit MovementSystem(ECS::Registry& registry)
            : m_registry { registry }
        {}

        void onUpdate(float dt) override
        {
            // Every entity that has BOTH a Transform and a Velocity2D
            // gets integrated this tick. Components are referenced in
            // place -- we write straight back into the packed arrays.
            for (
                auto&& [entity, transform, velocity]: m_registry.view<ECS::Transform, ECS::Velocity2D>().each()
                )
            {
                transform.position += velocity.linear  * dt;

                // Wrap into [0, 360) so the angle never grows without
                // bound (otherwise it accumulates forever and eventually
                // loses float precision -- and serializes as e.g. 4652°).
                transform.rotation = std::fmod(transform.rotation + velocity.angular * dt, 360.0f);
                if (transform.rotation < 0.0f)
                    transform.rotation += 360.0f;
            }
        }

    private:
        ECS::Registry& m_registry;

    }; // class MovementSystem

} // namespace Systems