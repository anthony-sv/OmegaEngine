module;

#include "nlohmann/json.hpp"

module Engine.Scene:Project;

import :Project;
import Engine.Core;
import std;

namespace Engine::Scene
{
    using json = nlohmann::json;
    using Core::ErrorInfo;
    using Core::ErrorCode;

    Core::Result<Project> Project::load(std::filesystem::path const& projectDir)
    {
        auto const file = projectDir / "project.json";

        std::ifstream in { file };
        if (!in)
            return std::unexpected(ErrorInfo::make(
                ErrorCode::FileReadFailed,
                std::format("cannot open project file '{}'", file.string())));

        // .at()/get<> throw on malformed data -- one try keeps a bad
        // manifest from crashing the launcher.
        try
        {
            json root;
            in >> root;

            Project p;
            // Absolute so root() is unambiguous no matter the cwd at
            // load time (callers set the working dir to it afterward).
            p.m_root         = std::filesystem::absolute(projectDir);
            p.m_name         = root.value("name", std::string { "Untitled" });
            p.m_startupScene = root.value("startupScene", std::string {});

            for (auto const& s : root.value("scenes", json::array()))
                p.m_scenes.push_back(s.get<std::string>());

            // Window config: every field optional, defaulting to the
            // WindowProps defaults so a minimal manifest still works.
            Core::WindowProps w;
            if (root.contains("window"))
            {
                auto const& jw = root["window"];
                w.title     = jw.value("title",     w.title);
                w.width     = jw.value("width",     w.width);
                w.height    = jw.value("height",    w.height);
                w.vsync     = jw.value("vsync",     w.vsync);
                w.decorated = jw.value("decorated", w.decorated);
            }
            p.m_window = std::move(w);

            // Default the startup scene to the first listed if unset.
            if (p.m_startupScene.empty() && !p.m_scenes.empty())
                p.m_startupScene = p.m_scenes.front();

            std::println("[Ω::Project] loaded '{}' ({} scene(s), startup '{}') <- '{}'",
                         p.m_name, p.m_scenes.size(), p.m_startupScene, file.string());
            return p;
        }
        catch (std::exception const& ex)
        {
            return std::unexpected(ErrorInfo::make(
                ErrorCode::FileReadFailed,
                std::format("error parsing '{}': {}", file.string(), ex.what())));
        }
    }

} // namespace Scene