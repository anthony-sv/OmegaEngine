export module Engine.Core:Application;

import :Window;
import :Error;
import :SystemManager;
import :LayerStack;
import :Layer;
import :System;
import std;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: Application
*   half                    short
*    vec2                  ivec2
*     u32                  i32
*  _________;          ________;
* [RayTracing]       [Rasterizer]
*
════════════════════════════════════════════════════════════════════════════════*/

namespace Engine::Core {

	export class Application {
	public:
		explicit Application(WindowProps const& props = {});
		virtual ~Application();

		Application(Application const&) = delete;
		Application& operator=(Application const&) = delete;
		Application(Application&&) = delete;
		Application& operator=(Application&&) = delete;

		Result<int> run();

		// Ω::Subsystem access ─────────────────────────────────────────
		[[nodiscard]] Window&			window()			{ return *m_window; }
		[[nodiscard]] SystemManager&	systemManager()		{ return m_systemManager; }
		[[nodiscard]] LayerStack&		layerStack()		{ return m_layerStack; }

		// Ω::Layer convenience ────────────────────────────────────────
		template<typename T, typename... Args>
			requires std::derived_from<T, ILayer>
		T& pushLayer(Args&&... args) {
			return m_layerStack.pushLayer<T>(std::forward<Args>(args)...);
		}

		template<typename T, typename... Args>
			requires std::derived_from<T, ILayer>
		T& pushOverlay(Args&&... args) {
			return m_layerStack.pushOverlay<T>(std::forward<Args>(args)...);
		}

		// Ω::System convenience ───────────────────────────────────────
		template<typename T, typename... Args>
			requires std::derived_from<T, ISystem>
		T& addSystem(Args&&... args) {
			return m_systemManager.add<T>(std::forward<Args>(args)...);
		}

		// Ω::Shutdown request ─────────────────────────────────────────
		void quit() { m_running = false; }

		[[nodiscard]] static Application& get() { return *s_instance; }
	protected:
		virtual void onInit()     {}
		virtual void onShutdown() {}
	private:
		std::optional<Window> m_window;
		SystemManager         m_systemManager;
		LayerStack            m_layerStack;

		bool m_running { true };

		static constexpr float FIXED_DT { 1.0f / 60.0f };
		static constexpr float MAX_FRAME_DT { 0.25f };

		WindowProps m_initWindowProps;

		inline static Application* s_instance { nullptr };
	}; // class Application
} // namespace Core