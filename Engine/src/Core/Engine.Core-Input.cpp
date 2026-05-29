module;

#include "GLFW/glfw3.h"
#include "glm/glm.hpp"

module Engine.Core:Input;

import :Input;
import :Window;
import :EventBus;
import std;

namespace Engine::Core
{
    namespace
    {
        // ── Device state ────────────────────────────────────────────
        // "cur" = this frame, "prev" = last frame -- the pair enables
        // edge detection (pressed / released).

        GLFWwindow*      g_window    { nullptr };
        EventBus*        g_bus       { nullptr };
        ActionMap const* g_actionMap { nullptr };   // active world's bindings

        std::array<bool, GLFW_KEY_LAST + 1>          g_keyCur  {};
        std::array<bool, GLFW_KEY_LAST + 1>          g_keyPrev {};
        std::array<bool, GLFW_MOUSE_BUTTON_LAST + 1> g_btnCur  {};
        std::array<bool, GLFW_MOUSE_BUTTON_LAST + 1> g_btnPrev {};

        glm::vec2 g_mouseCur  { 0.0f, 0.0f };
        glm::vec2 g_mousePrev { 0.0f, 0.0f };

        // The exact keys we poll each frame. Polling only these (rather
        // than 0..GLFW_KEY_LAST) avoids GLFW_INVALID_ENUM on undefined
        // codes and limits events to keys we expose.
        constexpr std::array g_polledKeys = {
            Key::Space,
            Key::D0, Key::D1, Key::D2, Key::D3, Key::D4,
            Key::D5, Key::D6, Key::D7, Key::D8, Key::D9,
            Key::A, Key::B, Key::C, Key::D, Key::E, Key::F, Key::G,
            Key::H, Key::I, Key::J, Key::K, Key::L, Key::M, Key::N,
            Key::O, Key::P, Key::Q, Key::R, Key::S, Key::T, Key::U,
            Key::V, Key::W, Key::X, Key::Y, Key::Z,
            Key::Escape, Key::Enter, Key::Tab, Key::Backspace, Key::Delete,
            Key::Right, Key::Left, Key::Down, Key::Up,
            Key::F1, Key::F2, Key::F3, Key::F4, Key::F5, Key::F6,
            Key::F7, Key::F8, Key::F9, Key::F10, Key::F11, Key::F12,
            Key::LeftShift, Key::LeftControl, Key::LeftAlt,
            Key::RightShift, Key::RightControl, Key::RightAlt,
        };

        // Guarantee our enum values match GLFW's. If GLFW renumbers,
        // these fail to build.
        static_assert(static_cast<int>(Key::Space)     == GLFW_KEY_SPACE);
        static_assert(static_cast<int>(Key::A)         == GLFW_KEY_A);
        static_assert(static_cast<int>(Key::Z)         == GLFW_KEY_Z);
        static_assert(static_cast<int>(Key::D0)        == GLFW_KEY_0);
        static_assert(static_cast<int>(Key::Escape)    == GLFW_KEY_ESCAPE);
        static_assert(static_cast<int>(Key::Up)        == GLFW_KEY_UP);
        static_assert(static_cast<int>(Key::F1)        == GLFW_KEY_F1);
        static_assert(static_cast<int>(Key::LeftShift) == GLFW_KEY_LEFT_SHIFT);
        static_assert(static_cast<int>(MouseButton::Left)   == GLFW_MOUSE_BUTTON_LEFT);
        static_assert(static_cast<int>(MouseButton::Right)  == GLFW_MOUSE_BUTTON_RIGHT);
        static_assert(static_cast<int>(MouseButton::Middle) == GLFW_MOUSE_BUTTON_MIDDLE);
    }

