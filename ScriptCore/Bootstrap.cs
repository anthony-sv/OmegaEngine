using System.Globalization;
using System.Numerics;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Runtime.Loader;

namespace OmegaEngine;

// The native ScriptHost calls these [UnmanagedCallersOnly] entry points.
// Instances are keyed by entity id, so the native ScriptComponent carries no
// runtime handle -- it stays pure (serializable) data.
//
// Hot reload: the project's Game.dll lives in a COLLECTIBLE load context that
// we can unload and rebuild without restarting the engine. OmegaEngine.dll
// (this assembly) stays in the parent context, so the `Script` base type keeps
// ONE identity across reloads and `instance is Script` never breaks.
public static class Bootstrap
{
    // Live script instances, keyed by entity id. Created lazily on first Tick;
    // recreated when the bound class name changes or its assembly is unloaded.
    private static readonly Dictionary<uint, Script?> Scripts = [];

    // The collectible context the project's game assembly lives in. Swapped
    // wholesale on hot reload.
    private static GameLoadContext? _game;

    // Absolute path of the game assembly we load and watch.
    private static string? _gamePath;
    private static FileSystemWatcher? _watcher;

    // Raised by the watcher (a thread-pool thread); consumed on the main thread
    // in BeginFrame so a reload never races a Tick.
    private static volatile bool _reloadPending;

    // ── Init ────────────────────────────────────────────────────────────────

    // Called once with a pointer to the engine's function table.
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe void Init(nint apiPtr)
    {
        Interop.Api = *(NativeApi*)apiPtr;
        Console.WriteLine($"[C#] OmegaEngine scripting initialized (.NET {Environment.Version})");
    }

    // ── Game assembly load + watch ──────────────────────────────────────────

    // Load the project's game assembly (e.g. Game.dll) into a fresh collectible
    // context and start watching it for rebuilds. Idempotent for a given path.
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    public static void LoadAssembly(nint pathUtf8)
    {
        if (Marshal.PtrToStringUTF8(pathUtf8) is not { Length: > 0 } raw)
            return;

        var path = Path.GetFullPath(raw);
        if (_game is not null && string.Equals(path, _gamePath, StringComparison.OrdinalIgnoreCase))
            return;     // already loaded + watched

        _gamePath = path;
        LoadGame();
        StartWatching();
    }

    // ── Per-frame + lifecycle entry points ──────────────────────────────────

    // Called once per update batch, before ticking, on the main thread. The
    // only place a pending hot reload is actually applied.
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    public static void BeginFrame()
    {
        if (_reloadPending)
        {
            _reloadPending = false;
            Reload();
        }
    }

    // Drop all live instances. The native side calls this when the world
    // changes (scene switch) so recycled entity ids don't inherit stale
    // scripts from the previous scene.
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    public static void Clear() => Scripts.Clear();

    // Hand the native sink the full name of every concrete Script subclass in
    // the loaded game assembly. Powers the editor's class picker.
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe void EnumScriptClasses(delegate* unmanaged[Cdecl]<nint, void> sink)
    {
        if (_game is null)
            return;

        foreach (var assembly in _game.Assemblies)
        {
            Type[] types;
            try { types = assembly.GetTypes(); }
            catch { continue; }     // skip an assembly whose types won't all load

            foreach (var type in types)
            {
                if (!type.IsClass || type.IsAbstract
                    || !type.IsAssignableTo(typeof(Script)) || type.FullName is not { } name)
                    continue;

                nint utf8 = Marshal.StringToCoTaskMemUTF8(name);
                try { sink(utf8); }
                finally { Marshal.FreeCoTaskMem(utf8); }
            }
        }
    }

