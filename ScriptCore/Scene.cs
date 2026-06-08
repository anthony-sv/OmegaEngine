namespace OmegaEngine;

// Scene control. Switching is applied at the next frame boundary, so it's safe
// to call from inside a script's OnUpdate / callbacks.
public static class Scene
{
    public static void Load(string name) => Interop.LoadScene(name);
}