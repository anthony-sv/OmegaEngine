module;

#include "glad/glad.h"

module Engine.Core:Application;

import :Application;
import :Error;
import :Window;
import std;

namespace Engine::Core {

    Application::Application(WindowProps const& props)
        : m_initWindowProps { props }
    {
        s_instance = this;
        std::println("[Ω::Application] created");
    }

    Application::~Application() {
        s_instance = nullptr;
        std::println("[Ω::Application] destroyed");
    }

    Result<int> Application::run() {
        using Clock = std::chrono::steady_clock;
        using Duration = std::chrono::duration<float>;

        // Ω::Create window ──────────────────────────────────────────
        auto windowResult = Window::create(m_initWindowProps);
        if(!windowResult) {
            return std::unexpected(windowResult.error());
        }
        m_window.emplace(std::move(*windowResult));

        // Ω::Init ──────────────────────────────────────────────────
        onInit();
        m_systemManager.initAll();

        std::println("[Ω::Application] entering main loop");
        std::println(
            "[Ω::Application] {} layers, {} overlays, fixed dt = {:.1f}ms",
                     m_layerStack.layerCount(),
                     m_layerStack.overlayCount(),
                     FIXED_DT * 1000.0f
        );

        auto  previousTime = Clock::now();
        float accumulator = 0.0f;

        // Ω::Loop ─────────────────────────────────────────────────────
        while(m_running && !m_window->shouldClose()) {

            // Ω::Time ─────────────────────────────────────────────────
            auto  currentTime = Clock::now();
            float frameTime = Duration(currentTime - previousTime).count();
            previousTime = currentTime;
            frameTime = std::min(frameTime, MAX_FRAME_DT);
            accumulator += frameTime;

            // Ω::Input ────────────────────────────────────────────────
            m_window->pollEvents();

            // Ω::Fixed-timestep update ────────────────────────────────
            while(accumulator >= FIXED_DT) {
                m_systemManager.updateAll(FIXED_DT);
                m_layerStack.updateAll(FIXED_DT);
                accumulator -= FIXED_DT;
            }

            // Ω::Render ───────────────────────────────────────────────
            if(!m_window->isMinimized()) {
                float const alpha = accumulator / FIXED_DT;

                glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT);

                m_layerStack.renderAll(alpha);
                m_layerStack.imGuiAll();

                m_window->swapBuffers();
            }
        }

        // Ω::Shutdown ─────────────────────────────────────────────────
        onShutdown();
        std::println("[Ω::Application] exited main loop");

        return 0;
    }
} // namespace Core