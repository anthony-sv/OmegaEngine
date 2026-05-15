module;

#include "glad/glad.h"
#include "GLFW/glfw3.h"

#ifdef _WIN32
#   define WIN32_LEAN_AND_MEAN
#   include <dwmapi.h>
#   include <windows.h>
#   pragma comment(lib, "dwmapi.lib")
extern "C" HWND glfwGetWin32Window(GLFWwindow*);
#endif

module Engine.Core:Window;

import :Window;
import :Error;
import std;

namespace Engine::Core {

    // Ω::Factory ──────────────────────────────────────────────────────
    Result<Window> Window::create(WindowProps const& props) {
        Window win;

        // Ω::GLFW init ────────────────────────────────────────────────
        if(!glfwInit()) {
            return std::unexpected(
                ErrorInfo::make(
                    ErrorCode::GlfwInitFailed,
                    "glfwInit() returned false"
                )
            );
        }

        win.m_ownsGlfw = true;

        // Ω::Hints ────────────────────────────────────────────────────
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(
            GLFW_DECORATED,
            props.decorated ? 
            GLFW_TRUE :
            GLFW_FALSE
        );
        glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);

        // Ω::Create ───────────────────────────────────────────────────
        auto* raw = glfwCreateWindow(props.width, props.height, props.title.data(),nullptr, nullptr);

        if(!raw) {
            glfwTerminate();
            return std::unexpected(
                ErrorInfo::make(
                    ErrorCode::WindowCreationFailed,
                    std::format("glfwCreateWindow failed for '{}' ({}x{})",props.title, props.width, props.height)
                )
            );
        }

        // Transfer ownership to the unique_ptr with custom deleter
        win.m_handle.reset(raw);
        win.m_width = props.width;
        win.m_height = props.height;

        glfwMakeContextCurrent(win.m_handle.get());
        win.setVsync(props.vsync);

        // Ω::Glad ─────────────────────────────────────────────────────
        if(!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
            glfwTerminate();
            return std::unexpected(
                ErrorInfo::make(
                    ErrorCode::GladLoadFailed,
                    "gladLoadGLLoader failed — GPU may not support GL 4.6"
                )
            );
        }

        // Ω::Centre on primary monitor ────────────────────────────────
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        GLFWvidmode const* mode = glfwGetVideoMode(monitor);
        int mx {}, my {};
        glfwGetMonitorPos(monitor, &mx, &my);
        glfwSetWindowPos(
            win.m_handle.get(),
            mx + (mode->width - props.width) / 2,
            my + (mode->height - props.height) / 2
        );

        // Ω::Resize callback ──────────────────────────────────────────
        glfwSetWindowUserPointer(win.m_handle.get(), &win);
        glfwSetFramebufferSizeCallback(
            win.m_handle.get(),
            [](GLFWwindow* glfw, int w, int h) {
                auto* self = static_cast<Window*>(glfwGetWindowUserPointer(glfw));
                self->m_width = w;
                self->m_height = h;
                glViewport(0, 0, w, h);
            }
        );

        // Ω::Win32 tweaks ─────────────────────────────────────────────
#ifdef _WIN32
        HWND hwnd = glfwGetWin32Window(win.m_handle.get());

        MARGINS margins { 1, 1, 1, 1 };
        DwmExtendFrameIntoClientArea(hwnd, &margins);

        LONG style = GetWindowLong(hwnd, GWL_STYLE);
        style |= WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
        SetWindowLong(hwnd, GWL_STYLE, style);
        SetWindowPos(
            hwnd, 
            nullptr, 
            0, 
            0, 
            0, 
            0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED
        );

        BOOL dark = TRUE;
        DwmSetWindowAttribute(hwnd,DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
#endif

        std::println("[Ω::Window] created {}x{} '{}'",props.width, props.height, props.title);

        return win;
    }

    // Ω::Move constructor
    Window::Window(Window&& other) noexcept
        : m_handle { std::move(other.m_handle) }
        , m_width { other.m_width }
        , m_height { other.m_height }
        , m_ownsGlfw { other.m_ownsGlfw }
    {
        other.m_ownsGlfw = false;
    }

	// Ω::Move assignment operator
    Window& Window::operator=(Window&& other) noexcept {
        if(this != &other) {
            m_handle = std::move(other.m_handle);
            m_width = other.m_width;
            m_height = other.m_height;
            m_ownsGlfw = std::exchange(other.m_ownsGlfw, false);
        }
        return *this;
    }

    // Ω::Destructor ───────────────────────────────────────────────────
    Window::~Window() {
        if(m_handle) {
            std::println("[Ω::Window] destroyed");
        }
        if(m_ownsGlfw) {
            glfwTerminate();
        }
    }

    // Ω::Per-frame ────────────────────────────────────────────────────
    void Window::pollEvents()  const { glfwPollEvents(); }
    void Window::swapBuffers() const { glfwSwapBuffers(m_handle.get()); }

    // Ω::Queries ──────────────────────────────────────────────────────
    bool Window::shouldClose() const {
        return glfwWindowShouldClose(m_handle.get());
    }
    bool Window::isMinimized() const {
        return glfwGetWindowAttrib(m_handle.get(), GLFW_ICONIFIED) == GLFW_TRUE;
    }
    bool Window::isMaximized() const {
        return glfwGetWindowAttrib(m_handle.get(), GLFW_MAXIMIZED) == GLFW_TRUE;
    }

    // Ω::Commands ─────────────────────────────────────────────────────
    void Window::minimize()                       const { glfwIconifyWindow(m_handle.get()); }
    void Window::maximize()                       const { glfwMaximizeWindow(m_handle.get()); }
    void Window::restore()                        const { glfwRestoreWindow(m_handle.get()); }
    void Window::close()                          const { glfwSetWindowShouldClose(m_handle.get(), GLFW_TRUE); }
    void Window::setTitle(std::string_view title) const { glfwSetWindowTitle(m_handle.get(), title.data()); }
    void Window::setVsync(bool enabled)           const { glfwSwapInterval(enabled ? 1 : 0); }
    void Window::setPosition(int x, int y)        const { glfwSetWindowPos(m_handle.get(), x, y); }
    void Window::setSize(int w, int h)            const { glfwSetWindowSize(m_handle.get(), w, h); }

} // namespace Core