    // Describe a class's editable fields to the native sink as
    // (name, type tag, default value string). Powers the inspector widgets.
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe void DescribeFields(nint classNameUtf8, delegate* unmanaged[Cdecl]<nint, byte, nint, void> sink)
    {
        if (ResolveType(Marshal.PtrToStringUTF8(classNameUtf8) ?? "") is not { } type
            || !type.IsAssignableTo(typeof(Script)))
            return;

        object? defaults = null;
        try { defaults = Activator.CreateInstance(type); } catch { /* leave defaults blank */ }

        foreach (var field in EditableFields(type))
        {
            var tag   = TagOf(field.FieldType)!.Value;
            var value = Format(tag, defaults is null ? null : field.GetValue(defaults));

            nint name = Marshal.StringToCoTaskMemUTF8(field.Name);
            nint val  = Marshal.StringToCoTaskMemUTF8(value);
            try { sink(name, (byte)tag, val); }
            finally { Marshal.FreeCoTaskMem(name); Marshal.FreeCoTaskMem(val); }
        }
    }

    // Tick an entity's script: instantiate it on first sight (by class name),
    // recreate it if the class changed or its assembly was unloaded, then drive
    // OnUpdate. One entry point keeps the native side trivial.
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    public static void Tick(uint entity, nint classNameUtf8, float dt)
    {
        var className = Marshal.PtrToStringUTF8(classNameUtf8);

        ref Script? script = ref CollectionsMarshal.GetValueRefOrAddDefault(Scripts, entity, out bool existed);
        if (!existed || script is null || script.GetType().FullName != className)
            script = Instantiate(className, entity);

        script?.OnUpdate(dt);
    }

    // Route a physics contact/sensor event to both involved scripts. `kind`
    // matches the native dispatcher (see PhysicsEvent).
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    public static void OnPhysicsEvent(uint a, uint b, byte kind, float normalX, float normalY)
    {
        var which  = (PhysicsEventKind)kind;
        var normal = new Vector2(normalX, normalY);   // points a -> b
        // Re-orient so each side's normal points AWAY from the other entity.
        Notify(a, b, which, -normal);
        Notify(b, a, which,  normal);
    }

    // Mirrors the native Scripting::PhysicsEventKind -- crosses the boundary as
    // a byte, so the underlying type AND order MUST stay in lockstep.
    private enum PhysicsEventKind : byte { CollisionEnter, CollisionExit, TriggerEnter, TriggerExit }

    private static void Notify(uint self, uint other, PhysicsEventKind kind, Vector2 normal)
    {
        if (!Scripts.TryGetValue(self, out var script) || script is null)
            return;

        var e = new Entity(other);
        switch (kind)
        {
            case PhysicsEventKind.CollisionEnter: script.OnCollisionEnter(new Collision(e, normal)); break;
            case PhysicsEventKind.CollisionExit:  script.OnCollisionExit(new Collision(e, normal));  break;
            case PhysicsEventKind.TriggerEnter:   script.OnTriggerEnter(e);   break;
            case PhysicsEventKind.TriggerExit:    script.OnTriggerExit(e);    break;
        }
    }

    // ── Hot reload ──────────────────────────────────────────────────────────

    private static void Reload()
    {
        if (_gamePath is null)
            return;

        Console.WriteLine("[C#] hot reload requested");

        // 1. Snapshot live instances: class name + transferable field values
        //    (anything that does NOT live in the assembly we're about to drop).
        var snapshots = new List<(uint entity, string className, Dictionary<string, object?> fields)>();
        foreach (var (entity, script) in Scripts)
        {
            if (script?.GetType().FullName is not { } className)
                continue;
            snapshots.Add((entity, className, SnapshotFields(script)));
        }

        // 2. Release every reference into the old assembly, then unload it.
        Scripts.Clear();
        var old = _game;
        _game = null;
        old?.Unload();
        for (var i = 0; i < 2; ++i)
        {
            GC.Collect();
            GC.WaitForPendingFinalizers();
        }

        // 3. Load the freshly built assembly. If it can't be read yet (the build
        //    is still writing), re-arm and try again next frame.
        if (!LoadGame())
        {
            _reloadPending = true;
            return;
        }

        // 4. Re-instantiate and restore the snapshot (no OnCreate -- we are
        //    resuming existing objects, not creating new ones).
        foreach (var (entity, className, fields) in snapshots)
        {
            var script = Create(className, entity);
            if (script is not null)
                RestoreFields(script, fields);
            Scripts[entity] = script;
        }

        Console.WriteLine($"[C#] hot reload complete ({snapshots.Count} script(s) restored)");
    }

