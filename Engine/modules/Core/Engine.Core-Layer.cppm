export module Engine.Core:Layer;

import std;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: ILayer
*   half                    short
*    vec2                  ivec2
*     u32                  i32
*  _________;          ________;
* [RayTracing]       [Rasterizer]
*
════════════════════════════════════════════════════════════════════════════════*/

namespace Engine::Core {

    export struct Event {
        std::uint32_t type { 0 };
        bool          handled { false };
    };

    export class ILayer {
    public:
        explicit ILayer(std::string_view name = "Layer")
            : m_name { name } {}

        virtual ~ILayer() = default;

        virtual void onAttach()            {}
        virtual void onDetach()            {}
        virtual void onUpdate(float dt)    {}
        virtual void onRender(float alpha) {}
        virtual void onImGuiRender()       {}

        // Return true to consume the event and stop propagation
        virtual bool onEvent(Event const&) { return false; }

        [[nodiscard]] std::string_view  name()    const { return m_name; }
        [[nodiscard]] bool              enabled() const { return m_enabled; }
        void                            setEnabled(bool v) { m_enabled = v; }

    protected:
        std::string m_name;
        bool        m_enabled { true };
    }; // class ILayer

} // namespace Core