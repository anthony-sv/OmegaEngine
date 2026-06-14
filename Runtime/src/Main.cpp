#ifdef _WIN32
#   include <Windows.h>
#endif

import Engine.Core;
import Engine.Scene;
import RuntimeApp;
import std;

namespace
{
    // Absolute path to the running executable's directory. The default
    // project is located relative to THIS, not the working directory, so
    // the runtime finds its content no matter where it is launched from.
    std::filesystem::path executableDir()
    {
#ifdef _WIN32
        std::wstring buf(1024, L'\0');
        auto const n = ::GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
        buf.resize(n);
        return std::filesystem::path { buf }.parent_path();
#else
        return std::filesystem::current_path();
#endif
    }

    // Walk `start` and its ancestors looking for `subpath`; return the first
    // existing match (e.g. find "projects/Sandbox" from x64/Debug up to the
    // repo root). Returns nullopt if it reaches the filesystem root.
    std::optional<std::filesystem::path> findUpwards(
        std::filesystem::path start, std::filesystem::path const& subpath)
    {
        std::error_code ec;
        for (;;)
        {
            if (auto candidate = start / subpath; std::filesystem::exists(candidate, ec))
                return candidate;

            auto parent = start.parent_path();
            if (parent == start)   // reached the root, give up
                return std::nullopt;
            start = std::move(parent);
        }
    }

    // Which project to open. argv[1] accepts a BARE NAME ("Moto"), a relative
    // path ("projects/Moto") or an absolute one -- tried as a literal first,
    // then as projects/<name> walking up from the exe (so it works no matter
    // the working directory). No argument = the bundled sample.
    std::filesystem::path resolveProjectDir(int argc, char* argv[])
    {
        if (argc > 1)
        {
            std::filesystem::path const arg { argv[1] };
            if (std::filesystem::exists(arg / "project.json"))
                return arg;
            if (auto const found = findUpwards(executableDir(), std::filesystem::path { "projects" } / arg))
                return *found;
            if (auto const found = findUpwards(executableDir(), arg))
                return *found;
            return arg;   // let Project::load report the failure
        }

        if (auto const found = findUpwards(executableDir(), "projects/Sandbox"))
            return *found;

        return "projects/Sandbox";
    }
}

// Ω::main ─────────────────────────────────────────────────────────────────────
//
// The runtime is generic: it OPENS a project folder and plays it. Once the
// project is loaded, the working directory is set to its root so every
// relative path (scenes/, assets/) resolves INTO the project.
auto main(int argc, char* argv[]) -> int {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    auto const projectDir = resolveProjectDir(argc, argv);

    auto project = Engine::Scene::Project::load(projectDir);
    if(!project) {
        std::println(std::cerr, "[Ω FATAL] cannot open project '{}': {}",
                     projectDir.string(), project.error().message);
        return 1;
    }

    // Content root: from here on, relative paths resolve into the project.
    std::filesystem::current_path(project->root());

    Runtime::RuntimeApp app { std::move(*project) };

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