module;

#ifdef _WIN32
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#endif

module Engine.Core:Paths;

import :Paths;
import std;

namespace Engine::Core
{
    std::filesystem::path const& Paths::executableDir()
    {
        static std::filesystem::path const dir = []
        {
#ifdef _WIN32
            std::wstring buf(1024, L'\0');
            auto const n = ::GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
            buf.resize(n);
            return std::filesystem::path { buf }.parent_path();
#else
            return std::filesystem::current_path();
#endif
        }();
        return dir;
    }

    std::optional<std::filesystem::path> Paths::findUpwards(
        std::filesystem::path start, 
        std::filesystem::path const& subpath
    )
    {
        std::error_code ec;
        for (;;)
        {
            if (auto candidate = start / subpath; std::filesystem::exists(candidate, ec))
                return candidate;

            auto parent = start.parent_path();
            if (parent == start)        // reached the filesystem root
                return std::nullopt;
            start = std::move(parent);
        }
    }

    std::filesystem::path Paths::resource(std::filesystem::path const& repoRelative)
    {
        if (auto const found = findUpwards(executableDir(), repoRelative))
            return *found;

        return repoRelative; // last resort: cwd-relative
    }

} // namespace Core