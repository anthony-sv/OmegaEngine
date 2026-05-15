export module Engine.Core:LayerStack;

import std;
import :Layer;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: LayerStack
*   half                    short
*    vec2                  ivec2
*     u32                  i32
*  _________;          ________;
* [RayTracing]       [Rasterizer]
*
════════════════════════════════════════════════════════════════════════════════*/

namespace Engine::Core {

    export class LayerStack {
    public:

        template<typename T, typename... Args>
            requires std::derived_from<T, ILayer>
        T& pushLayer(Args&&... args) {
            auto uptr = std::make_unique<T>(std::forward<Args>(args)...);
            auto& ref = *uptr;

            auto position = m_layers.begin()
                + static_cast<std::ptrdiff_t>(m_insertIndex);

            m_layers.insert(position, std::move(uptr));
            ++m_insertIndex;

            ref.onAttach();
            return ref;
        }

        template<typename T, typename... Args>
            requires std::derived_from<T, ILayer>
        T& pushOverlay(Args&&... args) {
            auto uptr = std::make_unique<T>(std::forward<Args>(args)...);
            auto& ref = *uptr;

            m_layers.push_back(std::move(uptr));

            ref.onAttach();
            return ref;
        }

        void popLayer(ILayer* layer) {
            auto zone = m_layers | std::ranges::views::take(m_insertIndex);
            auto it = std::ranges::find_if(zone,
                                           [layer](std::unique_ptr<ILayer> const& p) {
                                               return p.get() == layer;
                                           });

            auto boundary = m_layers.begin()
                + static_cast<std::ptrdiff_t>(m_insertIndex);

            if(it != boundary) {
                (*it)->onDetach();
                m_layers.erase(it);
                --m_insertIndex;
            }
        }

        void popOverlay(ILayer* overlay) {
            auto boundary = m_layers.begin()
                + static_cast<std::ptrdiff_t>(m_insertIndex);

            auto it = std::ranges::find_if(
                std::ranges::subrange(boundary, m_layers.end()),
                [overlay](std::unique_ptr<ILayer> const& p) {
                    return p.get() == overlay;
                });

            if(it != m_layers.end()) {
                (*it)->onDetach();
                m_layers.erase(it);
            }
        }

        // Ω::Per-frame dispatch ───────────────────────────────────────

        void updateAll(float dt) {
            for(auto& layer : m_layers)
                if(layer->enabled())
                    layer->onUpdate(dt);
        }

        void renderAll(float alpha) {
            for(auto& layer : m_layers)
                if(layer->enabled())
                    layer->onRender(alpha);
        }

        void imGuiAll() {
            for(auto& layer : m_layers)
                if(layer->enabled())
                    layer->onImGuiRender();
        }

        void dispatchEvent(Event& e) {
            for(auto it = m_layers.rbegin(); it != m_layers.rend(); ++it) {
                if(!(*it)->enabled()) continue;
                if((*it)->onEvent(e)) break;
            }
        }

        // Ω::Introspection ────────────────────────────────────────────
        [[nodiscard]] std::size_t size()         const { return m_layers.size(); }
        [[nodiscard]] std::size_t layerCount()   const { return m_insertIndex; }
        [[nodiscard]] std::size_t overlayCount() const { return m_layers.size() - m_insertIndex; }

        // Ω::Destructor — detach in reverse ───────────────────────────
        ~LayerStack() {
            for(auto it = m_layers.rbegin(); it != m_layers.rend(); ++it)
                (*it)->onDetach();
        }

    private:
        std::deque<std::unique_ptr<ILayer>> m_layers;
        std::size_t                         m_insertIndex { 0 };
    }; // class LayerStack

} // namespace Core