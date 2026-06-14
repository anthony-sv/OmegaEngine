#ifdef _WIN32
#   include <Windows.h>
#endif

import Engine.Core;
import Engine.Scene;
import EditorApp;
import std;

namespace
{
    // Which project to open. argv[1] accepts a BARE NAME ("Moto"), a relative
    // path ("projects/Moto") or an absolute one. No argument = the bundled sample.
    std::filesystem::path resolveProjectDir(int argc, char* argv[])
    {
        using Engine::Core::Paths;

        if (argc > 1)
        {
            std::filesystem::path const arg { argv[1] };
            if (std::filesystem::exists(arg / "project.json"))
                return arg;
            if (auto found = Paths::findUpwards(Paths::executableDir(), "projects" / arg))
                return *found;
            if (auto found = Paths::findUpwards(Paths::executableDir(), arg))
                return *found;
            return arg;   // let Project::load report the failure
        }

        return Paths::findUpwards(Paths::executableDir(), "projects/Sandbox")
            .value_or("projects/Sandbox");
    }
}

// Ω::main ─────────────────────────────────────────────────────────────────────
//
// The editor is generic: it OPENS a project folder and edits it -- the SAME
// project the runtime plays. The project is located by name or path (see
// resolveProjectDir); argv[1] overrides the bundled sample (the launcher will
// pass one). Once loaded, the working directory is set to the project root so
// scenes/ + assets/ resolve into it.
auto main(int argc, char* argv[]) -> int {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    std::filesystem::path const projectDir = resolveProjectDir(argc, argv);

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

    Editor::EditorApp app { std::move(*project) };

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