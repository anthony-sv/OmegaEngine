export module Engine.Scene:Serializer;

import :World;
import Engine.Core;   // Core::VoidResult, Core::AssetManager
import std;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: SceneSerializer
*   half                    short
*    vec2                  ivec2
*     u32                  i32
*  _________;          ________;
* [RayTracing]       [Rasterizer]
*
════════════════════════════════════════════════════════════════════════════════*/

namespace Engine::Scene
{

    // =================================================================
    //
    //  SceneSerializer -- saves/loads a World to/from a JSON file.
    //
    // =================================================================
    //
    // Serialization writes DATA (entities + their components), not logic
    // (systems are code, re-added by the world's setup). So load()
    // replaces a world's ENTITIES (World::clearEntities) while leaving
    // its systems intact.
    //
    // This is where the AssetManager pays off: a SpriteRenderer's texture
    // is a runtime pointer that can't be written to a file -- instead we
    // serialize its texturePath, and on load resolve it back to a pointer
    // via assets.load<Texture2D>(path).
    //
    // nlohmann/json is used only inside the .cpp (kept out of this
    // interface), so consumers never see it.
    //
    // =================================================================

    export class SceneSerializer
    {
    public:
        // Write the world's entities to `file` as JSON. (Non-const: it
        // iterates the registry, whose entity-enumeration API is non-const.)
        [[nodiscard]] static Core::VoidResult save(
            World& world,
            std::filesystem::path const& file
        );

        // Replace the world's entities from a JSON `file`. Systems are
        // kept. Texture references are resolved through `assets`.
        [[nodiscard]] static Core::VoidResult load(
            World& world,
            std::filesystem::path const& file,
            Core::AssetManager& assets
        );

        SceneSerializer() = delete;

    }; // class SceneSerializer

} // namespace Scene