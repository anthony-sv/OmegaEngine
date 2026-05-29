module;

#include "glm/glm.hpp"

export module Engine.Core:Input;

import :Window;
import :EventBus;
import std;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: Input
*   half                    short
*    vec2                  ivec2
*     u32                  i32
*  _________;          ________;
* [RayTracing]       [Rasterizer]
*
════════════════════════════════════════════════════════════════════════════════*/

namespace Engine::Core
{

    // =================================================================
    //
    //  Input -- the platform input DEVICE (a sibling of Window).
    //
    // =================================================================
    //
    // Raw keyboard/mouse, wrapping GLFW. It lives in Core (not a
    // "system") for the same reason Window does: it's a platform device,
    // driven by the platform loop. The Application loop calls
    // init/update/shutdown -- you never do. Query it from anywhere via
    // the static facade: Core::Input::isKeyDown(Key::W).
    //
    // No GLFW callbacks are used (poll-and-diff inside update), so this
    // never fights ImGui's GLFW callbacks. GLFW is included only in the
    // .cpp, so consumers of Engine.Core never see it.
    //
    // =================================================================


    // -- Key / MouseButton codes ----------------------------------------
    // Values deliberately match the GLFW codes (static_assert'd in the
    // .cpp) so they pass straight to glfwGetKey without a lookup table.

    export enum class Key : int
    {
        Space        = 32,

        D0 = 48, D1, D2, D3, D4, D5, D6, D7, D8, D9,   // 48..57

        A = 65, B, C, D, E, F, G, H, I, J, K, L, M,    // 65..77
        N, O, P, Q, R, S, T, U, V, W, X, Y, Z,         // 78..90

        Escape       = 256,
        Enter        = 257,
        Tab          = 258,
        Backspace    = 259,
        Delete       = 261,

        Right        = 262,
        Left         = 263,
        Down         = 264,
        Up           = 265,

        F1 = 290, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,   // 290..301

        LeftShift    = 340,
        LeftControl  = 341,
        LeftAlt      = 342,
        RightShift   = 344,
        RightControl = 345,
        RightAlt     = 346,
    };

    export enum class MouseButton : int
    {
        Left   = 0,
        Right  = 1,
        Middle = 2,
    };


    // -- Raw input events (published on the EventBus by update) ----------

    export struct KeyPressedEvent          { Key key; };
    export struct KeyReleasedEvent         { Key key; };
    export struct MouseMovedEvent          { glm::vec2 position; glm::vec2 delta; };
    export struct MouseButtonPressedEvent  { MouseButton button; };
    export struct MouseButtonReleasedEvent { MouseButton button; };


    // =================================================================
    //
    //  ActionMap -- a rebindable key -> action-name table.
    //
    // =================================================================
    //
    // The semantic layer: "Space means Jump". Bindings are DATA, owned
    // per-World (so each scene has its own) and changeable at runtime
    // (rebinding = editing the map). The active world's map is bound to
    // Input via setActionMap, and Input::update fires an ActionEvent
    // whenever a bound key transitions.
    //
    // =================================================================

    export class ActionMap
    {
    public:
        void bind(Key key, std::string action) { m_bindings[key] = std::move(action); }
        void unbind(Key key)                   { m_bindings.erase(key); }
        void clear()                           { m_bindings.clear(); }

        // The action bound to `key`, or nullptr if unbound.
        [[nodiscard]] std::string const* actionFor(Key key) const
        {
            auto it = m_bindings.find(key);
            return it == m_bindings.end() ? nullptr : &it->second;
        }

        [[nodiscard]] bool empty() const { return m_bindings.empty(); }

    private:
        std::unordered_map<Key, std::string> m_bindings;
    };

    // Fired on the EventBus when a bound key transitions.
    //   started == true  -> the action began (key pressed)
    //   started == false -> the action ended (key released)
    export struct ActionEvent
    {
        std::string name;
        bool        started;
    };


    // =================================================================
    //
    //  Input facade (static class).
    //
    // =================================================================

    export class Input
    {
    public:

        // -- Lifecycle (driven by the Application loop) -----------------
        static void init(Window& window, EventBus& bus);
        static void update();        // once per frame: poll, diff, fire events
        static void shutdown();

        // -- Action binding ---------------------------------------------
        // Point Input at the active world's ActionMap (or nullptr). When
        // set, update() fires ActionEvent for any bound key transition.
        static void setActionMap(ActionMap const* map);

        // -- Polling: keys ----------------------------------------------
        [[nodiscard]] static bool isKeyDown(Key key);
        [[nodiscard]] static bool wasKeyPressed(Key key);
        [[nodiscard]] static bool wasKeyReleased(Key key);

        // -- Polling: mouse ---------------------------------------------
        [[nodiscard]] static bool isMouseButtonDown(MouseButton button);
        [[nodiscard]] static bool wasMouseButtonPressed(MouseButton button);
        [[nodiscard]] static bool wasMouseButtonReleased(MouseButton button);

        [[nodiscard]] static glm::vec2 mousePosition();
        [[nodiscard]] static glm::vec2 mouseDelta();

        Input() = delete;   // static-only

    }; // class Input

} // namespace Core