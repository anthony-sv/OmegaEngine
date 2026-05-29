module;

#include "glm/glm.hpp"

export module Engine.ECS:TransformComponents;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: Transform
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
    // Transform -- position, rotation, and scale of an entity in 2D.
    //
    // This is the most fundamental component in any game engine.
    // Nearly every entity has one -- it defines WHERE the entity
    // exists in the world and HOW BIG it appears.
    //
    // Convention:
    //   position: world-space XY coordinates. The batch renderer
    //             treats this as the quad's bottom-left corner (or
    //             center, depending on the draw overload used).
    //   rotation: degrees, counter-clockwise positive. The render
    //             system passes this to drawRotatedQuad() when != 0.
    //   scale:    multiplier on the entity's base size. {1,1} means
    //             "one world unit". {2,1} stretches horizontally.
    //
    // Note: this is a FLAT 2D transform -- no parent/child hierarchy.
    // Hierarchical transforms (local vs. world space) would need a
    // separate HierarchyComponent + transform propagation system.
    // -----------------------------------------------------------------

    export struct Transform
    {
        glm::vec2 position { 0.0f, 0.0f };
        float     rotation { 0.0f };            // degrees, CCW positive
        glm::vec2 scale    { 1.0f, 1.0f };
	}; // struct Transform

} // namespace ECS