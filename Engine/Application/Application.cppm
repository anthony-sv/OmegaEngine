export module Application;

import std;

export class Application {
public:
    std::expected<int, int> run();
private:
    bool initGraphics();
    void renderLoop();
    void cleanup();

    struct WindowDeleter { void operator()(struct GLFWwindow* ptr) const; };
    std::unique_ptr<struct GLFWwindow, WindowDeleter> m_window;
};