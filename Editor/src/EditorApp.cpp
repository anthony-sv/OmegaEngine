import Engine.Core;
import std;
#include <Windows.h>

class EditorApp final: public Engine::Core::Application {
public:
    EditorApp()
        : Application { Engine::Core::WindowProps{
            .title = "ΩmegaEngine Editor",
            .width = 1600,
            .height = 900,
            .vsync = true,
            .decorated = false
        } } {}

protected:
    void onInit() override {
        std::println("[Ω::EditorApp] initialised — Ω ready");
    }

    void onShutdown() override {
        std::println("[Ω::EditorApp] shutting down — Ω offline");
    }
}; // class EditorApp

auto main(int argc, char* argv[]) -> int {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    EditorApp app;

    auto result = app.run();

    if(!result) {
        auto const& err = result.error();
        std::println(std::cerr, "[Ω FATAL] Error {}: {}", static_cast<int>(err.code), err.message);
        return 1;
    }

    return result.value();
}