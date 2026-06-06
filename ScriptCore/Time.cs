namespace OmegaEngine;

// Frame timing, published by the engine each update.
public static class Time
{
    public static float Delta   => Interop.TimeDelta();     // seconds since the last update
    public static float Elapsed => Interop.TimeElapsed();   // seconds since the host booted
}