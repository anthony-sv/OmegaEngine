module;

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <nethost.h>
#include <hostfxr.h>
#include <coreclr_delegates.h>

module Engine.Scripting:ScriptHost;

import :ScriptHost;
import Engine.Core;        // Application (assets), Input
import Engine.ECS;
import Engine.Renderer;    // Texture2D (for sprite SetTexture)
import Engine.Physics;     // PhysicsWorld (body velocity / impulse / force)
import std;

namespace Engine::Scripting
{
    namespace
    {
        // Blittable shapes crossing the boundary -- match C#'s
        // System.Numerics.Vector2 / Vector4 (tightly packed floats).
        struct Vec2 { float x; float y; };
        struct Vec4 { float x; float y; float z; float w; };

        // ── Per-frame context the script API operates on ──
        // Scripts run synchronously on the main thread, so file-statics are
        // safe; the ScriptSystem refreshes them before each batch of updates.
        ECS::Registry*          g_registry = nullptr;   // the world being ticked
        Physics::PhysicsWorld*  g_physics  = nullptr;   // its bodies (may be null)
        float                   g_delta    = 0.0f;       // this frame's dt
        float                   g_elapsed  = 0.0f;       // seconds since the host booted

        // Resolve an entity that is alive AND carries T; nullptr otherwise.
        template <typename T>
        T* component(std::uint32_t id)
        {
            if (!g_registry)
                return nullptr;
            auto e = g_registry->entityFromId(id);
            return (e.valid() && e.has<T>()) ? &e.get<T>() : nullptr;
        }

        // ═══════════════════════════════════════════════════════════════
        //  The API table -- engine functions C# calls back into. Entities
        //  are raw uint ids; vectors cross by pointer to keep the ABI
        //  unambiguous; booleans cross as int (0/1).
        // ═══════════════════════════════════════════════════════════════

        // -- Transform --------------------------------------------------

        void __cdecl Entity_GetPosition(std::uint32_t id, Vec2* out)
        {
            auto* t = component<ECS::Transform>(id);
            *out = t ? Vec2 { t->position.x, t->position.y } : Vec2 { 0.0f, 0.0f };
        }

        void __cdecl Entity_SetPosition(std::uint32_t id, Vec2* in)
        {
            if (auto* t = component<ECS::Transform>(id))
                t->position = { in->x, in->y };
        }

        float __cdecl Entity_GetRotation(std::uint32_t id)
        {
            auto* t = component<ECS::Transform>(id);
            return t ? t->rotation : 0.0f;
        }

        void __cdecl Entity_SetRotation(std::uint32_t id, float degrees)
        {
            if (auto* t = component<ECS::Transform>(id))
                t->rotation = degrees;
        }

        void __cdecl Entity_GetScale(std::uint32_t id, Vec2* out)
        {
            auto* t = component<ECS::Transform>(id);
            *out = t ? Vec2 { t->scale.x, t->scale.y } : Vec2 { 1.0f, 1.0f };
        }

        void __cdecl Entity_SetScale(std::uint32_t id, Vec2* in)
        {
            if (auto* t = component<ECS::Transform>(id))
                t->scale = { in->x, in->y };
        }

        // -- Physics body (Dynamic bodies: the sim owns the Transform, so
        //    scripts steer them through velocity / impulse / force) -------

        void __cdecl Body_GetVelocity(std::uint32_t id, Vec2* out)
        {
            if (g_physics)
            {
                auto const v = g_physics->getLinearVelocity(id);
                *out = { v.x, v.y };
            }
            else
            {
                *out = { 0.0f, 0.0f };
            }
        }

        void __cdecl Body_SetVelocity(std::uint32_t id, Vec2* in)
        {
            if (g_physics)
                g_physics->setLinearVelocity(id, { in->x, in->y });
        }

        void __cdecl Body_ApplyImpulse(std::uint32_t id, Vec2* in)
        {
            if (g_physics)
                g_physics->applyLinearImpulse(id, { in->x, in->y });
        }

        void __cdecl Body_ApplyForce(std::uint32_t id, Vec2* in)
        {
            if (g_physics)
                g_physics->applyForce(id, { in->x, in->y });
        }

        // -- Sprite -----------------------------------------------------

        void __cdecl Sprite_GetColor(std::uint32_t id, Vec4* out)
        {
            auto* s = component<ECS::SpriteRenderer>(id);
            *out = s ? Vec4 { s->color.r, s->color.g, s->color.b, s->color.a }
                     : Vec4 { 1.0f, 1.0f, 1.0f, 1.0f };
        }

        void __cdecl Sprite_SetColor(std::uint32_t id, Vec4* in)
        {
            if (auto* s = component<ECS::SpriteRenderer>(id))
                s->color = { in->x, in->y, in->z, in->w };
        }

