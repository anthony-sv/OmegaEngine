using System.Numerics;
using OmegaEngine;

namespace Game;

// The RC213V: a REAL vehicle -- chassis + two wheels on sprung wheel
// joints. The rear wheel is driven by its joint motor, brakes are wheel
// motors held at speed 0, and grip is plain friction -- so wheelspin,
// wheelies, stoppies and fork dive all EMERGE from the simulation instead
// of being scripted torques.
//
//   W / Up      throttle (rear wheel motor)
//   E / Q       shift up / down (six gears: torque vs top speed)
//   Space       FRONT brake -- strong, the forks dive, the rear lifts
//   S / Down    rear brake
//   A / D       lean back / forward (body english on the chassis)
//   R           restart        Escape   back to the menu
public sealed class Bike : Script
{
    // ── Engine (a real tach, not a "fraction of top speed" bar) ──
    // Revs idle to redline; the LIMITER sits above redline, so you can over-
    // rev into the red and bounce off the cut -- shift before that.
    private const float IdleRpm    = 3000f;
    private const float RedlineRpm = 16000f;   // the red zone starts here
    private const float LimiterRpm = 18000f;   // hard rev cut (no torque past it)

    // Per-gear WHEEL speed (sim m/s) at the limiter -- i.e. the top speed of
    // each gear. Box2D caps how fast a body can spin, so a rolling wheel of
    // radius 0.3 tops out near 14.5 m/s; EVERY gear's top stays under that, so
    // the engine reaches the rev cut (== redline) in each. ~1.3x steps so an
    // upshift drops the needle to mid-band.
    private static readonly float[] GearTop = [3.5f, 5.5f, 7.5f, 9.5f, 11.5f, 13.5f];

    // Peak wheel torque per gear (N*m). Low gears pull hardest (wheelies on a
    // hard launch); the auto-level assist (below) stops the bike getting STUCK
    // balancing a wheelie, so throttle pulls through to redline in every gear.
    private static readonly float[] PeakTorque = [6.0f, 5.5f, 5.0f, 4.5f, 4.0f, 3.6f];

    // The world is drawn compressed so the bike stays on camera; the speedo
    // reports the equivalent TRUE road speed (1 world unit ~ RoadScale metres).
    // The physical top is ~13.5 m/s -- this scales it to a MotoGP-like readout.
    private const float RoadScale = 6.0f;

    private static readonly Vector4 AmberHud = new(1.0f, 0.85f, 0.30f, 1.0f);
    private static readonly Vector4 RedHud   = new(1.0f, 0.25f, 0.18f, 1.0f);

    public float LeanTorque      = 10.0f;  // body english (A/D), scaled for the heavier chassis
    public float LevelAssist     = 7.0f;   // gentle self-righting when neither lean key is held
    public float FrontBrakeForce = 70f;    // chassis decel force (front, strong)
    public float RearBrakeForce  = 40f;    // chassis decel force (rear, milder, stable)
    public float FrontPitch      = 80f;    // nose-down torque under hard front braking (stoppie)
    public float WheelRadius     = 0.3f;   // MUST match the wheel colliders in the scene

    private Entity _rear, _front;
    private int    _gear;
    private float  _engineRpm = IdleRpm;
    private bool   _shiftUpHeld, _shiftDownHeld, _frontBrakeHeld, _redlineHud;
    private Vector2 _start, _rearOffset, _frontOffset;
    private float  _grace;       // ignore the crash check while settling onto the wheels

    // Engine torque vs rpm, normalised 0..1: soft off idle (a launch grips
    // instead of bogging into wheelspin), peak in the mid-high range, ZERO
    // past the limiter -- which is what caps each gear's top speed.
    private static float EngineTorqueFrac(float rpm)
    {
        if (rpm >= LimiterRpm) return 0f;
        float x = rpm / RedlineRpm;                      // 0 .. ~1.1
        return Math.Clamp(0.35f + 1.6f * x - 0.95f * x * x, 0f, 1f);
    }

    public override void OnCreate()
    {
        _rear  = Entity.Find("RearWheel");
        _front = Entity.Find("FrontWheel");
        _start = Entity.Position;
        _rearOffset  = _rear.Position - _start;
        _frontOffset = _front.Position - _start;
        _grace = 0.8f;

        Race.Reset();
        Audio.PlayMusic("assets/audio/engine_loop.wav");
        Audio.SetMusicVolume(0.30f);
        Audio.SetMusicPitch(0.65f);
    }

