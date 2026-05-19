#ifdef _WIN32
#   include <Windows.h>
#endif

#include "Core/EntryPoint.hpp"

import Engine.Core;
import EditorApp;

// Ω::createApplication ────────────────────────────────────────────────────────
std::unique_ptr<Engine::Core::Application> createApplication() {
    return std::make_unique<EditorApp>();
}

// Ω::main ─────────────────────────────────────────────────────────────────────
auto main(int argc, char* argv[]) -> int {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    auto app = createApplication();

    auto result = app->run();

    if(!result) {
        auto const& err = result.error();

        std::println(std::cerr,
                     "[Ω FATAL] Error {}: {}",
                     static_cast<int>(err.code),
                     err.message);
        return 1;
    }

    return result.value();
}
