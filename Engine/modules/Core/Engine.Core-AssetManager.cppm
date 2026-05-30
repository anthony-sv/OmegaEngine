export module Engine.Core:AssetManager;

import :Error;
import std;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: AssetManager
*   half                    short
*    vec2                  ivec2
*     u32                  i32
*  _________;          ________;
* [RayTracing]       [Rasterizer]
*
════════════════════════════════════════════════════════════════════════════════*/

namespace Engine::Core
{

    // =================================================================
    //
    //  AssetLoader<T> -- how to load an asset of type T from arguments.
    //
    // =================================================================
    //
    // Different asset types have different loaders (Texture2D::create vs
    // Shader::fromFiles). This trait decouples the AssetManager from
    // those signatures:
    //
    //   - DEFAULT: assume `T::create(args...) -> Result<T>`. Texture2D
    //     matches this, so it needs no specialization.
    //   - SPECIALIZE for types whose loader differs. Each owning module
    //     adds its own specialization (e.g. Engine.Renderer specializes
    //     AssetLoader<Shader> to call Shader::fromFiles). A type with
    //     neither create() nor a specialization fails to compile -- which
    //     is the right nudge to provide a loader.
    //
    // =================================================================

    export template <typename T>
    struct AssetLoader
    {
        template <typename... Args>
        [[nodiscard]] static Result<T> load(Args&&... args)
        {
            return T::create(std::forward<Args>(args)...);
        }
	}; // struct AssetLoader


    // =================================================================
    //
    //  AssetManager -- loads, owns and de-duplicates assets by key.
    //
    // =================================================================
    //
    // An app-wide service (Application owns one; reach it via
    // Application::get().assets()). It is the CACHE + OWNERSHIP layer;
    // the actual loading is delegated to AssetLoader<T> (i.e. each type's
    // own factory). Assets are loaded ON DEMAND -- the first request for
    // a key loads it; later requests for the same key return the cached
    // instance. The manager owns every asset on the heap, so the pointers
    // it hands back stay valid for as long as the manager lives (the app
    // lifetime) -- which is exactly what a SpriteRenderer's Texture2D*
    // needs.
    //
    // IDENTITY: keyed by string (the asset PATH for single-file assets).
    // The path is a stable, serialization-ready id -- a saved scene just
    // stores "textures/Sheet.png". (A UUID + .meta system can replace the
    // key later, behind this same interface, without touching callers.)
    //
    // =================================================================

    export class AssetManager
    {
    public:

        // Single-file asset: the path IS the cache key. The common case.
        //   auto* tex = assets.load<Texture2D>("textures/Sheet.png");
        // Returns a stable pointer owned by the manager, or nullptr on
        // failure (logged).
        template <typename T>
        [[nodiscard]] T* load(std::filesystem::path const& path)
        {
            return loadKeyed<T>(path.string(), path);
        }

        // Explicit key + loader arguments -- for assets whose loader takes
        // something other than a single path (e.g. a Shader from two files):
        //   auto* sh = assets.loadKeyed<Shader>("batch", vertPath, fragPath);
        template <typename T, typename... Args>
        [[nodiscard]] T* loadKeyed(std::string const& key, Args&&... args)
        {
            auto const full = makeKey<T>(key);

            if (auto it = m_cache.find(full); it != m_cache.end())
                return static_cast<T*>(it->second.get());

            Result<T> result = AssetLoader<T>::load(std::forward<Args>(args)...);
            if (!result)
            {
                std::println(std::cerr,
                             "[Ω::AssetManager] load failed for '{}': {}",
                             key, result.error().message);
                return nullptr;
            }

            // shared_ptr<void> type-erases the deleter so we can store any
            // asset type in one map; static_cast back is safe because the
            // key encodes the type.
            auto ptr = std::make_shared<T>(std::move(*result));
            T*   raw = ptr.get();
            m_cache.emplace(full, std::move(ptr));
            return raw;
        }

        // Already cached?
        template <typename T>
        [[nodiscard]] bool contains(std::string const& key) const
        {
            return m_cache.contains(makeKey<T>(key));
        }

        // Drop every asset. Pointers handed out before this become
        // dangling -- only call when nothing references them.
        void clear() { m_cache.clear(); }

        [[nodiscard]] std::size_t count() const { return m_cache.size(); }


    private:

        // Per-type namespacing so the same path under different types
        // (or just different types) never collide.
        template <typename T>
        [[nodiscard]] static std::string makeKey(std::string const& key)
        {
            return std::format("{}|{}", typeid(T).name(), key);
        }

        std::unordered_map<std::string, std::shared_ptr<void>> m_cache;

    }; // class AssetManager

} // namespace Core