    public override void OnUpdate(float dt)
    {
        if (Input.IsKeyDown(Key.Escape)) { Audio.StopMusic(); Scene.Load("Menu"); return; }
        if (Input.IsKeyDown(Key.R))      { Restart(); return; }

        float speed = MathF.Max(0f, Entity.Velocity.X);

        // Engine revs from the REAR WHEEL, not the bike: rolling right is a
        // negative angular velocity, so negate. revFrac is where the wheel
        // sits in this gear's band (0 = stopped, 1 = the limiter). On wheelspin
        // the wheel outruns the bike, so the revs flare -- as they should.
        float wheelSurface = -_rear.AngularVelocity * (MathF.PI / 180f) * WheelRadius;   // sim m/s
        float revFrac      = wheelSurface / GearTop[_gear];
        _engineRpm = Math.Clamp(MathF.Max(IdleRpm, revFrac * LimiterRpm), IdleRpm, LimiterRpm);

        // -- gears --
        bool up   = Input.IsKeyDown(Key.E);
        bool down = Input.IsKeyDown(Key.Q);
        if (up && !_shiftUpHeld && _gear < GearTop.Length - 1)
        {
            _gear++;
            Audio.Play("assets/audio/shift.wav");
            ExhaustPuff();
        }
        if (down && !_shiftDownHeld && _gear > 0)
        {
            _gear--;
            Audio.Play("assets/audio/shift.wav");
        }
        _shiftUpHeld = up; _shiftDownHeld = down;

        // -- throttle: drive by TORQUE off the engine's curve (not a target
        //    speed), so the bike keeps pulling until the rev limiter cuts it
        //    at the gear's top -- the tach reaches redline in every gear,
        //    uphill or down. Off throttle = freewheel coast.
        bool throttle   = Input.IsKeyDown(Key.W) || Input.IsKeyDown(Key.Up);
        bool rearBrake  = Input.IsKeyDown(Key.S) || Input.IsKeyDown(Key.Down);
        bool frontBrake = Input.IsKeyDown(Key.Space);

        if (throttle && !Race.Finished)
        {
            if (!Race.Started) Race.Started = true;

            if (revFrac < 1f)
            {
                _rear.SetMotorSpeed(-100000f);   // "faster" -- torque is the real limit
                _rear.SetMotorTorque(EngineTorqueFrac(_engineRpm) * PeakTorque[_gear]);
            }
            else
            {
                _rear.SetMotorTorque(0f);        // bounce off the limiter
            }

            // Wheelspin smoke: the contact patch sliding against the road.
            if (wheelSurface - speed > 2.5f)
                Particles.Burst(_rear.Position + new Vector2(-0.15f, -0.25f), 2,
                                new Vector4(0.9f, 0.9f, 0.9f, 0.6f),
                                speed: 0.8f, lifetime: 0.5f, size: 0.12f,
                                texture: "assets/textures/particle.png");
        }
        else
        {
            _rear.SetMotorTorque(0f);            // freewheel
        }

        // -- brakes: a decelerating FORCE on the chassis, not a wheel lock --
        // the wheels keep rolling (so the revs fall smoothly, no snap to idle)
        // and the force eases to nothing at a crawl, so a tap can't stop you
        // dead. The front is stronger and adds a little nose-down pitch -- a
        // hard grab AT SPEED can stoppie, but it's no longer automatic.
        float brakeEase = MathF.Min(1f, speed / 6f);     // 0 at a stop -> 1 by 6 m/s
        if (rearBrake && speed > 0.2f)
            Entity.ApplyForce(new Vector2(-RearBrakeForce * brakeEase, 0f));
        if (frontBrake && speed > 0.2f)
        {
            Entity.ApplyForce(new Vector2(-FrontBrakeForce * brakeEase, 0f));
            Entity.ApplyTorque(-FrontPitch * MathF.Min(1f, speed / 12f));
            if (!_frontBrakeHeld && speed > 8f)
                Audio.Play("assets/audio/brake.wav");
        }
        _frontBrakeHeld = frontBrake;

        // -- lean + auto-level: A/D are body english (A lifts/holds a wheelie,
        //    D drops the nose). With NEITHER held, a gentle assist pulls the
        //    bike toward level so it never gets stuck balancing a power wheelie.
        //    The assist is SUPPRESSED while front-braking, otherwise it fights
        //    the nose-down dive and a stoppie is impossible.
        bool leanA = Input.IsKeyDown(Key.A), leanD = Input.IsKeyDown(Key.D);
        if (leanA) Entity.ApplyTorque(+LeanTorque);
        if (leanD) Entity.ApplyTorque(-LeanTorque);
        if (!leanA && !leanD && !frontBrake)
        {
            float lean = NormalizeAngle(Entity.Rotation);
            Entity.ApplyTorque(-Math.Clamp(lean / 15f, -1f, 1f) * LevelAssist);
        }

        // -- crash / fell off the world --
        // The suspension settles for a moment after a spawn; don't read that
        // rocking as a crash.
        _grace = MathF.Max(0f, _grace - dt);
        float tilt = NormalizeAngle(Entity.Rotation);
        if (_grace <= 0f && MathF.Abs(tilt) > 80f)
        {
            Audio.Play("assets/audio/crash.wav");
            Particles.Burst(Entity.Position, 18, new Vector4(0.95f, 0.55f, 0.15f, 1f),
                            speed: 3.0f, lifetime: 0.6f, size: 0.1f,
                            texture: "assets/textures/particle.png");
            Restart();
            return;
        }
        if (Entity.Position.Y < -10f) { Restart(); return; }

        // -- timer + engine voice + camera + HUD --
        if (Race.Started && !Race.Finished)
            Race.Time += dt;

        float revNorm = _engineRpm / LimiterRpm;            // 0..1 of the tach
        Audio.SetMusicPitch(0.55f + 1.25f * revNorm);       // idle drone -> redline howl
        Audio.SetMusicVolume(throttle ? 0.38f : 0.28f);

        Camera.Position = new Vector2(Entity.Position.X + 2.5f, Entity.Position.Y + 1.0f);

        // Speedo = true road speed (the compressed world, scaled back up).
        int kmh = (int)(wheelSurface * RoadScale * 3.6f);

        var hud = Entity.Find("HudText");
        if (hud.IsValid)
        {
            hud.Position = Camera.Position + new Vector2(0f, 2.4f);
            hud.SetText($"GEAR {_gear + 1}   {kmh,3} km/h   {Race.Time,6:0.00}s");
        }
        var rpmHud = Entity.Find("HudRpm");
        if (rpmHud.IsValid)
        {
            // 18-cell tach; the bar turns RED once the needle is in the redline
            // zone (>= RedlineRpm) and a SHIFT cue lights -- shift before the
            // limiter. The cue field is a constant 10 wide so the bar holds still.
            int    filled = (int)(revNorm * 18f);
            string bar    = new string('|', filled).PadRight(18, '.');
            bool   red    = _engineRpm >= RedlineRpm;
            string cue    = (red && _gear < 5) ? "SHIFT! (E)" : "          ";
            rpmHud.Position = Camera.Position + new Vector2(0f, 2.0f);
            rpmHud.SetText($"{_engineRpm,6:N0} rpm [{bar}]  {cue}");
            if (red != _redlineHud)             // push colour only on the transition
            {
                _redlineHud = red;
                rpmHud.SetTextColor(red ? RedHud : AmberHud);
            }
        }
    }

