export module Engine.Core:Error;

import std;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: Error
*   half                    short
*    vec2                  ivec2
*     u32                  i32
*  _________;          ________;
* [RayTracing]       [Rasterizer]
*
════════════════════════════════════════════════════════════════════════════════*/

namespace Engine::Core {

	// Ω::ErrorCode ───────────────────────────────────────────────────
	export enum class ErrorCode: std::uint16_t {
		Unknown = 0,
		GlfwInitFailed = 100,
		WindowCreationFailed = 101,
		GladLoadFailed = 102,
	};

	// Ω::ErrorInfo ───────────────────────────────────────────────────
	export struct ErrorInfo {
		ErrorCode   code;
		std::string message;

		static ErrorInfo make(ErrorCode code, std::string msg) {
			return { code, std::move(msg) };
		}
	};

	// Ω::Result ──────────────────────────────────────────────────────
	template<typename T>
	using Result = std::expected<T, ErrorInfo>;

	// Ω::VoidResult ──────────────────────────────────────────────────
	using VoidResult = Result<std::monostate>;
} // namespace Core