        void __cdecl Sprite_SetTexture(std::uint32_t id, char const* path)
        {
            if (!g_registry || !path)
                return;
            auto e = g_registry->entityFromId(id);
            if (!e.valid())
                return;

            auto& sprite = e.has<ECS::SpriteRenderer>()
                         ? e.get<ECS::SpriteRenderer>()
                         : e.add<ECS::SpriteRenderer>();

            sprite.texturePath = path;
            sprite.texture     = Core::Application::get().assets().load<Renderer::Texture2D>(path);
            sprite.uvMin       = { 0.0f, 0.0f };   // a whole new image -> full UVs
            sprite.uvMax       = { 1.0f, 1.0f };
        }

        // Mirror horizontally via the sign of the X scale -- robust against
        // the animation systems (which rewrite UVs, not scale).
        void __cdecl Sprite_SetFlipX(std::uint32_t id, int flip)
        {
            if (auto* t = component<ECS::Transform>(id))
            {
                float const mag = std::abs(t->scale.x);
                t->scale.x = flip ? -mag : mag;
            }
        }

        // -- Input (global facade) --------------------------------------

        int __cdecl Input_IsKeyDown(int key)
        {
            return Core::Input::isKeyDown(static_cast<Core::Key>(key)) ? 1 : 0;
        }

        int __cdecl Input_WasKeyPressed(int key)
        {
            return Core::Input::wasKeyPressed(static_cast<Core::Key>(key)) ? 1 : 0;
        }

        int __cdecl Input_IsMouseDown(int button)
        {
            return Core::Input::isMouseButtonDown(static_cast<Core::MouseButton>(button)) ? 1 : 0;
        }

        void __cdecl Input_MousePosition(Vec2* out)
        {
            auto const p = Core::Input::mousePosition();
            *out = { p.x, p.y };
        }

        // -- Time -------------------------------------------------------

        float __cdecl Time_Delta()   { return g_delta;   }
        float __cdecl Time_Elapsed() { return g_elapsed; }

        // -- Entity lifecycle / lookup ----------------------------------

        std::uint32_t __cdecl Entity_Create(char const* name)
        {
            if (!g_registry)
                return 0xFFFF'FFFFu;
            auto e = g_registry->create(name ? name : "Entity");
            e.add<ECS::Transform>();
            return static_cast<std::uint32_t>(e.id());
        }

        void __cdecl Entity_Destroy(std::uint32_t id)
        {
            if (!g_registry)
                return;
            if (auto e = g_registry->entityFromId(id); e.valid())
                g_registry->destroy(e);
        }

        std::uint32_t __cdecl Entity_Find(char const* name)
        {
            std::uint32_t found = 0xFFFF'FFFFu;
            if (g_registry && name)
            {
                std::string_view const target { name };
                g_registry->eachEntity([&](ECS::Entity e)
                {
                    if (found == 0xFFFF'FFFFu && e.has<ECS::NameComponent>()
                        && e.get<ECS::NameComponent>().name == target)
                        found = static_cast<std::uint32_t>(e.id());
                });
            }
            return found;
        }

        int __cdecl Entity_IsValid(std::uint32_t id)
        {
            return (g_registry && g_registry->entityFromId(id).valid()) ? 1 : 0;
        }

        // Mirror of the managed NativeApi struct -- SAME ORDER, cdecl. The
        // managed side reads this struct by pointer, so the field layout is
        // a hard contract: keep both lists in lockstep.
        struct NativeApi
        {
            void  (__cdecl *GetPosition)  (std::uint32_t, Vec2*);
            void  (__cdecl *SetPosition)  (std::uint32_t, Vec2*);
            float (__cdecl *GetRotation)  (std::uint32_t);
            void  (__cdecl *SetRotation)  (std::uint32_t, float);
            void  (__cdecl *GetScale)     (std::uint32_t, Vec2*);
            void  (__cdecl *SetScale)     (std::uint32_t, Vec2*);

            void  (__cdecl *GetVelocity)  (std::uint32_t, Vec2*);
            void  (__cdecl *SetVelocity)  (std::uint32_t, Vec2*);
            void  (__cdecl *ApplyImpulse) (std::uint32_t, Vec2*);
            void  (__cdecl *ApplyForce)   (std::uint32_t, Vec2*);

            void  (__cdecl *GetColor)     (std::uint32_t, Vec4*);
            void  (__cdecl *SetColor)     (std::uint32_t, Vec4*);
            void  (__cdecl *SetTexture)   (std::uint32_t, char const*);
            void  (__cdecl *SetFlipX)     (std::uint32_t, int);

            int   (__cdecl *IsKeyDown)    (int);
            int   (__cdecl *WasKeyPressed)(int);
            int   (__cdecl *IsMouseDown)  (int);
            void  (__cdecl *MousePosition)(Vec2*);

            float (__cdecl *TimeDelta)    ();
            float (__cdecl *TimeElapsed)  ();

            std::uint32_t (__cdecl *Create) (char const*);
            void          (__cdecl *Destroy)(std::uint32_t);
            std::uint32_t (__cdecl *Find)   (char const*);
            int           (__cdecl *IsValid)(std::uint32_t);
        };
    }

    // =================================================================
    //  Impl -- the CoreCLR state + resolved managed entry points.
    // =================================================================

    struct ScriptHost::Impl
    {
        bool       ready { false };
        NativeApi  api {};

