module;

#include "GLFW/glfw3.h"

export module Engine.Core:Window;

import std;
import :Error;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: Window
*   half                    short
*    vec2                  ivec2
*     u32                  i32
*  _________;          ________;
* [RayTracing]       [Rasterizer]
*
════════════════════════════════════════════════════════════════════════════════*/

namespace Engine::Core {

    export struct WindowProps {
        std::string_view title { "ΩmegaEngine" };
        int              width { 1280 };
        int              height { 720 };
        bool             vsync { true };
        bool             decorated { false };
    };

    export class Window {
    public:
        [[nodiscard]] static Result<Window> create(WindowProps const& props = {});

        ~Window();
        Window(Window&&) noexcept;
        Window& operator=(Window&&) noexcept;
        Window(Window const&) = delete;
        Window& operator=(Window const&) = delete;

        // Ω::Per-frame ────────────────────────────────────────────────
        void pollEvents()  const;
        void swapBuffers() const;

        // Ω::Queries ──────────────────────────────────────────────────
        [[nodiscard]] bool shouldClose() const;
        [[nodiscard]] bool isMinimized() const;
        [[nodiscard]] bool isMaximized() const;
        [[nodiscard]] int  width()       const { return m_width; }
        [[nodiscard]] int  height()      const { return m_height; }

        // Ω::Commands ─────────────────────────────────────────────────
        void minimize()                       const;
        void maximize()                       const;
        void restore()                        const;
        void close()                          const;
        void setTitle(std::string_view title) const;
        void setVsync(bool enabled)           const;
        void setPosition(int x, int y)        const;
        void setSize(int w, int h)            const;

        [[nodiscard]] GLFWwindow* nativeHandle() const { return m_handle.get(); }

    private:
        Window() = default;

        struct GLFWwindowDeleter {
            void operator()(GLFWwindow* ptr) const {
                if(ptr) glfwDestroyWindow(ptr);
            }
        };

        std::unique_ptr<GLFWwindow, GLFWwindowDeleter> m_handle;
        int m_width { 0 };
        int m_height { 0 };
        bool m_ownsGlfw { false };
    }; // class Window

} // namespace Core