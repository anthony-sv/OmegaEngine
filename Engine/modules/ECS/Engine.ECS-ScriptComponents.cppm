export module Engine.ECS:ScriptComponents;

import std;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: Script
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
    // ScriptComponent -- attaches a C# script (by class name) to an
    // entity. Gameplay logic lives in C# scripts in the project, NOT in
    // engine components; this is the one generic hook that binds an entity
    // to a script class.
    //
    // Pure DATA: just the managed type name (e.g. "Game.Mover"). The managed
    // runtime keys script INSTANCES by entity id, so there is no runtime
    // handle to carry here -- the component stays trivially serializable.
    // -----------------------------------------------------------------

    export struct ScriptComponent
    {
        std::string className {};      // the C# class to run, e.g. "Game.Mover"
	}; // struct ScriptComponent

} // namespace ECS