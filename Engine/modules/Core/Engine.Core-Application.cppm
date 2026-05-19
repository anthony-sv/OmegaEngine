export module Engine.Core:Application;

import :Window;
import :EventBus;
import :Error;
import :GameLoop;
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
		explicit Application(WindowProps const& props = {}, GameLoop::Config const& loopCfg = {});
		virtual ~Application();

		Application(Application const&) = delete;
		Application& operator=(Application const&) = delete;
		Application(Application&&) = delete;
		Application& operator=(Application&&) = delete;

		Result<int> run();

		// Ω::Subsystem access ─────────────────────────────────────────
		[[nodiscard]] Window&			window()			{ return *m_window; }
		[[nodiscard]] EventBus&         eventBus()          { return m_eventBus; }
		[[nodiscard]] SystemManager&	systemManager()		{ return m_systemManager; }
		[[nodiscard]] LayerStack&		layerStack()		{ return m_layerStack; }
		[[nodiscard]] GameLoop&		    gameLoop()		    { return m_gameLoop; }

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
		// Ω::Subsystems ───────────────────────────────────────────────
		std::optional<Window> m_window;
		EventBus              m_eventBus;
		GameLoop			  m_gameLoop;
		SystemManager         m_systemManager;
		LayerStack            m_layerStack;

		bool m_running { true };

		WindowProps m_initWindowProps;

		inline static Application* s_instance { nullptr };

		Result<int> runLoop();
	}; // class Application
} // namespace Core