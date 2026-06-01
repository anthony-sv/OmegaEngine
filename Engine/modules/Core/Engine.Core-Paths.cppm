export module Engine.Core:Paths;

import std;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: Paths
*   half                    short
*    vec2                  ivec2
*     u32                  i32
*  _________;          ________;
* [RayTracing]       [Rasterizer]
*
════════════════════════════════════════════════════════════════════════════════*/

namespace Engine::Core
{

    // =================================================================
    //
    //  Paths -- executable-relative path resolution.
    //
    // =================================================================
    //
    // WHY: a project is the WORKING DIRECTORY (scenes/, assets/textures/
    // resolve there). But ENGINE + EDITOR resources -- the batch shader,
    // the editor's fonts/icon -- are NOT project content; they ship with
    // the executable. They must be found regardless of which project's
    // directory we are in. So they are resolved relative to the EXE, not
    // the cwd.
    //
    // resource() finds a (repo-relative) resource by walking up from the
    // executable -- the same trick the runtime uses to locate its sample
    // project. (A future "resources next to the exe" shipping layout can
    // be checked first inside resource(), without touching callers.)
    //
    // =================================================================

    export class Paths
    {
    public:

        // Directory that contains the running executable (cached).
        [[nodiscard]] static std::filesystem::path const& executableDir();

        // Walk `start` and its ancestors for `subpath`; first existing
        // match, or nullopt at the filesystem root.
        [[nodiscard]] static std::optional<std::filesystem::path> findUpwards(
            std::filesystem::path start, 
            std::filesystem::path const& subpath
        );

        // Resolve an engine/editor resource given as a repo-relative path
        // (e.g. "Engine/resources/shaders/batch_quad/vertex.glsl"). Found
        // by walking up from the executable. Falls back to the path
        // unchanged (cwd-relative) if nothing matches.
        [[nodiscard]] static std::filesystem::path resource(std::filesystem::path const& repoRelative);

        Paths() = delete;

    }; // class Paths

} // namespace Core