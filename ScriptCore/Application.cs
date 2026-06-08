namespace OmegaEngine;

// The host application. For now just a clean shutdown (e.g. a menu's Exit).
public static class Application
{
    public static void Quit() => Interop.Quit();
}