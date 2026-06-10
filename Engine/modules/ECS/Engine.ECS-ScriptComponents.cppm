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
    // Pure DATA: the managed type name (e.g. "Game.Mover") plus any authored
    // field overrides. The managed runtime keys script INSTANCES by entity id,
    // so there is no runtime handle to carry here -- the component stays
    // trivially serializable.
    //
    // `fields` holds editor-authored overrides for a script's serializable
    // fields (public / [SerializeField]), keyed by field name, value stored as
    // a string. The C# side converts to the real field type by reflection; a
    // field absent here keeps the script's default. (std::map for stable,
    // diff-friendly serialization order.)
    // -----------------------------------------------------------------

    export struct ScriptComponent
    {
        std::string                        className {};   // the C# class to run, e.g. "Game.Mover"
        std::map<std::string, std::string> fields    {};   // authored field overrides (value as string)
	}; // struct ScriptComponent

    // -----------------------------------------------------------------
    // GraphComponent -- attaches a visual-script GRAPH (by name) to an
    // entity. The graph itself is an EDITOR asset (graphs/<name>.ngraph);
    // saving it code-generates a C# class "Game.<name>", which is what the
    // runtime actually executes -- the engine never parses graph files.
    //
    // The graph name must be a valid C# identifier (it becomes the class
    // name). An entity should carry a Script OR a Graph, not both: script
    // instances are keyed by entity id, so two class names on one entity
    // would recreate the instance every frame.
    // -----------------------------------------------------------------

    export struct GraphComponent
    {
        std::string graphName {};   // graphs/<graphName>.ngraph -> class "Game.<graphName>"
	}; // struct GraphComponent

} // namespace ECS