    // Serializable fields (public / [SerializeField]) whose type lives OUTSIDE
    // the collectible game context -- those are safe to carry across an unload.
    // A field of a game-defined type is skipped (carrying it would pin the old
    // assembly and block the unload).
    private static Dictionary<string, object?> SnapshotFields(Script script)
    {
        var result = new Dictionary<string, object?>();
        foreach (var field in SerializableFields(script.GetType()))
        {
            if (!IsTransferable(field.FieldType))
                continue;

            var value = field.GetValue(script);
            if (value is not null && !IsTransferable(value.GetType()))
                continue;       // the runtime value itself would pin the old ALC

            result[field.Name] = value;
        }
        return result;
    }

    private static void RestoreFields(Script script, Dictionary<string, object?> fields)
    {
        var type = script.GetType();
        foreach (var (name, value) in fields)
        {
            if (type.GetField(name, BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance) is not { } field)
                continue;
            if (value is null)
            {
                if (!field.FieldType.IsValueType)
                    field.SetValue(script, null);
            }
            else if (field.FieldType.IsInstanceOfType(value))
            {
                field.SetValue(script, value);
            }
        }
    }

    private static bool IsTransferable(Type type)
        => AssemblyLoadContext.GetLoadContext(type.Assembly) is not GameLoadContext;

