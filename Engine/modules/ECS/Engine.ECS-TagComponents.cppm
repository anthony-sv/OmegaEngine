export module Engine.ECS:TagComponents;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: Tags
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
    // Tag components -- empty structs used as boolean markers.
    //
    // In ECS, a "tag" component has no data. Its presence or absence
    // on an entity IS the information. Basically a boolean flag:
    //   entity has Disabled  →  true
    //   entity lacks Disabled →  false
    //
    // The ECS stores these extremely efficiently -- no per-entity
    // memory allocation, just a sparse set tracking which entities
    // carry the tag. sizeof(Disabled) == 1 (empty struct), but the
    // pool doesn't actually store instances. The tag pool is purely
    // structural.
    //
    // Systems can include or exclude tags in their queries:
    //
    //   // Skip disabled entities in your callback:
    //   registry.each<Transform, SpriteRenderer>(
    //       [](Entity e, Transform& tf, SpriteRenderer& sr) {
    //           if (e.has<Disabled>()) return;
    //           // ... process entity
    //       });
    //
    // To toggle:
    //   entity.add<Disabled>();       // disable
    //   entity.remove<Disabled>();    // re-enable
    //
    // This is more cache-friendly than an "active" bool inside every
    // component, because most entities are active -- so checking a
    // bool on every entity wastes time. With a tag, disabled entities
    // are simply excluded from the iteration set upfront.
    // -----------------------------------------------------------------

    // Entity exists but all systems should skip it.
    // Use this instead of destroying entities when you need to
    // temporarily hide something (object pooling, cutscenes,
    // respawn timers, off-screen culling).

    export struct Disabled {};

} // namespace ECS