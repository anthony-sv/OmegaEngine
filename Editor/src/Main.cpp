import std;
import Engine.Core;
#include <Windows.h>
import EditorApp;

auto main(int argc, char* argv[]) -> int {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    EditorApp app;
    auto result = app.run();

    if(!result) {
        auto const& err = result.error();
        std::println(std::cerr, "[Ω FATAL] Error {}: {}",
                     static_cast<int>(err.code), err.message);
        return 1;
    }
    return result.value();
}