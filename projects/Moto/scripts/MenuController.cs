using System.Numerics;
using OmegaEngine;

namespace Game;

// The Moto menu: Race starts a run, Exit quits. W/S move, D/Enter/Space
// select. Shows the saved best time under the title.
public sealed class MenuController : Script
{
    public string Option0  = "Opt_Race";
    public string Option1  = "Opt_Exit";
    public string Selector = "Selector";
    public string BestText = "BestTime";
    public float  Gap      = 0.7f;

    private Entity[] _options = [];
    private int  _index;
    private bool _upHeld, _downHeld, _selectHeld;

    public override void OnCreate()
    {
        Camera.Position = Vector2.Zero;
        _options = [Entity.Find(Option0), Entity.Find(Option1)];
        PlaceSelector();

        var best = Entity.Find(BestText);
        if (best.IsValid)
            best.SetText(Save.Has("besttime")
                ? $"best run  {Save.GetFloat("besttime"):0.00}s"
                : "no run yet");
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
        Audio.Play("assets/audio/select.wav");
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
            case 0: Scene.Load("Track");  break;
            case 1: Application.Quit();   break;
        }
    }
}