using System.Numerics;
using OmegaEngine;

namespace Game;

// Drives a vertical menu. W/Up and S/Down move the selector between the option
// entities (found by name); D / Enter / Space activates the current one. The
// selector is a separate entity parked just left of the chosen option.
//
//   Level 1  -> NEW GAME: clears the checkpoint save, loads `Level` fresh.
//   Continue -> loads the scene recorded in the save (the checkpoint script
//               then places the player); with no save it just starts fresh.
//   Exit     -> quits.
public sealed class MenuController : Script
{
    public string Option0  = "Opt_Level1";    // new game
    public string Option1  = "Opt_Continue";  // resume from the save
    public string Option2  = "Opt_Exit";      // quit
    public string Selector = "Selector";       // the '>' marker entity
    public string Level    = "Level1";         // scene loaded by a new game
    public float  Gap      = 0.7f;             // selector distance left of an option

    private Entity[] _options = [];
    private int  _index;
    private bool _upHeld, _downHeld, _selectHeld;

    public override void OnCreate()
    {
        // Re-frame the view on the menu (the camera may have followed the
        // player in a previous scene).
        Camera.Position = Vector2.Zero;

        _options = [Entity.Find(Option0), Entity.Find(Option1), Entity.Find(Option2)];
        PlaceSelector();
    }

    public override void OnUpdate(float dt)
    {
        var up     = Input.IsKeyDown(Key.W) || Input.IsKeyDown(Key.Up);
        var down   = Input.IsKeyDown(Key.S) || Input.IsKeyDown(Key.Down);
        var select = Input.IsKeyDown(Key.D) || Input.IsKeyDown(Key.Enter) || Input.IsKeyDown(Key.Space);

        if (up     && !_upHeld)     Step(-1);
        if (down   && !_downHeld)   Step(+1);
        if (select && !_selectHeld) Activate();

        _upHeld = up; _downHeld = down; _selectHeld = select;
    }

    private void Step(int dir)
    {
        var n = _options.Length;
        if (n == 0) return;
        _index = (_index + dir + n) % n;
        PlaceSelector();
    }

    private void PlaceSelector()
    {
        if (_index >= _options.Length || !_options[_index].IsValid)
            return;

        var marker = Entity.Find(Selector);
        if (!marker.IsValid)
            return;

        var p = _options[_index].Position;
        marker.Position = new Vector2(p.X - Gap, p.Y);
    }

    private void Activate()
    {
        switch (_index)
        {
            case 0:   // new game: forget the old run, start at the beginning
                Save.Delete("checkpoint.scene");
                Save.Delete("checkpoint.tag");
                Scene.Load(Level);
                break;

            case 1:   // continue: resume the saved scene (fresh run if none)
                Scene.Load(Save.GetString("checkpoint.scene", Level));
                break;

            case 2:
                Application.Quit();
                break;
        }
    }
}