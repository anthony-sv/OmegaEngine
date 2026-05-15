export module Engine.Core:SystemManager;

import std;
import :System;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: SystemManager
*   half                    short
*    vec2                  ivec2
*     u32                  i32
*  _________;          ________;
* [RayTracing]       [Rasterizer]
*
════════════════════════════════════════════════════════════════════════════════*/

namespace Engine::Core {

    class SystemManager {
    public:

        template<typename T, typename... Args>
            requires std::derived_from<T, ISystem>
        T& add(Args&&... args) {
            auto uptr = std::make_unique<T>(std::forward<Args>(args)...);
            auto& ref = *uptr;
            m_systems.push_back(std::move(uptr));
            return ref;
        }

        void initAll() {
            for(auto& sys : m_systems)
                sys->onInit();
        }

        void updateAll(float dt) {
            for(auto& sys : m_systems)
                if(sys->enabled)
                    sys->onUpdate(dt);
        }

        void shutdownAll() {
            for(auto it = m_systems.rbegin(); it != m_systems.rend(); ++it)
                (*it)->onShutdown();
        }

        ~SystemManager() { shutdownAll(); }

    private:
        std::vector<std::unique_ptr<ISystem>> m_systems;
    }; // class SystemManager

} // namespace Core