        // The world bound last frame. When it changes (a scene switch) the
        // managed instance table is cleared so recycled entity ids don't
        // inherit stale scripts.
        ECS::Registry* lastRegistry { nullptr };

        // Managed entry points (OmegaEngine.Bootstrap.*).
        void (__cdecl *Init)(void*)                                       { nullptr };
        void (__cdecl *Tick)(std::uint32_t, char const*, float)           { nullptr };
        void (__cdecl *LoadAssembly)(char const*)                         { nullptr };
        void (__cdecl *BeginFrame)()                                      { nullptr };
        void (__cdecl *Clear)()                                           { nullptr };
        void (__cdecl *OnPhysicsEvent)(std::uint32_t, std::uint32_t, std::uint8_t) { nullptr };

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

    void ScriptHost::bindRegistry(ECS::Registry* registry)
    {
        g_registry = registry;

        // A different world this frame -> drop the previous world's script
        // instances (their entity ids belong to a registry that's gone).
        if (registry != m_impl->lastRegistry)
        {
            if (m_impl->ready && m_impl->Clear)
                m_impl->Clear();
            m_impl->lastRegistry = registry;
        }
    }

    void ScriptHost::bindPhysics(Physics::PhysicsWorld* physics) { g_physics = physics; }

    void ScriptHost::setTime(float dt)
    {
        g_delta    = dt;
        g_elapsed += dt;
    }

    void ScriptHost::beginFrame()
    {
        if (m_impl->ready && m_impl->BeginFrame)
            m_impl->BeginFrame();
    }

    void ScriptHost::dispatchPhysicsEvent(std::uint32_t a, std::uint32_t b, PhysicsEventKind kind)
    {
        if (m_impl->ready && m_impl->OnPhysicsEvent)
            m_impl->OnPhysicsEvent(a, b, static_cast<std::uint8_t>(kind));   // raw byte at the ABI
    }

    void ScriptHost::loadGame(std::filesystem::path const& gameAssembly)
    {
        if (!m_impl->ready || !m_impl->LoadAssembly)
            return;

        auto const abs = std::filesystem::absolute(gameAssembly);
        if (!std::filesystem::exists(abs))
        {
            std::println(
                std::cerr,
                "[Ω::Scripting] no game assembly at '{}' (no project scripts)",
                abs.string()
            );
            return;
        }
        m_impl->LoadAssembly(abs.string().c_str());
    }

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

        if (!resolve(L"Init",           reinterpret_cast<void**>(&m_impl->Init)))           return false;
        if (!resolve(L"Tick",           reinterpret_cast<void**>(&m_impl->Tick)))           return false;
        if (!resolve(L"LoadAssembly",   reinterpret_cast<void**>(&m_impl->LoadAssembly)))   return false;
        if (!resolve(L"BeginFrame",     reinterpret_cast<void**>(&m_impl->BeginFrame)))     return false;
        if (!resolve(L"Clear",          reinterpret_cast<void**>(&m_impl->Clear)))          return false;
        if (!resolve(L"OnPhysicsEvent", reinterpret_cast<void**>(&m_impl->OnPhysicsEvent))) return false;

        // 4. Hand C# the engine's function table (order MUST match the
        //    managed NativeApi struct).
        m_impl->api = {
            .GetPosition   = &Entity_GetPosition,
            .SetPosition   = &Entity_SetPosition,
            .GetRotation   = &Entity_GetRotation,
            .SetRotation   = &Entity_SetRotation,
            .GetScale      = &Entity_GetScale,
            .SetScale      = &Entity_SetScale,
            .GetVelocity   = &Body_GetVelocity,
            .SetVelocity   = &Body_SetVelocity,
            .ApplyImpulse  = &Body_ApplyImpulse,
            .ApplyForce    = &Body_ApplyForce,
            .GetColor      = &Sprite_GetColor,
            .SetColor      = &Sprite_SetColor,
            .SetTexture    = &Sprite_SetTexture,
            .SetFlipX      = &Sprite_SetFlipX,
            .IsKeyDown     = &Input_IsKeyDown,
            .WasKeyPressed = &Input_WasKeyPressed,
            .IsMouseDown   = &Input_IsMouseDown,
            .MousePosition = &Input_MousePosition,
            .TimeDelta     = &Time_Delta,
            .TimeElapsed   = &Time_Elapsed,
            .Create        = &Entity_Create,
            .Destroy       = &Entity_Destroy,
            .Find          = &Entity_Find,
            .IsValid       = &Entity_IsValid,
        };
        m_impl->Init(&m_impl->api);
        m_impl->ready = true;
        std::println("[Ω::Scripting] CoreCLR hosted; OmegaEngine.dll loaded from '{}'", managedDir.string());

        // The project's game assembly is loaded separately (loadGame), from the
        // project's build output, so it can be watched and hot-reloaded.
        return true;
    }

    void ScriptHost::tick(std::uint32_t entity, std::string const& className, float dt)
    {
        if (m_impl->ready)
            m_impl->Tick(entity, className.c_str(), dt);
    }

} // namespace Scripting