    void Input::init(Window& window, EventBus& bus)
    {
        g_window    = window.nativeHandle();
        g_bus       = &bus;
        g_actionMap = nullptr;

        g_keyCur.fill(false);  g_keyPrev.fill(false);
        g_btnCur.fill(false);  g_btnPrev.fill(false);

        double x = 0.0, y = 0.0;
        if (g_window) glfwGetCursorPos(g_window, &x, &y);
        g_mouseCur = g_mousePrev = { static_cast<float>(x), static_cast<float>(y) };
    }

    void Input::shutdown()
    {
        g_window    = nullptr;
        g_bus       = nullptr;
        g_actionMap = nullptr;
    }

    void Input::setActionMap(ActionMap const* map) { g_actionMap = map; }

    void Input::update()
    {
        if (!g_window)
            return;

        // ── Keys: raw events + action dispatch ──────────────────────
        g_keyPrev = g_keyCur;
        for (Key key : g_polledKeys)
        {
            int const  code = static_cast<int>(key);
            bool const down = glfwGetKey(g_window, code) == GLFW_PRESS;
            g_keyCur[code]  = down;

            bool const pressed  = down && !g_keyPrev[code];
            bool const released = !down && g_keyPrev[code];
            if (!pressed && !released)
                continue;

            if (g_bus)
            {
                if (pressed) g_bus->publish(KeyPressedEvent{ key });
                else         g_bus->publish(KeyReleasedEvent{ key });

                // Action layer: if the active map binds this key, turn
                // the raw transition into a semantic ActionEvent.
                if (g_actionMap)
                    if (std::string const* action = g_actionMap->actionFor(key))
                        g_bus->publish(ActionEvent{ *action, pressed });
            }
        }

        // ── Mouse buttons ───────────────────────────────────────────
        g_btnPrev = g_btnCur;
        for (int b = 0; b <= GLFW_MOUSE_BUTTON_LAST; ++b)
        {
            bool const down = glfwGetMouseButton(g_window, b) == GLFW_PRESS;
            g_btnCur[b]     = down;

            if (g_bus)
            {
                if (down && !g_btnPrev[b])
                    g_bus->publish(MouseButtonPressedEvent{ static_cast<MouseButton>(b) });
                else if (!down && g_btnPrev[b])
                    g_bus->publish(MouseButtonReleasedEvent{ static_cast<MouseButton>(b) });
            }
        }

        // ── Mouse position ──────────────────────────────────────────
        g_mousePrev = g_mouseCur;
        double x = 0.0, y = 0.0;
        glfwGetCursorPos(g_window, &x, &y);
        g_mouseCur = { static_cast<float>(x), static_cast<float>(y) };

        glm::vec2 const delta = g_mouseCur - g_mousePrev;
        if (g_bus && (delta.x != 0.0f || delta.y != 0.0f))
            g_bus->publish(MouseMovedEvent{ g_mouseCur, delta });
    }

    // -- Polling: keys --------------------------------------------------

    bool Input::isKeyDown(Key key) { return g_keyCur[static_cast<int>(key)]; }

    bool Input::wasKeyPressed(Key key)
    {
        int const i = static_cast<int>(key);
        return g_keyCur[i] && !g_keyPrev[i];
    }

    bool Input::wasKeyReleased(Key key)
    {
        int const i = static_cast<int>(key);
        return !g_keyCur[i] && g_keyPrev[i];
    }

    // -- Polling: mouse -------------------------------------------------

    bool Input::isMouseButtonDown(MouseButton button) { return g_btnCur[static_cast<int>(button)]; }

    bool Input::wasMouseButtonPressed(MouseButton button)
    {
        int const i = static_cast<int>(button);
        return g_btnCur[i] && !g_btnPrev[i];
    }

    bool Input::wasMouseButtonReleased(MouseButton button)
    {
        int const i = static_cast<int>(button);
        return !g_btnCur[i] && g_btnPrev[i];
    }

    glm::vec2 Input::mousePosition() { return g_mouseCur; }
    glm::vec2 Input::mouseDelta()    { return g_mouseCur - g_mousePrev; }

} // namespace Core