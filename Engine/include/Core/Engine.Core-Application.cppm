module;

#include "GLFW/glfw3.h"

export module Engine.Core:Application;

import std;

namespace Engine::Core {

    export class Application {
    public:
        std::expected<int, int> run();
    private:
        bool initGraphics();
        void renderLoop();
        void cleanup();

        struct WindowDeleter { void operator()(GLFWwindow* ptr) const; };
        std::unique_ptr<GLFWwindow, WindowDeleter> m_window;
    }; // class Application

} // namespace Core