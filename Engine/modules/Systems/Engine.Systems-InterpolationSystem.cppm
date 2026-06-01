export module Engine.Systems:InterpolationSystem;

import Engine.Core;   // Core::ISystem
import Engine.ECS;    // Registry, Transform, PreviousTransform
import std;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: InterpolationSystem
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
    //  InterpolationSystem -- enables smooth RENDER INTERPOLATION.
    //
    // =================================================================
    //
    // The simulation runs at a FIXED rate (GameLoop::fixedDt, e.g. 60Hz)
    // while the screen refreshes faster (144/160Hz...). Without help, the
    // renderer would show each sim state for several frames then jump,
    // producing micro-stutter that is very visible on high-refresh
    // displays.
    //
    // This system is the first half of the fix. Registered as the FIRST
    // system in a world (before MovementSystem/PhysicsSystem), each fixed
    // step it copies every entity's CURRENT Transform into its
    // PreviousTransform -- BEFORE the movers overwrite the Transform. So
    // after the step:
    //
    //   PreviousTransform = state at the start of this step (= last step)
    //   Transform         = state at the end   of this step
    //
    // The RenderSystem (second half) then draws lerp(previous, current,
    // alpha), where alpha = accumulator / fixedDt is the sub-step
    // fraction. Net effect: motion is smooth at ANY refresh rate, and the
    // simulation stays deterministic + rate-independent.
    //
    // It touches a DIFFERENT component pool (PreviousTransform) than the
    // one it iterates (Transform), so adding the component during view
    // iteration is safe.
    //
    // =================================================================

    export class InterpolationSystem final : public Core::ISystem
    {
    public:

        explicit InterpolationSystem(ECS::Registry& registry)
            : m_registry { registry }
        {}

        void onUpdate(float /*dt*/) override
        {
            for (auto&& [entity, transform] : m_registry.view<ECS::Transform>().each())
            {
                if (m_registry.hasComponent<ECS::PreviousTransform>(entity))
                {
                    auto& previous    = m_registry.getComponent<ECS::PreviousTransform>(entity);
                    previous.position = transform.position;
                    previous.rotation = transform.rotation;
                }
                else
                {
                    // First sight: seed previous == current so the first
                    // rendered frame doesn't lerp from a garbage origin.
                    m_registry.addComponent<ECS::PreviousTransform>(
                        entity, ECS::PreviousTransform { transform.position, transform.rotation });
                }
            }
        }

    private:
        ECS::Registry& m_registry;

    }; // class InterpolationSystem

} // namespace Systems