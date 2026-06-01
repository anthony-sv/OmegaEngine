module;

#include "glad/glad.h"

// stb_image_write uses sprintf/fopen internally; silence the MSVC SDL
// "deprecated CRT" error (C4996) just for this third-party header.
#pragma warning(push)
#pragma warning(disable: 4996)
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#pragma warning(pop)

module Engine.Renderer:Screenshot;

import :Screenshot;
import Engine.Core;
import std;

namespace Engine::Renderer
{
    using Core::ErrorInfo;
    using Core::ErrorCode;

    Core::VoidResult Screenshot::capture(
        std::filesystem::path const& path, 
        int x, int y, 
        int width, int height
    )
    {
        if (width <= 0 || height <= 0)
            return std::unexpected(
                ErrorInfo::make(
                    ErrorCode::FileReadFailed,
                    std::format("invalid screenshot size {}x{}", width, height)
                )
            );

        // Read RGBA8 from the bound framebuffer. PACK_ALIGNMENT=1 so rows
        // aren't padded (width isn't necessarily a multiple of 4).
        std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height * 4);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

        std::error_code ec;
        if (path.has_parent_path())
            std::filesystem::create_directories(path.parent_path(), ec);

        // GL is bottom-up; PNG is top-down.
        stbi_flip_vertically_on_write(true);

        int const ok = stbi_write_png(
            path.string().c_str(), 
            width, height, 
            4, 
            pixels.data(), 
            width * 4
        );

        if (!ok)
            return std::unexpected(
                ErrorInfo::make(
                    ErrorCode::FileReadFailed,
                    std::format("failed to write screenshot '{}'", path.string())
                )
            );

        std::println("[Ω::Screenshot] saved {}x{} -> '{}'", width, height, path.string());
        return {};
    }

    std::filesystem::path Screenshot::timestamped(std::string_view dir, std::string_view prefix)
    {
        // Decompose the current UTC time into calendar fields, then format
        // with plain INTEGER specifiers -- avoids the locale time_put facet
        // that std::format's "{:%Y...}" chrono path would pull in.
        auto const now  = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
        auto const days = std::chrono::floor<std::chrono::days>(now);

        std::chrono::year_month_day const ymd { days };
        std::chrono::hh_mm_ss<std::chrono::seconds> const hms { now - days };

        auto const file = std::format(
            "{}_{:04}{:02}{:02}_{:02}{:02}{:02}.png",
            prefix,
            static_cast<int>(ymd.year()),
            static_cast<unsigned>(ymd.month()),
            static_cast<unsigned>(ymd.day()),
            hms.hours().count(),
            hms.minutes().count(),
            hms.seconds().count());

        return std::filesystem::path { dir } / file;
    }

} // namespace Renderer