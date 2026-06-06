using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Runtime.Loader;

namespace OmegaEngine;

// The native ScriptHost calls these [UnmanagedCallersOnly] entry points.
// Instances are keyed by entity id, so the native ScriptComponent carries no
// runtime handle -- it stays pure (serializable) data.
public static class Bootstrap
{
    private static readonly Dictionary<uint, Script?> Scripts = [];

    // Called once with a pointer to the engine's function table.
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe void Init(nint apiPtr)
    {
        Interop.Api = *(NativeApi*)apiPtr;
        Console.WriteLine($"[C#] OmegaEngine scripting initialized (.NET {Environment.Version})");
    }

    // Load the project's game assembly (e.g. Game.dll) into the default
    // context so its Script subclasses become discoverable by name. Its
    // reference to OmegaEngine resolves to this already-loaded assembly.
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    public static void LoadAssembly(nint pathUtf8)
    {
        if (Marshal.PtrToStringUTF8(pathUtf8) is not { Length: > 0 } path)
            return;

        try
        {
            // Load into the SAME context this engine API lives in -- the host
            // loads us into an isolated context, so the default context would
            // pull a SECOND OmegaEngine and the game's `Script` type wouldn't
            // match ours (`is Script` would fail).
            AssemblyLoadContext context =
                AssemblyLoadContext.GetLoadContext(typeof(Bootstrap).Assembly) ?? AssemblyLoadContext.Default;

            var assembly = context.LoadFromAssemblyPath(path);
            Console.WriteLine($"[C#] loaded game assembly '{assembly.GetName().Name}'");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[C#] failed to load '{path}': {ex.Message}");
        }
    }

    // Tick an entity's script: instantiate it on first sight (by class name),
    // then drive OnUpdate. One entry point keeps the native side trivial.
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    public static void Tick(uint entity, nint classNameUtf8, float dt)
    {
        ref Script? script = ref CollectionsMarshal.GetValueRefOrAddDefault(Scripts, entity, out bool existed);
        if (!existed)
            script = Instantiate(Marshal.PtrToStringUTF8(classNameUtf8), entity);

        script?.OnUpdate(dt);
    }

    private static Script? Instantiate(string? className, uint entity)
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
        script.OnCreate();

        Console.WriteLine($"[C#] created '{className}' on entity {entity}");
        return script;
    }

    // Resolve a type by its full name across every loaded assembly (so game
    // scripts in any loaded assembly are found).
    private static Type? ResolveType(string name) =>
        AppDomain.CurrentDomain.GetAssemblies()
            .Select(assembly => assembly.GetType(name))
            .FirstOrDefault(type => type is not null)
        ?? Type.GetType(name);
}