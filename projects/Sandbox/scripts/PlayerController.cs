using System.Numerics;
using OmegaEngine;

namespace Game;

// Side-scroller player. A/D or arrows move; Space/W/Up jump. Faces the movement
// direction and swaps between idle and jump textures by state.
public sealed class PlayerController : Script
{
    public float  MoveSpeed   = 4.0f;
    public float  JumpSpeed   = 7.0f;
    public float  CoyoteTime  = 0.1f;     // grounded grace after leaving the floor
    public string IdleTexture = "assets/textures/Idle.png";
    public string JumpTexture = "assets/textures/Jump.png";

    // Count of active floor (up-facing) contacts. The ground tilemap is ONE body
    // with many fixtures, so keying floors by the OTHER entity id would collapse
    // them -- count contacts instead. (Exits carry no normal, so we decrement on
    // any exit; fine where every contact is a floor.)
    private int   _floorContacts;
    // Grounded grace timer: refreshed while touching, counts down after. Smooths
    // brief contact jitter (landing settle) AND gives a small post-ledge jump
    // window -- so the idle/jump texture never flickers and jumps feel forgiving.
    private float _coyote;
    private bool  _facingRight = true;
    private bool  _wasGrounded = true;
    private bool  _jumpHeld;

    private bool Grounded => _coyote > 0.0f;

    public override void OnUpdate(float dt)
    {
        _coyote = _floorContacts > 0 ? CoyoteTime : MathF.Max(0.0f, _coyote - dt);

        // Horizontal: drive X velocity from input, leave gravity to own Y.
        var dir = 0.0f;
        if (Input.IsKeyDown(Key.A) || Input.IsKeyDown(Key.Left))  dir -= 1.0f;
        if (Input.IsKeyDown(Key.D) || Input.IsKeyDown(Key.Right)) dir += 1.0f;

        var velocity = Entity.Velocity;
        velocity.X = dir * MoveSpeed;

        // Edge-detect the jump from the LEVEL key state. Scripts tick on the
        // fixed timestep, but Input.WasKeyPressed is a RENDER-frame edge -- it's
        // missed on frames that run no fixed tick. Sampling IsKeyDown each tick
        // and finding the rising edge ourselves is reliable for fixed-step play.
        var jumpHeld = Input.IsKeyDown(Key.Space) || Input.IsKeyDown(Key.W) || Input.IsKeyDown(Key.Up);
        if (Grounded && jumpHeld && !_jumpHeld)
        {
            velocity.Y = JumpSpeed;
            _coyote    = 0.0f;     // consume the grounded grace so we can't double-jump
            Audio.Play("assets/audio/jump.wav");
            Particles.Burst(Entity.Position + new Vector2(0f, -0.15f), 8,
                            new Vector4(0.75f, 0.70f, 0.60f, 0.9f),
                            speed: 1.2f, lifetime: 0.35f, size: 0.07f,
                            texture: "assets/textures/particle.png");
        }
        _jumpHeld = jumpHeld;

        Entity.Velocity = velocity;

        // Face the way we're moving.
        if (dir > 0.0f && !_facingRight) { _facingRight = true;  Entity.SetFlipX(false); }
        if (dir < 0.0f &&  _facingRight) { _facingRight = false; Entity.SetFlipX(true); }

        // Idle on the ground, jump pose in the air.
        if (Grounded != _wasGrounded)
        {
            _wasGrounded = Grounded;
            Entity.SetTexture(Grounded ? IdleTexture : JumpTexture);
        }
    }

    // A contact whose normal points up is a floor we're standing on.
    public override void OnCollisionEnter(Collision c)
    {
        if (c.Normal.Y > 0.5f)
            ++_floorContacts;
    }

    public override void OnCollisionExit(Collision _)
    {
        if (_floorContacts > 0)
            --_floorContacts;
    }
}