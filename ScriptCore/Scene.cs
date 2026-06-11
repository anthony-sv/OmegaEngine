namespace OmegaEngine;

// Scene control. Switching is applied at the next frame boundary, so it's safe
// to call from inside a script's OnUpdate / callbacks.
public static class Scene
{
    public static void Load(string name) => Interop.LoadScene(name);

    // The active scene's name -- e.g. recorded into save data so a Continue
    // option knows which scene to load.
    public static string Current => Interop.SceneName();
}