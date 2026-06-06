export module Engine.ECS:MarkerComponents;

import std;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: Markers
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
    // MarkerComponent -- a typed gameplay OBJECT placed in a scene.
    //
    // A marker is an authoring-time anchor that game logic reads but the
    // player never sees as art: where to spawn, a trigger volume, an NPC
    // or item slot, a camera-bounds region. It is just an entity with a
    // Transform + this component; the editor draws an icon for it (since
    // it has no sprite) so you can see and position it.
    //
    //   SpawnPoint  -- where the player starts / respawns.
    //   Trigger     -- a volume that fires an event when a body enters
    //                  (paired with a sensor collider). Drives things like
    //                  checkpoints, level exits, save points.
    //   NPC / Item  -- a placeholder slot for a character / pickup.
    //   CameraBound -- a region the camera is kept within.
    //
    // `tag` is an optional reference id ("exit", "checkpoint_1", ...) so
    // logic can find a specific marker by name.
    // -----------------------------------------------------------------

    export enum class MarkerType : std::uint8_t
    {
        SpawnPoint,
        Trigger,
        NPC,
        Item,
        CameraBound,
	}; // enum class MarkerType

    export struct MarkerComponent
    {
        MarkerType  type { MarkerType::SpawnPoint };
        std::string tag  {};   // optional reference id
	}; // struct MarkerComponent

} // namespace ECS