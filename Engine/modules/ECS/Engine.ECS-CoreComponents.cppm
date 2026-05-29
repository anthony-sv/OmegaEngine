export module Engine.ECS:CoreComponents;

import std;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: Core
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
    // NameComponent -- gives an entity a human-readable name.
    //
    // Every entity in ECS is just an integer ID (like a database
    // primary key). That's great for performance but terrible for
    // debugging: "Entity 42 has a bug" means nothing. NameComponent
    // lets you name entities so they show up as "Player", "Camera",
    // "Background Tile #7" in the editor and log messages.

    // Usage:
    //   auto player = registry.create("Player");  // auto-adds NameComponent
    //   // or manually:
    //   entity.add<NameComponent>({ "Player" });
    // -----------------------------------------------------------------

    export struct NameComponent
    {
        std::string name;
	}; // struct NameComponent

} // namespace ECS