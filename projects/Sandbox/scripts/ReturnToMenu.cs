using OmegaEngine;

namespace Game;

// Press Escape during gameplay to go back to the menu scene.
public sealed class ReturnToMenu : Script
{
    public string Menu = "Menu";

    private bool _escHeld;

    public override void OnUpdate(float dt)
    {
        var esc = Input.IsKeyDown(Key.Escape);
        if (esc && !_escHeld)
            Scene.Load(Menu);
        _escHeld = esc;
    }
}