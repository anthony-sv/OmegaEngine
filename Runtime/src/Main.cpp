#ifdef _WIN32
#   include <Windows.h>
#endif

import Engine.Core;
import Engine.Scene;
import SandboxApp;
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

    // Which project to open: an explicit argv[1] (the launcher will pass one,
    // later), else the bundled sample found by walking up from the exe, else
    // a last-ditch cwd-relative guess.
    std::filesystem::path resolveProjectDir(int argc, char* argv[])
    {
        if (argc > 1)
            return argv[1];

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

    SandboxApp app { std::move(*project) };

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