#ifdef _WIN32
#   include <Windows.h>
#endif

import Engine.Core;
import Engine.Scene;
import EditorApp;
import std;

// Ω::main ─────────────────────────────────────────────────────────────────────
//
// The editor is generic: it OPENS a project folder and edits it -- the SAME
// project the runtime plays. The project is located relative to the
// executable (so the editor finds it no matter the working directory);
// argv[1] overrides (the launcher will pass one). Once loaded, the working
// directory is set to the project root so scenes/ + assets/ resolve into it.
auto main(int argc, char* argv[]) -> int {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    std::filesystem::path const projectDir = (argc > 1)
        ? std::filesystem::path { argv[1] }
        : Engine::Core::Paths::findUpwards(Engine::Core::Paths::executableDir(), "projects/Sandbox")
              .value_or("projects/Sandbox");

    auto project = Engine::Scene::Project::load(projectDir);
    if(!project) {
        std::println(std::cerr, "[Ω FATAL] cannot open project '{}': {}",
                     projectDir.string(), project.error().message);
        return 1;
    }

    // Content root: scenes/ + assets/ now resolve into the project. Engine
    // and editor resources are exe-relative (Core::Paths), so they are still
    // found.
    std::filesystem::current_path(project->root());

    EditorApp app { std::move(*project) };

    auto result = app.run();
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