    public override void OnDestroy()
    {
        Audio.StopMusic();   // the engine loop dies with the scene / on Stop
    }

    private void Restart()
    {
        // Place all three bodies back at the grid, dead and level. The
        // chassis rotation must be zeroed BEFORE the teleports (Teleport
        // syncs each body to its Transform's angle).
        Entity.Rotation = 0f;
        _rear.Rotation  = 0f;
        _front.Rotation = 0f;
        Entity.Teleport(_start);
        _rear.Teleport(_start + _rearOffset);
        _front.Teleport(_start + _frontOffset);
        Entity.Velocity        = Vector2.Zero;
        _rear.Velocity         = Vector2.Zero;
        _front.Velocity        = Vector2.Zero;
        Entity.AngularVelocity = 0f;
        _rear.AngularVelocity  = 0f;
        _front.AngularVelocity = 0f;
        _rear.SetMotorTorque(0f);
        _front.SetMotorTorque(0f);
        _gear  = 0;
        _grace = 0.8f;
        Race.Reset();
    }

    private void ExhaustPuff()
    {
        Particles.Burst(Entity.Position + new Vector2(-0.95f, -0.1f), 4,
                        new Vector4(0.45f, 0.45f, 0.5f, 0.7f),
                        speed: 0.6f, lifetime: 0.45f, size: 0.1f,
                        texture: "assets/textures/particle.png");
    }

    private static float NormalizeAngle(float degrees)
    {
        degrees %= 360f;
        if (degrees > 180f)  degrees -= 360f;
        if (degrees < -180f) degrees += 360f;
        return degrees;
    }
}