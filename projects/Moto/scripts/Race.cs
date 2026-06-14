namespace Game;

// Shared race state. Scripts in one assembly see the same statics, so this
// is the simplest cross-script blackboard: the bike drives it, the HUD reads
// it, the finish line closes it.
public static class Race
{
    public static float Time;        // seconds since launch (0 until moving)
    public static bool  Started;     // first throttle input seen
    public static bool  Finished;

    public static void Reset()
    {
        Time     = 0f;
        Started  = false;
        Finished = false;
    }
}