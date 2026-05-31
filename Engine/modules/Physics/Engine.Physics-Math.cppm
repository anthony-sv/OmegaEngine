module;

#include "glm/glm.hpp"

export module Engine.Physics:Math;

import std;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: Physics::Math
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
    //  Geometry + contact value types (engine-side, glm-based).
    //
    // =================================================================
    //
    // These are OUR public types -- the Box2D equivalents (b2AABB,
    // b2Manifold) never cross a module boundary. The PhysicsWorld
    // translates between them inside its .cpp.
    //
    // =================================================================

    // An axis-aligned bounding box, used for region (overlap) queries.
    export struct AABB
    {
        glm::vec2 min { 0.0f, 0.0f };
        glm::vec2 max { 0.0f, 0.0f };

        [[nodiscard]] glm::vec2 center()  const { return (min + max) * 0.5f; }
        [[nodiscard]] glm::vec2 extents() const { return (max - min) * 0.5f; }
    }; // struct AABB

    // One point of contact in a manifold.
    export struct ContactPoint
    {
        glm::vec2 point       { 0.0f, 0.0f };   // world-space contact point
        float     penetration { 0.0f };         // overlap depth (>= 0 when touching)
    }; // struct ContactPoint

    // The contact geometry between two shapes for a single collision.
    // `normal` points from body A toward body B. A 2D manifold has at
    // most two points (a flat edge contact), so this rarely allocates.
    export struct ContactManifold
    {
        glm::vec2                 normal { 0.0f, 0.0f };
        std::vector<ContactPoint> points;
    }; // struct ContactManifold

} // namespace Physics