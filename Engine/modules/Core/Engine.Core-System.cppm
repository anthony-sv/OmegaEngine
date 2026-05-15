export module Engine.Core:System;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: ISystem
*   half                    short
*    vec2                  ivec2
*     u32                  i32
*  _________;          ________;
* [RayTracing]       [Rasterizer]
*
════════════════════════════════════════════════════════════════════════════════*/

namespace Engine::Core {

    export class ISystem {
    public:
        virtual ~ISystem() = default;

        virtual void onInit()           {}
        virtual void onUpdate(float dt) = 0;
        virtual void onShutdown()       {}

        bool enabled { true };
    }; // class ISystem

} // namespace Core