    // Read the assembly from a byte copy so the file on disk stays unlocked --
    // the project can rebuild Game.dll while we hold the previous bytes. Returns
    // false if the file isn't readable yet (mid-build).
    private static bool LoadGame()
    {
        if (_gamePath is null)
            return false;

        try
        {
            using var stream = new FileStream(_gamePath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
            var buffer = new byte[stream.Length];
            stream.ReadExactly(buffer);

            _game = new GameLoadContext();
            var assembly = _game.LoadFromStream(new MemoryStream(buffer));
            Console.WriteLine($"[C#] loaded game assembly '{assembly.GetName().Name}' (collectible)");
            return true;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[C#] could not load '{_gamePath}': {ex.Message}");
            _game = null;
            return false;
        }
    }

    private static void StartWatching()
    {
        if (_gamePath is null || _watcher is not null)
            return;

        _watcher = new FileSystemWatcher(Path.GetDirectoryName(_gamePath)!, Path.GetFileName(_gamePath))
        {
            NotifyFilter = NotifyFilters.LastWrite | NotifyFilters.Size | NotifyFilters.CreationTime,
            EnableRaisingEvents = true,
        };
        _watcher.Changed += (_, _) => _reloadPending = true;
        _watcher.Created += (_, _) => _reloadPending = true;
        _watcher.Renamed += (_, _) => _reloadPending = true;
        Console.WriteLine($"[C#] watching '{_gamePath}' for hot reload");
    }

    // ── Serializable fields ([SerializeField] / public) ──────────────────────

    // Type tag crossing to the editor -- MUST match native Scripting::ScriptFieldType.
    private enum FieldType : byte { Float, Int, Bool, String, Vector2 }

    // Public instance fields + [SerializeField] private ones (minus the Script
    // base's Entity handle). Used for BOTH the hot-reload snapshot and editor
    // exposure, so the two stay consistent.
    private static IEnumerable<FieldInfo> SerializableFields(Type type)
        => type.GetFields(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance)
               .Where(f => (f.IsPublic || f.IsDefined(typeof(SerializeFieldAttribute)))
                           && f.Name != nameof(Script.Entity));

    // Of those, the ones whose type the editor can render and store.
    private static IEnumerable<FieldInfo> EditableFields(Type type)
        => SerializableFields(type).Where(f => TagOf(f.FieldType) is not null);

    private static FieldType? TagOf(Type t) => t switch
    {
        _ when t == typeof(float)   => FieldType.Float,
        _ when t == typeof(int)     => FieldType.Int,
        _ when t == typeof(bool)    => FieldType.Bool,
        _ when t == typeof(string)  => FieldType.String,
        _ when t == typeof(Vector2) => FieldType.Vector2,
        _ => null,
    };

    private static string Format(FieldType tag, object? value) => tag switch
    {
        FieldType.Float   => ((float)(value ?? 0.0f)).ToString(CultureInfo.InvariantCulture),
        FieldType.Int     => ((int)(value ?? 0)).ToString(CultureInfo.InvariantCulture),
        FieldType.Bool    => value is true ? "true" : "false",
        FieldType.String  => value as string ?? "",
        FieldType.Vector2 => value is Vector2 v
            ? $"{v.X.ToString(CultureInfo.InvariantCulture)} {v.Y.ToString(CultureInfo.InvariantCulture)}"
            : "0 0",
        _ => "",
    };

    private static object? Parse(FieldType tag, string s) => tag switch
    {
        FieldType.Float   => float.TryParse(s, NumberStyles.Float, CultureInfo.InvariantCulture, out var f) ? f : 0.0f,
        FieldType.Int     => int.TryParse(s, NumberStyles.Integer, CultureInfo.InvariantCulture, out var i) ? i : 0,
        FieldType.Bool    => s is "true" or "1" or "True",
        FieldType.String  => s,
        FieldType.Vector2 => ParseVector2(s),
        _ => null,
    };

    private static Vector2 ParseVector2(string s)
    {
        var p = s.Split([' ', ','], StringSplitOptions.RemoveEmptyEntries);
        float At(int i) => p.Length > i && float.TryParse(p[i], NumberStyles.Float, CultureInfo.InvariantCulture, out var v) ? v : 0.0f;
        return new Vector2(At(0), At(1));
    }

    // Apply the editor-authored overrides (read from the native ScriptComponent)
    // onto a fresh instance, before OnCreate. A field with no override keeps its
    // code default.
    private static void ApplyAuthoredFields(Script script, uint entity)
    {
        foreach (var field in EditableFields(script.GetType()))
        {
            if (Interop.GetScriptField(entity, field.Name) is not { } raw)
                continue;
            if (Parse(TagOf(field.FieldType)!.Value, raw) is { } value)
                field.SetValue(script, value);
        }
    }

    // ── Instantiation ───────────────────────────────────────────────────────

    private static Script? Instantiate(string? className, uint entity)
    {
        var script = Create(className, entity);
        if (script is null)
            return null;

        script.OnCreate();
        Console.WriteLine($"[C#] created '{className}' on entity {entity}");
        return script;
    }

    // Build a bound-but-uninitialized instance (no OnCreate). The reload path
    // uses this directly so resumed objects aren't re-created.
    private static Script? Create(string? className, uint entity)
    {
        if (string.IsNullOrEmpty(className))
            return null;

        if (ResolveType(className) is not { } type || !type.IsAssignableTo(typeof(Script)))
        {
            Console.WriteLine($"[C#] '{className}' not found or not a Script");
            return null;
        }

        var script = (Script)Activator.CreateInstance(type)!;
        script.Entity = new Entity(entity);
        ApplyAuthoredFields(script, entity);
        return script;
    }

    // Resolve a type by full name. The collectible game context is searched
    // first (so a fresh reload wins over an assembly still being unloaded),
    // then every other loaded assembly.
    private static Type? ResolveType(string name)
    {
        if (_game is not null)
            foreach (var assembly in _game.Assemblies)
                if (assembly.GetType(name) is { } type)
                    return type;

        return AppDomain.CurrentDomain.GetAssemblies()
            .Select(assembly => assembly.GetType(name))
            .FirstOrDefault(type => type is not null)
            ?? Type.GetType(name);
    }

    // The project's game assembly lives here so it can be unloaded and rebuilt.
    // Engine API references resolve to the already-loaded host copy; everything
    // else (the BCL) falls through to the default context.
    private sealed class GameLoadContext() : AssemblyLoadContext(isCollectible: true)
    {
        protected override Assembly? Load(AssemblyName name)
            => name.Name == "OmegaEngine" ? typeof(Bootstrap).Assembly : null;
    }
}