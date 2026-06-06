module;

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <nethost.h>
#include <hostfxr.h>
#include <coreclr_delegates.h>

module Engine.Scripting:ScriptHost;

import :ScriptHost;
import Engine.ECS;
import std;

namespace Engine::Scripting
{
    namespace
    {
        // Matches C#'s System.Numerics.Vector2 (two floats) -- the blittable
        // shape crossing the boundary.
        struct Vec2 { float x; float y; };

        // The ECS world the script API operates on this frame. Scripts run
        // synchronously on the main thread, so a file-static is safe; the
        // ScriptSystem sets it before each batch of updates.
        ECS::Registry* g_registry = nullptr;

        // ── Engine functions that C# calls back into (the API table) ──
        // Passed to managed as raw function pointers. Entities are raw uint
        // ids; positions cross by pointer to keep the ABI unambiguous.

        void __cdecl Entity_GetPosition(std::uint32_t id, Vec2* out)
        {
            if (!g_registry) { *out = { 0.0f, 0.0f }; return; }
            auto e = g_registry->entityFromId(id);
            if (e.valid() && e.has<ECS::Transform>())
            {
                auto const& p = e.get<ECS::Transform>().position;
                *out = { p.x, p.y };
            }
            else
            {
                *out = { 0.0f, 0.0f };
            }
        }

        void __cdecl Entity_SetPosition(std::uint32_t id, Vec2* in)
        {
            if (!g_registry) return;
            auto e = g_registry->entityFromId(id);
            if (e.valid() && e.has<ECS::Transform>())
            {
                auto& p = e.get<ECS::Transform>().position;
                p.x = in->x;
                p.y = in->y;
            }
        }

        // Mirror of the managed NativeApi struct (sequential layout, cdecl).
        struct NativeApi
        {
            void (__cdecl *GetPosition)(std::uint32_t, Vec2*);
            void (__cdecl *SetPosition)(std::uint32_t, Vec2*);
        };
    }

    // =================================================================
    //  Impl -- the CoreCLR state + resolved managed entry points.
    // =================================================================

    struct ScriptHost::Impl
    {
        bool       ready { false };
        NativeApi  api {};

        // Managed entry points (OmegaEngine.Bootstrap.*).
        void (__cdecl *Init)(void*)                             { nullptr };
        void (__cdecl *Tick)(std::uint32_t, char const*, float) { nullptr };
        void (__cdecl *LoadAssembly)(char const*)              { nullptr };

        load_assembly_and_get_function_pointer_fn load_assembly { nullptr };
    };

    ScriptHost::ScriptHost()  : m_impl { std::make_unique<Impl>() } {}
    ScriptHost::~ScriptHost() = default;

    ScriptHost& ScriptHost::instance()
    {
        static ScriptHost host;
        return host;
    }

    bool ScriptHost::ready() const { return m_impl->ready; }

    void ScriptHost::bindRegistry(ECS::Registry* registry) { g_registry = registry; }

    bool ScriptHost::ensureInitialized(std::filesystem::path const& managedDir)
    {
        if (m_impl->ready)
            return true;

        // 1. nethost -> hostfxr, bind the exports we need.
        wchar_t hostfxrPath[1024];
        std::size_t pathSize = sizeof(hostfxrPath) / sizeof(wchar_t);
        if (int const rc = ::get_hostfxr_path(hostfxrPath, &pathSize, nullptr); rc != 0)
        {
            std::println(std::cerr, "[Ω::Scripting] get_hostfxr_path failed: 0x{:x}", rc);
            return false;
        }

        HMODULE const fxr = ::LoadLibraryW(hostfxrPath);
        if (!fxr)
        {
            std::println(std::cerr, "[Ω::Scripting] LoadLibrary(hostfxr) failed");
            return false;
        }

        auto init_fptr     = reinterpret_cast<hostfxr_initialize_for_runtime_config_fn>(::GetProcAddress(fxr, "hostfxr_initialize_for_runtime_config"));
        auto delegate_fptr = reinterpret_cast<hostfxr_get_runtime_delegate_fn>(::GetProcAddress(fxr, "hostfxr_get_runtime_delegate"));
        auto close_fptr    = reinterpret_cast<hostfxr_close_fn>(::GetProcAddress(fxr, "hostfxr_close"));
        if (!init_fptr || !delegate_fptr || !close_fptr)
        {
            std::println(std::cerr, "[Ω::Scripting] hostfxr exports missing");
            return false;
        }

        // 2. Boot the runtime from OmegaEngine's runtimeconfig.json and grab
        //    the load_assembly_and_get_function_pointer delegate.
        std::wstring const config = (managedDir / "OmegaEngine.runtimeconfig.json").wstring();
        hostfxr_handle ctx = nullptr;
        if (int const rc = init_fptr(config.c_str(), nullptr, &ctx); rc != 0 || ctx == nullptr)
        {
            std::println(std::cerr, "[Ω::Scripting] initialize_for_runtime_config failed: 0x{:x} ('{}')",
                         rc, (managedDir / "OmegaEngine.runtimeconfig.json").string());
            if (ctx) close_fptr(ctx);
            return false;
        }

        void* loadFn = nullptr;
        int const drc = delegate_fptr(ctx, hdt_load_assembly_and_get_function_pointer, &loadFn);
        close_fptr(ctx);
        if (drc != 0 || loadFn == nullptr)
        {
            std::println(std::cerr, "[Ω::Scripting] get_runtime_delegate failed: 0x{:x}", drc);
            return false;
        }
        m_impl->load_assembly = reinterpret_cast<load_assembly_and_get_function_pointer_fn>(loadFn);

        // 3. Resolve the managed [UnmanagedCallersOnly] entry points.
        std::wstring const dll = (managedDir / "OmegaEngine.dll").wstring();
        wchar_t const* const type = L"OmegaEngine.Bootstrap, OmegaEngine";

        auto resolve = [&](wchar_t const* method, void** out) -> bool
        {
            int const rc = m_impl->load_assembly(dll.c_str(), type, method,
                                                 UNMANAGEDCALLERSONLY_METHOD, nullptr, out);
            if (rc != 0 || *out == nullptr)
            {
                std::println(std::cerr, "[Ω::Scripting] could not load managed method (0x{:x})", rc);
                return false;
            }
            return true;
        };

        if (!resolve(L"Init",         reinterpret_cast<void**>(&m_impl->Init))) return false;
        if (!resolve(L"Tick",         reinterpret_cast<void**>(&m_impl->Tick))) return false;
        if (!resolve(L"LoadAssembly", reinterpret_cast<void**>(&m_impl->LoadAssembly))) return false;

        // 4. Hand C# the engine's function table.
        m_impl->api.GetPosition = &Entity_GetPosition;
        m_impl->api.SetPosition = &Entity_SetPosition;
        m_impl->Init(&m_impl->api);
        m_impl->ready = true;
        std::println("[Ω::Scripting] CoreCLR hosted; OmegaEngine.dll loaded from '{}'", managedDir.string());

        // 5. Load the project's game assembly (deployed next to the exe) so
        //    its scripts (e.g. Game.Mover) are discoverable.
        if (auto const gameDll = managedDir / "Game.dll"; std::filesystem::exists(gameDll))
            m_impl->LoadAssembly(gameDll.string().c_str());
        else
            std::println(std::cerr, "[Ω::Scripting] no Game.dll at '{}' (no project scripts)", gameDll.string());

        return true;
    }

    void ScriptHost::tick(std::uint32_t entity, std::string const& className, float dt)
    {
        if (m_impl->ready)
            m_impl->Tick(entity, className.c_str(), dt);
    }

} // namespace Scripting