export module Engine.Core:GameLoop;

import std;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: GameLoop
*   half                    short
*    vec2                  ivec2
*     u32                  i32
*  _________;          ________;
* [RayTracing]       [Rasterizer]
*
════════════════════════════════════════════════════════════════════════════════*/

namespace Engine::Core {

    // Ω::GameLoop ─────────────────────────────────────────────────────────────
    export class GameLoop {
    public:
        // Ω::Configuration ────────────────────────────────────────────────────
        struct Config {
            float fixedDt { 1.0f / 60.0f };
            float maxFrameDt { 0.25f };
        };

        explicit GameLoop(Config const& cfg = {})
            : m_cfg { cfg } {}

        // Ω::Per-frame ─────────────────────────────────────────────────────────
        void addFrameTime(float rawDt);
        bool consumeTick();
        void reset();

        // Ω::Accessors ────────────────────────────────────────────────────────
        [[nodiscard]] float fixedDt()     const { return m_cfg.fixedDt; }
        [[nodiscard]] float maxFrameDt()  const { return m_cfg.maxFrameDt; }
        [[nodiscard]] float accumulator() const { return m_accumulator; }

        [[nodiscard]] float alpha() const {
            return m_accumulator / m_cfg.fixedDt;
        }

    private:
        Config m_cfg;
        float  m_accumulator { 0.0f };
    }; // class GameLoop

} // namespace Core