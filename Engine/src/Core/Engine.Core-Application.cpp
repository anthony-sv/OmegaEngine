module;

#include "glad/glad.h"

module Engine.Core:Application;

import :Application;
import :Error;
import :Window;
import :Input;
import :GameLoop;
import std;

namespace Engine::Core {

    Application::Application(WindowProps const& props, GameLoop::Config const& loopCfg)
        : m_initWindowProps { props }
        , m_gameLoop { loopCfg }
    {
        s_instance = this;
        std::println("[Ω::Application] created");
    }

    Application::~Application() {
        s_instance = nullptr;
        std::println("[Ω::Application] destroyed");
    }

    Result<int> Application::run() {
        // Ω::Create window then run──────────────────────────────────────────
        return Window::create(m_initWindowProps)
            .and_then(
                [this](Window&& win) -> Result<int> {
                    m_window.emplace(std::move(win));
                    return runLoop();
                }
            );
    }

    Result<int> Application::runLoop() {
        using Clock = std::chrono::steady_clock;
        using Duration = std::chrono::duration<float>;

        // Ω::Init ─────────────────────────────────────────────────────
        // Bring the input device online (sibling of the window) before
        // user init, so layers/systems can query it from frame one.
        Input::init(*m_window, m_eventBus);

        onInit();
        m_systemManager.initAll();

        std::println("[Ω::Application] entering main loop");
        std::println(
            "[Ω::Application] {} layers, {} overlays, fixed dt = {:.1f}ms",
            m_layerStack.layerCount(),
            m_layerStack.overlayCount(),
            m_gameLoop.fixedDt() * 1000.0f
        );

        auto previousTime = Clock::now();
        m_gameLoop.reset();

        // Ω::Loop ─────────────────────────────────────────────────────
        while(m_running && !m_window->shouldClose()) {

            // Ω::Time ─────────────────────────────────────────────────
            auto  currentTime = Clock::now();
            float frameTime = Duration(currentTime - previousTime).count();
            previousTime = currentTime;

            m_gameLoop.addFrameTime(frameTime);

            // Ω::Input ────────────────────────────────────────────────
            // pollEvents() pumps the OS queue (updating GLFW's internal
            // key/mouse state); Input::update() then snapshots + diffs it
            // once per frame, firing key/mouse/action events on the bus.
            m_window->pollEvents();
            Input::update();

            // Ω::Fixed-timestep update ────────────────────────────────
            while(m_gameLoop.consumeTick()) {
                m_systemManager.updateAll(m_gameLoop.fixedDt());
                m_layerStack.updateAll(m_gameLoop.fixedDt());
            }

            // Ω::Render ───────────────────────────────────────────────
            if(!m_window->isMinimized()) {
                glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT);

                m_layerStack.renderAll(m_gameLoop.alpha());
                m_layerStack.imGuiAll();

                m_window->swapBuffers();
            }
        }

        // Ω::Shutdown ─────────────────────────────────────────────────
        onShutdown();
        Input::shutdown();
        std::println("[Ω::Application] exited main loop");

        return 0;
    }
} // namespace Core