export module Engine.Scripting:ScriptSystem;

import Engine.Core;   // Core::ISystem
import Engine.ECS;    // Registry, ScriptComponent
import :ScriptHost;
import std;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: ScriptSystem
*   half                    short
*    vec2                  ivec2
*     u32                  i32
*  _________;          ________;
* [RayTracing]       [Rasterizer]
*
════════════════════════════════════════════════════════════════════════════════*/

namespace Engine::Scripting
{

    // =================================================================
    //
    //  ScriptSystem -- ticks every entity's C# script.
    //
    // =================================================================
    //
    // An UPDATE-phase Core::ISystem. onInit boots the host; each onUpdate
    // points the script API at this world, then for every ScriptComponent:
    // lazily instantiates its managed object (createScript) the first time,
    // and calls into C# OnUpdate. `managedDir` is where OmegaEngine.dll +
    // its runtimeconfig.json live (the executable directory).
    //
    // =================================================================

    export class ScriptSystem final : public Core::ISystem
    {
    public:

        ScriptSystem(ECS::Registry& registry, std::filesystem::path managedDir)
            : m_registry   { registry }
            , m_managedDir { std::move(managedDir) }
        {}

        void onInit() override
        {
            ScriptHost::instance().ensureInitialized(m_managedDir);
        }

        void onUpdate(float dt) override
        {
            auto& host = ScriptHost::instance();
            if (!host.ready())
                return;

            host.bindRegistry(&m_registry);

            // The managed runtime instantiates each script on first sight and
            // keys instances by entity id, so we just tick every one.
            for (auto&& [entity, script] : m_registry.view<ECS::ScriptComponent>().each())
                host.tick(static_cast<std::uint32_t>(entity), script.className, dt);
        }

    private:
        ECS::Registry&        m_registry;
        std::filesystem::path m_managedDir;

    }; // class ScriptSystem

} // namespace Scripting